#!/usr/bin/env python3
"""Explicit, disposable MVP2 demo. Existing normal/user databases are untouched."""
import argparse
import fcntl
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from mvp2_helpers import Server, SERVICES, PASSWORD, ok, send, befriend

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default=str(ROOT / 'build/mvp2/IMServer'))
    parser.add_argument('--reset', action='store_true')
    args = parser.parse_args()
    db = ROOT / 'data/mvp2-demo.sqlite3'
    db.parent.mkdir(exist_ok=True)
    with Path(str(db)+'.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if db.exists() and not args.reset:
            raise SystemExit('MVP2 demo exists; stop its server and explicitly use --reset to replace only this demo.')
        if args.reset:
            for suffix in ['', '-wal', '-shm']:
                Path(str(db)+suffix).unlink(missing_ok=True)
    server = Server(args.server, db)
    try:
        server.start()
        alice, ai = server.user('Alice')
        bob, bi = server.user('Bob')
        carol, ci = server.user('Carol', ['WECHAT'])
        david, di = server.user('David')
        emily, ei = server.user('Emily', ['QQ'])
        for client in [alice, bob]: ok(client.call('account.binding.create'))
        for source, target, display in [(alice,bob,bi['QQ']), (alice,david,di['QQ']),
                                        (bob,david,di['QQ']), (bob,emily,ei['QQ'])]:
            befriend(source,target,display)
        ok(alice.call('friend.remark.set',dict(service_type='QQ',target_display_id=bi['QQ'],remark='同学 Bob')))
        pending = send(bob,ai['WEIBO'],'WEIBO')
        cross = ok(alice.call('friend.recommend.cross',dict(source_service='QQ',target_service='WECHAT')))
        assert [item['display_id'] for item in cross['items']] == [bi['WECHAT']]
        assert ok(alice.call('friend.mutual.list',dict(service_type='QQ',target_display_id=bi['QQ'])))['total'] == 1
        for name, ids in [('Alice',ai),('Bob',bi),('Carol',ci),('David',di),('Emily',ei)]:
            print(name, ids)
        print('Demo-only password:', PASSWORD)
        print('QQ: Alice—Bob, Alice—David, Bob—David, Bob—Emily')
        print('QQ/WeChat binding: Alice and Bob only; David deliberately unbound.')
        print('Pending WEIBO Bob→Alice:', pending)
        print('Alice: QQ mutual(Bob)=David; same-service suggestion=Emily; QQ→WECHAT suggestion=Bob.')
        print('Database:', db)
    finally:
        server.close()
if __name__ == '__main__': main()
