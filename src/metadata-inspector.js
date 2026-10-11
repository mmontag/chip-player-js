/**
 * Extracts live DOM metadata and structured data for in-browser inspection (Shift+M).
 */
export function getDomMetadataText() {
  const getMeta = (selector) => {
    const el = document.querySelector(selector);
    return el ? (el.getAttribute('content') || el.textContent) : null;
  };
  const getLink = (rel) => {
    const el = document.querySelector(`link[rel="${rel}"]`);
    return el ? el.getAttribute('href') : null;
  };

  const lines = [
    `--- Metadata (Shift+M) ---`,
    `URL: ${window.location.href}`,
    `Title: ${document.title}`,
    `Description: ${getMeta('meta[name="description"]') || '(none)'}`,
    `Canonical: ${getLink('canonical') || '(none)'}`,
    ``,
    `--- Open Graph ---`,
    `og:title: ${getMeta('meta[property="og:title"]') || '(none)'}`,
    `og:description: ${getMeta('meta[property="og:description"]') || '(none)'}`,
    `og:image: ${getMeta('meta[property="og:image"]') || '(none)'}`,
    `og:url: ${getMeta('meta[property="og:url"]') || '(none)'}`,
    `og:type: ${getMeta('meta[property="og:type"]') || '(none)'}`,
    ``,
    `--- Twitter Card ---`,
    `twitter:card: ${getMeta('meta[property="twitter:card"]') || '(none)'}`,
    `twitter:title: ${getMeta('meta[property="twitter:title"]') || '(none)'}`,
    `twitter:description: ${getMeta('meta[property="twitter:description"]') || '(none)'}`,
    `twitter:image: ${getMeta('meta[property="twitter:image"]') || '(none)'}`,
  ];

  const jsonLdScripts = document.querySelectorAll('script[type="application/ld+json"]');
  if (jsonLdScripts.length > 0) {
    lines.push(``, `--- Schema.org (JSON-LD) ---`);
    jsonLdScripts.forEach((scriptEl) => {
      try {
        const parsed = JSON.parse(scriptEl.textContent);
        lines.push(JSON.stringify(parsed, null, 2));
      } catch (e) {
        lines.push(scriptEl.textContent);
      }
    });
  }

  if (window.__chipConfig) {
    lines.push(``, `--- window.__chipConfig ---`, JSON.stringify(window.__chipConfig, null, 2));
  }

  return lines.join('\n');
}
