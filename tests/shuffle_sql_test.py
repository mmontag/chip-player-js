import json
import pathlib
import re
import sqlite3
import unittest


class ShuffleTest(unittest.TestCase):
    def test_literal_directory_prefixes(self):
        source = pathlib.Path('server/database.js').read_text()
        literal = re.search(r'getShuffleStmt: db\.prepare\((".*?")\)', source).group(1)
        sql = json.loads(literal)
        db = sqlite3.connect(':memory:')
        db.execute('CREATE TABLE music (path TEXT)')
        db.executemany('INSERT INTO music VALUES (?)', [
            ('A_B/song.mid',), ('A-B/song.mid',),
            ('100%/song.mid',), ('100x/song.mid',),
            ('slash\\name/song.mid',), ('slashxname/song.mid',),
        ])
        for pattern, expected in [
            (r'A\_B/%', 'A_B/song.mid'),
            (r'100\%/%', '100%/song.mid'),
            (r'slash\\name/%', 'slash\\name/song.mid'),
        ]:
            self.assertEqual(db.execute(sql, (pattern, 10)).fetchall(), [(expected,)])
        self.assertEqual(len(db.execute(sql, ('%', 10)).fetchall()), 6)


if __name__ == '__main__':
    unittest.main()
