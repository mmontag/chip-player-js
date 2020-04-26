export default function promisify(xhr) {
  const oldSend = xhr.send;
  xhr.send = function () {
    const xhrArguments = arguments;
    return new Promise(function (resolve, reject) {
      xhr.onload = function () {
        if (xhr.status < 200 || xhr.status >= 300) {
          reject({
            status: xhr.status,
            statusText: xhr.statusText,
          });
        } else {
          resolve(xhr);
        }
      };
      xhr.onerror = function () {
        reject({
          status: xhr.status,
          statusText: xhr.statusText,
        });
      };
      try {
        oldSend.apply(xhr, xhrArguments);
      } catch (e) {}
    });
  };
  return xhr;
}
