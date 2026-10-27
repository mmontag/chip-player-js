// Bundle the real ES modules for Node's test runner without rewriting source.
const webpack = require('webpack');
const os = require('node:os');
const path = require('node:path');
const fs = require('node:fs');
const { spawnSync } = require('node:child_process');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'player-tests-'));
webpack({
  mode: 'development', target: 'node', devtool: false,
  entry: path.resolve(__dirname, 'player-lifecycle.test.js'),
  output: { path: dir, filename: 'tests.cjs' },
  module: { rules: [{ test: /\.(png|jpg|gif)$/, type: 'asset/resource' }] },
}, (error, stats) => {
  try {
    if (error || stats.hasErrors()) {
      console.error(error || stats.toString({ all: false, errors: true }));
      process.exitCode = 1;
      return;
    }
    const result = spawnSync(process.execPath, ['--require', path.join(__dirname, 'player-test-browser-globals.cjs'), '--test', path.join(dir, 'tests.cjs')], { stdio: 'inherit' });
    process.exitCode = result.status === null ? 1 : result.status;
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
