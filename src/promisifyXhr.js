export default function promisify(xhr) {
  const oldSend = xhr.send;
  xhr.send = function () {
    const xhrArguments = arguments;
    return new Promise(function (resolve, reject) {
      xhr.onload = function () {
        if (xhr.status < 200 || xhr.status >= 300) {
          reject({request: xhr});
        } else {
          resolve(xhr);
        }
      };
      xhr.onerror = function () {
        reject({request: xhr});
      };
      oldSend.apply(xhr, xhrArguments);
    });
  };
  return xhr;
}
