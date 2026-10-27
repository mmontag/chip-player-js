const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('server/index.js', 'utf8');
function route(path, globals) {
  const prefix = `router.get('${path}',`;
  const start = source.indexOf(prefix);
  assert.notEqual(start, -1);
  const end = source.indexOf('\n});', start) + 4;
  let handler;
  vm.runInNewContext(source.slice(start, end), {
    ...globals, router: { get: (_path, ...args) => { handler = args.at(-1); } },
    cache1Hour() {}, console: { log() {}, error() {} }, performance,
  });
  return handler;
}
function invoke(handler, query) {
  let response;
  handler({ query }, { json: value => { response = value; } });
  return response;
}

test('cached short searches respect small and larger limits', () => {
  const cached = Array.from({ length: 50 }, (_, i) => ({ file: `song${i}` }));
  const all = Array.from({ length: 70 }, (_, i) => ({ file: `song${i}` }));
  const calls = [];
  const handler = route('/search', { searchMap: new Map([['ab', cached]]),
    searchStmt: { all: (term, limit) => { calls.push([term, limit]); return all.slice(0, limit); } } });
  assert.equal(invoke(handler, { query: 'ab', limit: '1' }).items.length, 1);
  assert.equal(invoke(handler, { query: 'ab', limit: '100' }).items.length, 70);
  assert.equal(calls.length, 1);
  assert.equal(calls[0][1], 100);
});

test('shuffle escapes SQL LIKE wildcards and backslashes in directory paths', () => {
  let argument;
  const handler = route('/shuffle', { getShuffleStmt: { pluck: () => ({
    all: (path, limit) => { argument = [path, limit]; return []; },
  }) } });
  invoke(handler, { path: 'test_100%\\mix', limit: '3' });
  assert.equal(argument[0], 'test\\_100\\%\\\\mix/%');
  assert.equal(argument[1], 3);
});
