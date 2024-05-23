import path from 'path';
import autoBind from 'auto-bind';
import debounce from 'lodash/debounce';

/**
 * A thin wrapper around Emscripten filesystem.
 */
export default class LocalFilesManager {
  constructor(emscriptenFS, rootPath) {
    autoBind(this);
    this.fs = emscriptenFS;
    this.rootPath = rootPath || 'local';
    this.fs.mkdirTree(this.rootPath);
    this.fs.mount(this.fs.filesystems.IDBFS, {}, this.rootPath);

    this.commit = debounce(this.commit, 500);
  }

  fullPath(filePath) {
    // TODO: no-op - the caller of read/write/delete knows about the '/local' root path.
    return filePath;
  }

  /**
   * readAll wraps Emscripten FS to make it behave like the
   * Chip Player node.js server (browse endpoint).
   */
  readAll() {
    const fileList = this.fs.readdir(this.rootPath);

    if (fileList) {
      const filesInfo = fileList
        .filter(file => file !== '..' && file !== '.')
        .map((file, idx) => {
          const fullPath = path.join(this.rootPath, file);
          const {
            size,
            mtime
          } = this.fs.stat(fullPath);
          return {
            idx: idx,
            type: 'file',
            size: size,
            mtime: Math.floor(Date.parse(mtime) / 1000),
            path: fullPath,
          };
        });

      // Sort by mtime ascending, then by path ascending
      filesInfo.sort((a, b) => {
        if (a.mtime !== b.mtime) {
          return a.mtime - b.mtime;
        } else {
          return a.path.localeCompare(b.path);
        }
      });

      return filesInfo;
    }

    return [];
  }

  write(filePath, buffer) {
    const data = new Uint8Array(buffer)
    this.fs.writeFile(this.fullPath(filePath), data);
    this.commit();
  }

  read(filePath) {
    return this.fs.readFile(this.fullPath(filePath));
  }

  delete(filePath) {
    if (this.fs.analyzePath(filePath).exists) {
      this.fs.unlink(this.fullPath(filePath));
      this.commit();
      return true;
    }
    return false;
  }

  commit() {
    return new Promise((resolve, reject) => {
      this.fs.syncfs(false, (err) => {
        if (err) {
          console.error('Error synchronizing local files to indexeddb.', err);
          reject();
        } else {
          console.debug(`Synchronized local files to indexeddb.`);
          resolve();
        }
      });
    });
  }
}
