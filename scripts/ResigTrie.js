// Borrowed from https://github.com/jeresig/trie-js

module.exports = {
  listToTrie: listToTrie,
  trieToList: trieToList,
};

function listToTrie(strings) {
  const trie = {},
    end = {},
    keepEnd = {},
    endings = [0];

  // Build a simple Trie structure
  for (let i = 0, l = strings.length; i < l; i++) {
    const word = strings[i], letters = word.split("");
    let cur = trie;

    for (let j = 0; j < letters.length; j++) {
      const letter = letters[j], pos = cur[letter];
      if (pos == null) {
        cur = cur[letter] = j === letters.length - 1 ? 0 : {};
      } else if (pos === 0) {
        cur = cur[letter] = {$: 0};
      } else {
        cur = cur[letter];
      }
    }
  }

  // Optimize the structure
  optimize(trie);
  // Figure out common suffixes
  suffixes(trie, end);

  for (const key in end) {
    if (end[key].count > 10) {
      keepEnd[key] = endings.length;
      endings.push(end[key].obj);
    }
  }
  // And extract the suffixes
  finishSuffixes(trie, keepEnd, end);
  trie.$ = endings;

  return trie;
}

function trieToList(trie) {
  const words = [];
  const end = trie.$;
  delete trie.$;
  dig('', trie);
  return words;

  function dig(word, cur) {
    for (const node in cur) {
      const val = cur[node];
      if (node === '$') {
        words.push(word);
      } else if (val === 0) {
        words.push(word + node);
      } else {
        dig(word + node, typeof val === 'number' ? end[val] : val);
      }
    }
  }
}

function optimize(cur) {
  let num = 0, last;

  for (let node in cur) {
    if (typeof cur[node] === "object") {
      const ret = optimize(cur[node]);

      if (ret) {
        delete cur[node];
        cur[node + ret.name] = ret.value;
        node = node + ret.name;
      }
    }

    last = node;
    num++;
  }

  if (num === 1) {
    return {name: last, value: cur[last]};
  }
}

function suffixes(cur, end) {
  let hasObject = false, key = "";

  for (const node in cur) {
    if (typeof cur[node] === "object") {
      hasObject = true;
      const ret = suffixes(cur[node], end);

      if (ret) {
        cur[node] = ret;
      }
    }

    key += "," + node;
  }

  if (!hasObject) {
    if (end[key]) {
      end[key].count++;
    } else {
      end[key] = {obj: cur, count: 1};
    }

    return key;
  }
}

function finishSuffixes(cur, keepEnd, end) {
  for (const node in cur) {
    const val = cur[node];

    if (typeof val === 'object') {
      finishSuffixes(val, keepEnd, end);
    } else if (typeof val === 'string') {
      cur[node] = keepEnd[val] || end[val].obj;
    }
  }
}
