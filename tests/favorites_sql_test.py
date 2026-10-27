import pathlib
import re
import sqlite3
import unittest


class FavoriteRemovalTest(unittest.TestCase):
    def test_removal_preserves_unrelated_legacy_entries_and_matches_song_id(self):
        source = pathlib.Path('server/database.js').read_text()
        sql = re.search(r'removeFavoriteByPathStmt: db\.prepare\(`(.*?)`\)', source, re.S).group(1)
        db = sqlite3.connect(':memory:')
        db.execute('CREATE TABLE music (path TEXT, song_id TEXT)')
        db.execute('CREATE TABLE playlists (user_id TEXT, type TEXT, items TEXT, modified_at INTEGER)')
        db.executemany('INSERT INTO music VALUES (?, ?)', [('canonical.mid', 'same'), ('other.mid', 'other')])
        db.execute('INSERT INTO playlists VALUES (?, ?, ?, ?)', ('user', 'favorites',
            '[{"songId":"same","href":"old.mid"},{"songId":"other","href":"other.mid"}]', 0))
        db.execute(sql, {'path': 'canonical.mid', 'now': 1, 'userId': 'user'})
        self.assertEqual(db.execute('SELECT items FROM playlists').fetchone()[0],
                         '[{"songId":"other","href":"other.mid"}]')


if __name__ == '__main__':
    unittest.main()
