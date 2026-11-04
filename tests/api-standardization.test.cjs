const { test } = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const Database = require('better-sqlite3');

test('database.js keeps SQL column names song_id and file_size', () => {
  const catalogDbPath = path.resolve(__dirname, '../server/catalog.db');
  const userDbPath = path.resolve(__dirname, '../server/users.db');

  const db = new Database(catalogDbPath, { readonly: true });
  db.exec(`ATTACH DATABASE '${userDbPath}' AS user_db`);

  try {
    const { dbStatements } = require('../server/database.js');

    const topGlobal = dbStatements.getGlobalTopStmt.all(0, 10, 5);
    if (topGlobal.length > 0) {
      assert.ok('song_id' in topGlobal[0], 'database.js query must use column name song_id');
      assert.ok('file_size' in topGlobal[0], 'database.js query must use column name file_size');
    }

    const topFavorites = dbStatements.getTopFavoritesStmt.all(10, 5);
    if (topFavorites.length > 0) {
      assert.ok('song_id' in topFavorites[0], 'database.js query must use column name song_id');
      assert.ok('file_size' in topFavorites[0], 'database.js query must use column name file_size');
    }
  } finally {
    db.close();
  }
});
