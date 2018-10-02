const fs = require('fs');
const glob = require('glob');
const chalk = require('chalk');
const {spawnSync} = require('child_process');

const genesisRoot = '../../Music/Video Game Music/Sega Genesis/';
const vgmComp = '../vgmtools/vgm_cmp';
const dirs = glob.sync(genesisRoot + '*/');

dirs.forEach(dir => {
  optimizeVgmToVgz(dir);
});

function optimizeVgmToVgz(path) {
  let files = glob.sync(`${path}*.vgm`);
  if (files.length === 0) return;

  let fileSize = 0;
  files.forEach(file => {
    fileSize += fs.lstatSync(file).size;
  });

  if ((fileSize / files.length) < 10240) {
    console.log(chalk.gray('Average VGM file size is less than 10 KB. Skipping optimization.'));
    return;
  }

  console.log(chalk.gray('Optimizing %d VGM files (%s MB on disk)...'), files.length, (fileSize / 1048576).toFixed(3));
  files.forEach(file => {
    const result = spawnSync(vgmComp, [file, file]);
    if (result.status !== 0) {
      WARN(result.stderr);
    } else {
      const result = spawnSync('gzip', ['-f', file]);
      if (result.status !== 0) {
        FATAL(result.stderr);
      } else {
        const newFile = `${file.slice(0, -4)}.vgz`;
        spawnSync('mv', [file + '.gz', newFile]);
      }
    }
  });

  files = glob.sync(`${path}*.vgz`);
  fileSize = 0;
  files.forEach(file => {
    fileSize += fs.lstatSync(file).size;
  });
  console.log(chalk.gray('Optimized to VGZ (%s MB on disk).'), (fileSize / 1048576).toFixed(3));
}
