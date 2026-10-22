#!/usr/bin/env python3
"""MVP2 contract acceptance through real TCP and disposable SQLite databases.

Run: python3 tests/mvp2_acceptance.py --server /absolute/path/IMServer
M2-T14 covers server rollback/disconnect only. M2-T18 requires separate Qt GUI evidence.
This script never opens or resets a user's development database.
"""
import argparse
import concurrent.futures
import sqlite3
import tempfile
import traceback
import uuid
from pathlib import Path
from mvp2_helpers import (Server, PASSWORD, SERVICES, registration, ok, error, rows,
                          no_private, send, befriend)

class Acceptance:
    def __init__(self, server, directory):
        self.s, self.directory = server, Path(directory)
    def user(self, name, *args, **kwargs):
        return self.s.user(name, *args, **kwargs)
    def sql(self, query, args=()):
        with sqlite3.connect(self.s.db) as db: return db.execute(query, args).fetchall()
    def friends(self, client, service='QQ'):
        return rows(client, 'friend.list', service_type=service)['items']
    def cross(self, client, source='QQ', target='WECHAT', kind='friend.recommend.cross'):
        return rows(client, kind, source_service=source, target_service=target)
    def test01(self):
        a, ids = self.user('t01', ['WECHAT'])
        first = ok(a.call('account.service.open', {'service_type':'QQ'}))
        qq = first['account']['display_id']
        assert not next(x for x in first['services'] if x['service_type']=='QQ')['active']
        error(a.call('friend.list', {'service_type':'QQ'}), 'SERVICE_NOT_ACTIVE')
        assert ok(a.call('account.service.open', {'service_type':'WEIBO'}))['account']['display_id']==qq
        assert ok(a.call('account.service.open', {'service_type':'QQ'}))['account']['display_id']==qq
        assert len(ok(a.call('account.services.list'))['services']) == 3
        assert len(self.sql('SELECT * FROM accounts WHERE user_id=(SELECT user_id FROM users WHERE login_name=?)', ('t01',)))==3
    def test02(self):
        a, ai = self.user('t02a'); b, bi = self.user('t02b')
        befriend(a,b,bi['QQ']); befriend(a,b,bi['WECHAT'],'WECHAT')
        ok(a.call('friend.remark.set', dict(service_type='QQ',target_display_id=bi['QQ'],remark='saved')))
        assert ok(a.call('account.binding.create'))['bound']
        assert ok(a.call('account.binding.create'))['bound']
        binding=ok(a.call('account.binding.get'))
        assert binding['qq_display_id']==ai['QQ'] and binding['wechat_display_id']==ai['WECHAT']
        for payload in [{'user_id':9999},{'qq_account_pk':1,'wechat_account_pk':2},{'wechat_display_id':bi['WECHAT']}]:
            assert a.call('account.binding.create',payload)['success'] is False
        assert not ok(a.call('account.binding.remove'))['bound']
        assert self.friends(a)[0]['remark']=='saved' and len(self.friends(a,'WECHAT'))==1
        assert len(ok(a.call('account.services.list'))['services'])==3
    def test03(self):
        a, ai = self.user('t03a'); b, bi = self.user('t03b')
        assert bi['QQ']==bi['WEIBO']
        for service in ['QQ','WEIBO']:
            result=rows(a,'friend.search',service_type=service,query=bi[service])
            assert result['total']==1 and result['items'][0]['service_type']==service
        befriend(a,b,bi['QQ'])
        assert len(self.friends(a))==1 and self.friends(a,'WEIBO')==[]
    def test04(self):
        a, ai = self.user('t04a'); b, bi = self.user('t04b'); c, ci = self.user('t04c',['WECHAT'])
        found=rows(a,'friend.search',service_type='QQ',query=bi['QQ'])
        assert found['total']==1 and found['items'][0]['display_id']==bi['QQ']; no_private(found)
        assert rows(a,'friend.search',service_type='QQ',query='t04c')['total']==0
        for i in range(22): self.user('t04page%02d'%i,['QQ'])
        result=rows(a,'friend.search',service_type='QQ',query='t04page',limit=7)
        seen=[]
        while True:
            assert len(result['items'])<=7 and result['total']==22; no_private(result)
            seen += [x['display_id'] for x in result['items']]
            if result['next_cursor'] is None: break
            result=rows(a,'friend.search',service_type='QQ',query='t04page',limit=7,cursor=result['next_cursor'])
        assert len(seen)==len(set(seen))==22 and seen==sorted(seen)
        for extra in [{'limit':0},{'limit':21},{'cursor':'-1'},{'cursor':'garbage'}]:
            error(a.call('friend.search',dict(service_type='QQ',query='t04page',**extra)),'VALIDATION_ERROR')
    def test05(self):
        a, ai = self.user('t05a'); b, bi = self.user('t05b')
        mark=len(b.events); rid=send(a,bi['QQ'])
        assert self.sql('SELECT status FROM friend_requests WHERE request_id=?',(rid,))==[('PENDING',)]
        event=b.event('friend.request.new',mark); no_private(event)
        assert event['data']['friend_request_id']==rid
        pending=rows(b,'friend.request.pending',service_type='QQ'); no_private(pending)
        assert any(x['friend_request_id']==rid and x['direction']=='INCOMING' for x in pending['items'])
    def test06(self):
        a, ai = self.user('t06a'); b, bi = self.user('t06b'); request=befriend(a,b,bi['QQ'])
        assert [x['display_id'] for x in self.friends(a)]==[bi['QQ']]
        assert [x['display_id'] for x in self.friends(b)]==[ai['QQ']]
        assert self.sql('SELECT count(*) FROM friendships WHERE account_a IN (SELECT account_pk FROM accounts WHERE display_id=? AND service_type=?) AND account_b IN (SELECT account_pk FROM accounts WHERE display_id=? AND service_type=?)', (ai['QQ'],'QQ',bi['QQ'],'QQ'))==[(1,)]
        assert self.sql('SELECT count(*) FROM friendships WHERE account_a>=account_b')==[(0,)]
    def test07(self):
        a, ai = self.user('t07a'); b, bi = self.user('t07b'); rid=send(a,bi['QQ'])
        payload=dict(friend_request_id=rid,decision='REJECT'); correlation=str(uuid.uuid4())
        first=ok(b.call('friend.request.respond',payload,correlation)); assert first['status']=='REJECTED'
        assert ok(b.call('friend.request.respond',payload,correlation))==first
        error(b.call('friend.request.respond',dict(payload,decision='ACCEPT')),'FRIEND_REQUEST_ALREADY_RESOLVED')
        assert self.friends(a)==self.friends(b)==[]
    def test08(self):
        a, ai = self.user('t08a'); b, bi = self.user('t08b'); c, ci = self.user('t08c',['WECHAT'])
        error(a.call('friend.request.send',dict(service_type='QQ',target_display_id=ai['QQ'])),'FRIEND_SELF_FORBIDDEN')
        error(a.call('friend.request.send',dict(service_type='QQ',target_display_id=ci['WECHAT'])),'ACCOUNT_NOT_FOUND')
        rid=send(a,bi['QQ'])
        error(a.call('friend.request.send',dict(service_type='QQ',target_display_id=bi['QQ'])),'FRIEND_REQUEST_PENDING')
        error(b.call('friend.request.send',dict(service_type='QQ',target_display_id=ai['QQ'])),'FRIEND_REQUEST_PENDING')
        error(a.call('friend.request.respond',dict(friend_request_id=rid,decision='ACCEPT',user_id=9999)),'FRIEND_PERMISSION_DENIED')
        anon=self.s.client()
        error(anon.call('friend.list',dict(service_type='QQ',user_id=1,session_id='forged')),'AUTH_REQUIRED')
        inactive=self.s.login('t08b',[])
        error(inactive.call('friend.request.respond',dict(friend_request_id=rid,decision='ACCEPT')),'SERVICE_NOT_ACTIVE')
        ok(b.call('friend.request.respond',dict(friend_request_id=rid,decision='ACCEPT')))
        error(a.call('friend.request.send',dict(service_type='QQ',target_display_id=bi['QQ'])),'FRIEND_ALREADY_EXISTS')
        # Session identity overrides a forged actor rather than granting another user's graph.
        forged=rows(a,'friend.list',service_type='QQ',user_id=9999,from_account_id=9999)
        assert [x['display_id'] for x in forged['items']]==[bi['QQ']]; no_private(forged)
    def test09(self):
        a, ai=self.user('t09a'); b, bi=self.user('t09b'); befriend(a,b,bi['QQ']); befriend(a,b,bi['WEIBO'],'WEIBO')
        ok(a.call('friend.remark.set',dict(service_type='QQ',target_display_id=bi['QQ'],remark='my Bob')))
        assert self.friends(a)[0]['remark']=='my Bob'
        assert self.friends(b)[0]['remark']==''
        assert rows(a,'friend.search',service_type='QQ',query=bi['QQ'])['items'][0]['nickname']=='t09b'
        ok(a.call('friend.remark.set',dict(service_type='QQ',target_display_id=bi['QQ'],remark='')))
        assert self.friends(a)[0]['remark']==''
        ok(b.call('friend.remark.set',dict(service_type='QQ',target_display_id=ai['QQ'],remark='reverse')))
        ok(a.call('friend.remove',dict(service_type='QQ',target_display_id=bi['QQ'])))
        assert self.friends(a)==self.friends(b)==[] and len(self.friends(a,'WEIBO'))==1
        assert not self.sql('SELECT 1 FROM friend_remarks WHERE owner_account_pk=(SELECT account_pk FROM accounts JOIN users USING(user_id) WHERE login_name=? AND service_type=?)',('t09b','QQ'))
    def test10(self):
        a, ai=self.user('t10a'); b, bi=self.user('t10b'); c, ci=self.user('t10c'); d, di=self.user('t10d'); e, ei=self.user('t10e')
        for source,target,ids in [(a,b,bi),(a,c,ci),(b,c,ci),(b,d,di),(c,d,di),(b,e,ei)]: befriend(source,target,ids['QQ'])
        send(a,ei['QQ'])
        result=rows(a,'friend.mutual.list',service_type='QQ',target_display_id=bi['QQ'])
        assert result['total']==1 and result['items'][0]['display_id']==ci['QQ']; no_private(result)
        rec=rows(a,'friend.recommend.same',service_type='QQ')
        assert [(x['display_id'],x['mutual_count']) for x in rec['items']]==[(di['QQ'],2)]; no_private(rec)
    def test11(self):
        a, ai=self.user('t11a'); b, bi=self.user('t11b'); befriend(a,b,bi['QQ'])
        ok(a.call('account.binding.create')); ok(b.call('account.binding.create'))
        assert [x['display_id'] for x in self.cross(a)['items']]==[bi['WECHAT']]
        befriend(a,b,bi['WECHAT'],'WECHAT')
        assert self.cross(a)['items']==[]
        common=self.cross(a,kind='friend.cross.common'); assert [x['display_id'] for x in common['items']]==[bi['WECHAT']]; no_private(common)
        ok(b.call('account.binding.remove'))
        assert self.cross(a,kind='friend.cross.common')['items']==[]
        ok(b.call('account.binding.create')); ok(a.call('account.binding.remove'))
        error(a.call('friend.cross.common',dict(source_service='QQ',target_service='WECHAT')),'BINDING_REQUIRED')
    def test12(self):
        a, ai=self.user('t12a'); b, bi=self.user('t12b'); c, ci=self.user('t12c',['QQ','WEIBO'])
        befriend(a,b,bi['QQ']); befriend(a,c,ci['QQ'])
        error(a.call('friend.recommend.cross',dict(source_service='QQ',target_service='WECHAT')),'BINDING_REQUIRED')
        ok(a.call('account.binding.create'))
        assert self.cross(a)['items']==[]
        ok(b.call('account.binding.create'))
        assert [x['display_id'] for x in self.cross(a)['items']]==[bi['WECHAT']]
        inactive=self.s.login('t12a',['QQ'])
        error(inactive.call('friend.recommend.cross',dict(source_service='QQ',target_service='WECHAT')),'SERVICE_NOT_ACTIVE')
        # WECHAT<->WEIBO uses the QQ binding as an explicit bridge.
        befriend(a,b,bi['WECHAT'],'WECHAT')
        assert [x['display_id'] for x in self.cross(a,'WECHAT','WEIBO')['items']]==[bi['WEIBO']]
        ok(b.call('account.binding.remove'))
        assert self.cross(a,'WECHAT','WEIBO')['items']==[]
        assert sorted(x['display_id'] for x in self.cross(a,'QQ','WEIBO')['items'])==sorted([bi['WEIBO'],ci['WEIBO']])
        assert len(self.friends(a,'WEIBO'))==0
        assert self.sql("SELECT count(*) FROM accounts JOIN users USING(user_id) WHERE login_name='t12c'")==[(2,)]
    def test13(self):
        a, ai=self.user('t13a'); b, bi=self.user('t13b')
        ma,mb=len(a.events),len(b.events); rid=send(a,bi['QQ'])
        b.event('friend.request.new',mb)
        ok(b.call('friend.request.respond',dict(friend_request_id=rid,decision='ACCEPT')))
        a.event('friend.request.resolved',ma); a.event('friend.relationship.changed',ma); b.event('friend.relationship.changed',mb)
        ma,mb=len(a.events),len(b.events)
        ok(a.call('friend.remove',dict(service_type='QQ',target_display_id=bi['QQ'])))
        a.event('friend.relationship.changed',ma); b.event('friend.relationship.changed',mb)
        ok(b.call('auth.logout')); b.close(); rid=send(a,bi['QQ'])
        b=self.s.login('t13b')
        assert any(x['friend_request_id']==rid for x in rows(b,'friend.request.pending',service_type='QQ')['items'])
        ok(b.call('friend.request.respond',dict(friend_request_id=rid,decision='ACCEPT')))
        assert len(self.friends(a))==len(self.friends(b))==1
    def test14(self):
        a, ai=self.user('t14a',['WECHAT']); b, bi=self.user('t14b')
        before=self.sql('SELECT * FROM id_counters ORDER BY namespace')
        with sqlite3.connect(self.s.db) as db: db.execute("CREATE TRIGGER m2_fail_open BEFORE INSERT ON accounts BEGIN SELECT RAISE(ABORT,'injected'); END")
        rid=str(uuid.uuid4())
        try: error(a.call('account.service.open',{'service_type':'QQ'},rid),'DATABASE_ERROR')
        finally:
            with sqlite3.connect(self.s.db) as db: db.execute('DROP TRIGGER m2_fail_open')
        assert self.sql('SELECT * FROM id_counters ORDER BY namespace')==before
        assert len(ok(a.call('account.services.list'))['services'])==1
        assert self.sql('SELECT count(*) FROM mutation_requests WHERE request_id=?',(rid,))==[(0,)]
        ok(a.call('account.service.open',{'service_type':'QQ'},rid)); ok(a.call('account.service.activate',{'service_type':'QQ'}))
        request=send(a,bi['QQ'])
        with sqlite3.connect(self.s.db) as db: db.execute("CREATE TRIGGER m2_fail_accept BEFORE INSERT ON friendships BEGIN SELECT RAISE(ABORT,'injected'); END")
        response_id=str(uuid.uuid4()); data=dict(friend_request_id=request,decision='ACCEPT')
        try: error(b.call('friend.request.respond',data,response_id),'DATABASE_ERROR')
        finally:
            with sqlite3.connect(self.s.db) as db: db.execute('DROP TRIGGER m2_fail_accept')
        assert self.sql('SELECT status FROM friend_requests WHERE request_id=?',(request,))==[('PENDING',)]
        assert self.friends(a)==[]
        assert self.sql('SELECT count(*) FROM mutation_requests WHERE request_id=?',(response_id,))==[(0,)]
        ok(b.call('friend.request.respond',data,response_id))
        # Abrupt connection close must not kill server or expose another session.
        disconnected=self.s.client(); disconnected.close()
        assert ok(a.call('system.ping')) is not None
    def test15(self):
        a, ai=self.user('t15a',['QQ','WECHAT']); b, bi=self.user('t15b'); c, ci=self.user('t15c')
        befriend(a,b,bi['QQ']); pending=send(c,ai['QQ']); ok(a.call('account.binding.create'))
        ok(a.call('friend.remark.set',dict(service_type='QQ',target_display_id=bi['QQ'],remark='persist')))
        rid=str(uuid.uuid4()); payload={'service_type':'WEIBO'}
        opened=ok(a.call('account.service.open',payload,rid))
        self.s.stop(); self.s.start(); a=self.s.login('t15a')
        assert self.friends(a)[0]['remark']=='persist'
        assert ok(a.call('account.binding.get'))['bound']
        assert any(x['friend_request_id']==pending for x in rows(a,'friend.request.pending',service_type='QQ')['items'])
        assert ok(a.call('account.service.open',payload,rid))==opened
        assert len(ok(a.call('account.services.list'))['services'])==3
    def test16(self):
        a, ai=self.user('t16a'); b, bi=self.user('t16b')
        a2=self.s.login('t16a'); b2=self.s.login('t16b')
        def race(functions):
            with concurrent.futures.ThreadPoolExecutor(max_workers=len(functions)) as pool:
                return list(pool.map(lambda fn:fn(),functions))
        result=race([lambda:a.call('friend.request.send',dict(service_type='QQ',target_display_id=bi['QQ'])),lambda:b.call('friend.request.send',dict(service_type='QQ',target_display_id=ai['QQ']))])
        assert sum(x['success'] for x in result)==1, result
        winner=0 if result[0]['success'] else 1; target=b if winner==0 else a; target2=b2 if winner==0 else a2
        request=result[winner]['data']['friend_request_id']; payload=dict(friend_request_id=request,decision='ACCEPT')
        result=race([lambda:target.call('friend.request.respond',payload),lambda:target2.call('friend.request.respond',payload)])
        assert sum(x['success'] for x in result)==1 and len(self.friends(a))==1, result
        # Exercise every MVP2 mutation with identical replay and changed-data conflict.
        c, ci=self.user('t16c',['QQ','WECHAT']); d, di=self.user('t16d')
        def replay(client,kind,data):
            rid=str(uuid.uuid4()); first=ok(client.call(kind,data,rid))
            ok(client.call('system.ping')); event_count=len(client.events)
            assert ok(client.call(kind,data,rid))==first
            ok(client.call('system.ping'))
            assert len(client.events)==event_count, 'idempotent replay emitted duplicate event'
            error(client.call(kind,dict(data,unexpected='changed payload'),rid),'REQUEST_ID_CONFLICT')
            return first,rid
        replay(c,'account.service.open',{'service_type':'WEIBO'})
        replay(c,'account.binding.create',{})
        replay(c,'account.binding.remove',{})
        sent,send_id=replay(c,'friend.request.send',dict(service_type='QQ',target_display_id=di['QQ']))
        replay(d,'friend.request.respond',dict(friend_request_id=sent['friend_request_id'],decision='ACCEPT'))
        remark_data=dict(service_type='QQ',target_display_id=di['QQ'],remark='idempotent')
        _,remark_rid=replay(c,'friend.remark.set',remark_data)
        inactive=self.s.login('t16c',[])
        error(inactive.call('friend.remark.set',remark_data,remark_rid),'SERVICE_NOT_ACTIVE')
        replay(c,'friend.remove',dict(service_type='QQ',target_display_id=di['QQ']))
        error(d.call('friend.request.send',dict(service_type='QQ',target_display_id=ci['QQ']),send_id),'REQUEST_ID_CONFLICT')
        error(c.call('account.binding.create',{},send_id),'REQUEST_ID_CONFLICT')
        # Same correlation concurrently across connections creates exactly one pending record.
        e, ei=self.user('t16e'); e2=self.s.login('t16e'); f, fi=self.user('t16f')
        rid=str(uuid.uuid4()); data=dict(service_type='QQ',target_display_id=fi['QQ'])
        result=race([lambda:e.call('friend.request.send',data,rid),lambda:e2.call('friend.request.send',data,rid)])
        assert all(x['success'] for x in result) and result[0]['data']==result[1]['data'],result
        assert rows(f,'friend.request.pending',service_type='QQ')['total']==1
    def test17(self):
        # Generate authentic MVP1 credentials/fingerprint via public registration,
        # then copy only the untouched v1 schema tables into a NEW database.
        seed=self.s.client(); rid=str(uuid.uuid4()); data=registration('t17legacy',['WECHAT'])
        registered=ok(seed.call('auth.register',data,rid))
        legacy=self.directory/'legacy-v1.sqlite3'
        tables=['users','accounts','id_counters','registration_requests']; snapshot={}
        with sqlite3.connect(self.s.db) as source, sqlite3.connect(legacy) as target:
            target.executescript((Path(__file__).resolve().parents[1]/'database/schema.sql').read_text())
            target.execute('PRAGMA foreign_keys=ON')
            target.execute('DELETE FROM id_counters')
            # All original rows are copied, so IDs and registration replay remain exact.
            for table in tables:
                records=source.execute('SELECT * FROM '+table).fetchall(); snapshot[table]=records
                for record in records:
                    target.execute('INSERT INTO '+table+' VALUES('+','.join('?' for _ in record)+')',record)
        server=Server(self.s.exe,legacy)
        try:
            server.start()
            with sqlite3.connect(legacy) as db:
                assert db.execute('PRAGMA user_version').fetchone()[0]==4
                for table,records in snapshot.items(): assert db.execute('SELECT * FROM '+table).fetchall()==records,table
                assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
            backups=list(self.directory.glob('legacy-v1.sqlite3*v1*'))
            assert backups,'migration did not create a v1 backup'
            verified=False
            for backup in backups:
                with sqlite3.connect(backup) as db:
                    if db.execute('PRAGMA user_version').fetchone()[0]!=1: continue
                    assert db.execute('PRAGMA quick_check').fetchone()[0]=='ok'
                    for table,records in snapshot.items(): assert db.execute('SELECT * FROM '+table).fetchall()==records,table
                    verified=True
            assert verified
            client=server.login('t17legacy',['WECHAT'])
            assert ok(client.call('auth.register',data,rid))==registered
            ok(client.call('account.service.open',{'service_type':'QQ'}))
            before={p.name:p.read_bytes() for p in backups}
            server.stop(); server.start(); server.login('t17legacy',['QQ','WECHAT'])
            assert all((self.directory/name).read_bytes()==contents for name,contents in before.items())
            with sqlite3.connect(legacy) as db: assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
        finally: server.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',required=True)
    parser.add_argument('--only',help='comma-separated case numbers, e.g. 4,16,17')
    args=parser.parse_args()
    selected=set(map(int,args.only.split(','))) if args.only else set(range(1,18))
    executable=Path(args.server).resolve(); assert executable.is_file(), executable
    failures=[]
    with tempfile.TemporaryDirectory(prefix='im-mvp2-') as directory:
        server=Server(executable,Path(directory)/'acceptance.sqlite3')
        try:
            server.start(); suite=Acceptance(server,directory)
            for number in sorted(selected):
                try:
                    getattr(suite,'test%02d'%number)()
                    print('M2-T%02d PASS'%number,flush=True)
                except Exception as exc:
                    failures.append(number)
                    print('M2-T%02d FAIL: %s: %s'%(number,type(exc).__name__,exc),flush=True)
                    traceback.print_exc()
            print('M2-T18 PARTIAL: IMServer built externally and startup exercised; Qt Creator and three-client GUI require separate evidence.',flush=True)
            print('M2-T14 scope: server SQL rollback and disconnect only; GUI responsiveness requires separate evidence.',flush=True)
        finally: server.close()
    print('%d passed; %d failed; %d not selected'%(len(selected)-len(failures),len(failures),17-len(selected)))
    return bool(failures)

if __name__=='__main__': raise SystemExit(main())
