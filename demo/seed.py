#!/usr/bin/env python3
"""Create only the dedicated demo database. Never called by normal startup."""
import argparse
import fcntl
import socket
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from acceptance import start, stop, connect, call, reg

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--server', default=str(ROOT / 'build/mvp1/IMServer'))
    ap.add_argument('--reset', action='store_true', help='explicitly replace only data/mvp1-demo.sqlite3')
    args = ap.parse_args()
    folder = ROOT / 'data'; folder.mkdir(exist_ok=True)
    db = folder / 'mvp1-demo.sqlite3'
    with (folder / 'mvp1-demo.sqlite3.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if db.exists() and not args.reset:
            raise SystemExit('Demo database exists. Use --reset explicitly, after stopping its server.')
        if args.reset:
            for suffix in ['', '-wal', '-shm']:
                Path(str(db) + suffix).unlink(missing_ok=True)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0)); port = probe.getsockname()[1]
    with (folder / 'demo-seed.log').open('w') as log:
        p = start(args.server, db, port, log)
        try:
            with connect(port) as s:
                for name, services in [('Alice',['QQ']),('Bob',['QQ','WECHAT','WEIBO']),('Carol',['WECHAT'])]:
                    r = call(s, 'auth.register', reg(name, services))
                    if not r['success']: raise RuntimeError(r['error']['code'])
                    print(name, r['data'])
        finally: stop(p)
    print('Demo password for all three accounts: Mvp1-Pass-2026!')
    print('Start IMServer --db', db)

if __name__ == '__main__': main()
