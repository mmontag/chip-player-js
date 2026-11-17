const fs = require('fs').promises;
const path = require('path');
const axios = require('axios');
const { createCanvas, loadImage } = require('@napi-rs/canvas');

const JPEG_QUALITY = 85;
const LOGO_SIZE = 280;
const LOGO_OVERLAP = 36;
const LOGO_Y = 180;
const BORDER_WIDTH = 5;

async function resolveAssetPath(clientBuildRoot, publicDir, filename) {
  const buildPath = path.join(clientBuildRoot, filename);
  try {
    await fs.access(buildPath);
    return buildPath;
  } catch {
    return path.join(publicDir, filename);
  }
}

async function loadImageSource(imageUrl) {
  if (imageUrl.startsWith('http://') || imageUrl.startsWith('https://')) {
    const response = await axios.get(imageUrl, {
      responseType: 'arraybuffer',
      timeout: 5000,
    });
    return loadImage(Buffer.from(response.data));
  }
  const fileBuffer = await fs.readFile(imageUrl);
  return loadImage(fileBuffer);
}

/**
 * Creates a reusable OpenGraph preview renderer backed by @napi-rs/canvas.
 * Pre-bakes the tinted background, scaled logo, and fallback buffer once at startup.
 */
function createPreviewRenderer({
  clientBuildRoot,
  publicDir,
  chipPlayerPng = 'chip-player-1200x640.png',
  logoPng = 'chip-player-logo.png',
  jpegQuality = JPEG_QUALITY,
}) {
  let initPromise = null;

  async function initAssets() {
    const [bgPath, logoPath] = await Promise.all([
      resolveAssetPath(clientBuildRoot, publicDir, chipPlayerPng),
      resolveAssetPath(clientBuildRoot, publicDir, logoPng),
    ]);
    const [bg, logo] = await Promise.all([loadImage(bgPath), loadImage(logoPath)]);

    const width = bg.width;
    const height = bg.height;

    const defaultCanvas = createCanvas(width, height);
    defaultCanvas.getContext('2d').drawImage(bg, 0, 0);
    const defaultBuffer = defaultCanvas.toBuffer('image/jpeg', jpegQuality);

    const tintedBgCanvas = createCanvas(width, height);
    const tintedCtx = tintedBgCanvas.getContext('2d');
    tintedCtx.drawImage(bg, 0, 0);
    tintedCtx.fillStyle = '#00008080';
    tintedCtx.fillRect(0, 0, width, height);

    const logoCanvas = createCanvas(LOGO_SIZE, LOGO_SIZE);
    logoCanvas.getContext('2d').drawImage(logo, 0, 0, LOGO_SIZE, LOGO_SIZE);

    const workCanvas = createCanvas(width, height);
    const workCtx = workCanvas.getContext('2d');

    return { width, height, defaultBuffer, tintedBgCanvas, logoCanvas, workCanvas, workCtx };
  }

  function getAssets() {
    if (!initPromise) {
      initPromise = initAssets().catch((err) => {
        initPromise = null;
        throw err;
      });
    }
    return initPromise;
  }

  /**
   * Renders a 1200x640 JPEG preview buffer for the given image path or URL.
   * Falls back to the pre-baked default preview if imageUrl is missing or fails to load.
   */
  async function renderPreview(imageUrl) {
    const assets = await getAssets();
    if (!imageUrl) {
      return { buffer: assets.defaultBuffer, isDefault: true };
    }

    let songImage;
    try {
      songImage = await loadImageSource(imageUrl);
    } catch (e) {
      console.error('Error loading song image:', e);
      return { buffer: assets.defaultBuffer, isDefault: true };
    }

    if (!songImage.width || !songImage.height) {
      return { buffer: assets.defaultBuffer, isDefault: true };
    }

    const { width, height, tintedBgCanvas, logoCanvas, workCanvas, workCtx } = assets;
    const scale = Math.min((width * 0.8) / songImage.width, (height * 0.8) / songImage.height);
    const drawWidth = songImage.width * scale;
    const drawHeight = songImage.height * scale;
    const offset = LOGO_SIZE - LOGO_OVERLAP;
    const x = (width - drawWidth - offset) / 2;
    const y = (height - drawHeight) / 2;

    // Synchronous draw + encode on workCanvas so concurrent requests never interleave.
    workCtx.drawImage(tintedBgCanvas, 0, 0);
    workCtx.fillStyle = '#ffffff';
    workCtx.shadowColor = '#000040';
    workCtx.shadowBlur = 60;
    workCtx.shadowOffsetY = 15;
    workCtx.fillRect(
      x - BORDER_WIDTH + offset,
      y - BORDER_WIDTH,
      drawWidth + BORDER_WIDTH * 2,
      drawHeight + BORDER_WIDTH * 2
    );
    workCtx.shadowColor = 'transparent';

    workCtx.drawImage(songImage, x + offset, y, drawWidth, drawHeight);
    workCtx.drawImage(logoCanvas, x, LOGO_Y);

    return {
      buffer: workCanvas.toBuffer('image/jpeg', jpegQuality),
      isDefault: false,
    };
  }

  return { renderPreview };
}

module.exports = { createPreviewRenderer };
