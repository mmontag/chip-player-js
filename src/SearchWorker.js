import TrieSearch from 'trie-search';
import {trieToList} from "./ResigTrie";

const filesTrie = require('./catalog.json');

const trie = new TrieSearch('file', {
  indexField: 'id',
  idFieldOrFunction: 'id',
  splitOnRegEx: /[^A-Za-z0-9]/g,
});

onmessage = (message) => {
  const data = message.data;
  switch (data.type) {
    case 'load':
      const start = performance.now();
      const files = trieToList(filesTrie).map((file, i) => ({id: i, file: file}));
      trie.addAll(files);
      const time = (performance.now() - start).toFixed(1);
      console.log('Added %s items (%s tokens) to search trie in %s ms.', files.length, trie.size, time);
      postMessage({
        type: 'status',
        payload: {numRecords: files.length}
      });
      break;
    case 'search':
      search(data.payload.query, data.payload.maxResults);
      break;
  }
};

function search(query, maxResults) {
  let results = trie.get(query, TrieSearch.UNION_REDUCER) || [];
  if (maxResults) {
    results = results.slice(0, maxResults);
  }
  postMessage({
    type: 'results',
    payload: results
  });
}
