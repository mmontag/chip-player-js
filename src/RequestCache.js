import promisify from './promisifyXhr';

class RequestCache {
  constructor() {
    this.data = {};
    this.request = null;
  }

  fetchCached(url) {
    if (this.data[url]) return Promise.resolve(this.data[url]);

    if (this.request) this.request.abort();
    this.request = promisify(new XMLHttpRequest());
    this.request.responseType = 'json';
    this.request.open('GET', url);
    return this.request.send()
      .then(xhr => {
        if (xhr.status >= 200 && xhr.status < 400) {
          this.data[url] = xhr.response;
          return xhr.response;
        }
        throw Error('RequestCache HTTP ' + xhr.status);
      });
  }
}

// Cache singleton
const requestCache = new RequestCache();

export default requestCache;
