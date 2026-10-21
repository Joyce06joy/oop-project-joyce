#!/usr/bin/env python3
"""Real TCP/service acceptance. Uses disposable databases only."""
import argparse, concurrent.futures, json, os, socket, sqlite3, struct, subprocess, tempfile, time, uuid
from pathlib import Path

def packet(t, data=None, rid=None):
    obj = dict(kind='request', type=t, request_id=rid or str(uuid.uuid4()), data=data or {})
    raw = json.dumps(obj, ensure_ascii=False).encode()
    return struct.pack('!I', len(raw)) + raw

def read(s):
    def exact(n):
        b = b''
        while len(b)<n:
            x=s.recv(n-len(b))
            if not x: raise EOFError('connection closed')
            b+=x
        return b
    return json.loads(exact(struct.unpack('!I', exact(4))[0]))

def connect(port):
    s=socket.create_connection(('127.0.0.1',port),timeout=5); s.settimeout(8); return s

def call(s,t,data=None,rid=None):
    s.sendall(packet(t,data,rid)); return read(s)

def reg(name, services):
    return dict(login_name=name,nickname=name.title(),password='Mvp1-Pass-2026!',birthday=None,location='长春',services=services)

def start(exe, db, port, log):
    p=subprocess.Popen([exe,'--db',str(db),'--port',str(port)],stdout=log,stderr=log)
    for _ in range(100):
        if p.poll() is not None: raise RuntimeError('server exited at startup')
        try:
            s=connect(port); s.close(); return p
        except OSError: time.sleep(.05)
    p.terminate(); raise RuntimeError('server not ready')

def stop(p):
    p.send_signal(2); p.wait(timeout=10); assert p.returncode==0, p.returncode

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--server',required=True); args=ap.parse_args()
    assert Path(args.server).is_file(), 'MVP1 IMServer executable does not exist yet'
    with tempfile.TemporaryDirectory(prefix='im-mvp1-') as td:
        db=Path(td)/'acceptance.sqlite3'; logfile=Path(td)/'server.log'
        with socket.socket() as probe:
            probe.bind(('127.0.0.1',0)); port=probe.getsockname()[1]
        with logfile.open('w') as log:
            p=start(args.server,db,port,log)
            try:
                a,b,c=[connect(port) for _ in range(3)]
                alice=call(a,'auth.register',reg('Alice',['QQ'])); assert alice['success'],alice
                bob=call(b,'auth.register',reg('Bob',['QQ','WECHAT','WEIBO'])); assert bob['success'],bob
                carol=call(c,'auth.register',reg('Carol',['WECHAT'])); assert carol['success'],carol
                assert len({x['data']['user_id'] for x in [alice,bob,carol]})==3
                ids={x['service_type']:x['display_id'] for x in bob['data']['accounts']}
                assert ids['QQ']==ids['WEIBO'] and ids['WECHAT'].startswith('WX')
                assert [x['service_type'] for x in carol['data']['accounts']]==['WECHAT']
                print('T01 T02 T03 PASS: saved registrations and ID spaces')
                for s,name,service in [(a,'ALICE','QQ'),(b,'bob','WEIBO'),(c,'Carol','WECHAT')]:
                    r=call(s,'auth.login',dict(login_name=name,password='Mvp1-Pass-2026!')); assert r['success'],r
                    assert call(s,'account.service.activate',dict(service_type=service))['success']
                    assert any(v['active'] for v in call(s,'account.services.list')['data']['services'])
                assert call(a,'account.service.activate',dict(service_type='WECHAT'))['error']['code']=='SERVICE_NOT_OPEN'
                assert call(a,'auth.logout')['success']
                assert call(a,'account.services.list')['error']['code']=='AUTH_REQUIRED'
                assert call(a,'auth.login',dict(login_name='Alice',password='wrong'))['error']['code']=='AUTH_INVALID_CREDENTIALS'
                assert call(a,'auth.login',dict(login_name='absent',password='wrong'))['error']['code']=='AUTH_INVALID_CREDENTIALS'
                print('T04 PASS; T05 service PASS; three simultaneous TCP sessions PASS')
                rid=str(uuid.uuid4()); data=reg('Dave',['WEIBO','QQ'])
                r=call(a,'auth.register',data,rid); assert r['success']
                data['services']=['QQ','WEIBO']; data['login_name']='DAVE'
                assert call(a,'auth.register',data,rid)['data']==r['data']
                data['nickname']='different'
                assert call(a,'auth.register',data,rid)['error']['code']=='REQUEST_ID_CONFLICT'
                assert call(a,'auth.register',reg('aLiCe',['QQ']))['error']['code']=='LOGIN_NAME_TAKEN'
                print('T09 PASS: normalized replay, conflict, duplicate login')
                for data in [reg('bad name',['QQ']), reg('empty',[]),dict(reg('bad',['QQ']),birthday='2026-02-30'),dict(reg('bad',['QQ']),password='short')]:
                    assert call(a,'auth.register',data)['error']['code']=='VALIDATION_ERROR'
                frame=packet('system.ping')
                for byte in frame: a.sendall(bytes([byte]))
                assert read(a)['success']
                a.sendall(packet('system.ping')+packet('system.ping')); assert read(a)['success'] and read(a)['success']
                for raw in [struct.pack('!I',0),struct.pack('!I',65537),struct.pack('!I',2)+b'\xff\xfe',struct.pack('!I',1)+b'{']:
                    with connect(port) as bad:
                        bad.sendall(raw)
                        try:
                            response=read(bad); assert response['error']['code']=='PROTOCOL_ERROR'
                        except (EOFError,ConnectionResetError): pass
                assert call(b,'system.ping')['success']
                # A valid maximum-size request must never crash response framing.
                huge = dict(kind='request',type='',request_id=str(uuid.uuid4()),data={})
                overhead=len(json.dumps(huge,separators=(',',':')).encode())
                huge['type']='x'*(65536-overhead)
                raw=json.dumps(huge,separators=(',',':')).encode()
                with connect(port) as edge:
                    edge.sendall(struct.pack('!I',len(raw))+raw)
                    try:
                        result=read(edge); assert result['error']['code']=='PROTOCOL_ERROR'
                    except (EOFError,ConnectionResetError): pass
                assert call(b,'system.ping')['success'], 'oversize error response killed server'
                print('T11 PASS: split/coalesced/invalid length/UTF8/JSON')
                with sqlite3.connect(db) as sql:
                    sql.execute('PRAGMA foreign_keys=ON')
                    assert sql.execute('PRAGMA user_version').fetchone()[0]==1
                    assert sql.execute('select count(*) from users').fetchone()[0]==4
                    for statement in ["insert into accounts(user_id,service_type,display_id,status,opened_at) values(9999,'QQ','x','ACTIVE','now')", "insert into accounts(user_id,service_type,display_id,status,opened_at) select user_id,service_type,display_id,status,opened_at from accounts limit 1", "insert into users(login_name,password_hash,nickname,registered_at) values('ALICE','x','x','now')"]:
                        try: sql.execute(statement)
                        except sqlite3.IntegrityError: sql.rollback()
                        else: raise AssertionError('constraint not enforced')
                    counters_before=sql.execute('SELECT namespace,next_value FROM id_counters ORDER BY namespace').fetchall()
                    sql.execute("CREATE TRIGGER force_failure BEFORE INSERT ON accounts BEGIN SELECT RAISE(ABORT,'injected write failure'); END")
                assert call(a,'auth.register',reg('Rollback',['QQ']))['error']['code']=='DATABASE_ERROR'
                with sqlite3.connect(db) as sql:
                    assert sql.execute("select count(*) from users where login_name='rollback'").fetchone()[0]==0
                    assert sql.execute('select count(*) from registration_requests').fetchone()[0]==4
                    assert sql.execute('SELECT namespace,next_value FROM id_counters ORDER BY namespace').fetchall()==counters_before
                    sql.execute('DROP TRIGGER force_failure')
                print('T07 T12 PASS: injected SQL failure rolls back user/accounts/counter/request; constraints')
                a.close(); b.close(); c.close(); stop(p)
                p=start(args.server,db,port,log)
                with connect(port) as s:
                    assert call(s,'auth.login',dict(login_name='bob',password='Mvp1-Pass-2026!'))['success']
                    assert call(s,'auth.register',reg('Dave',['QQ','WEIBO']),rid)['success']
                # Three simultaneous registrations with one idempotency key must
                # commit exactly once, even across separate connections.
                shared_id=str(uuid.uuid4())
                def concurrent_register(_):
                    with connect(port) as connection:
                        return call(connection,'auth.register',reg('Concurrent',['QQ','WECHAT','WEIBO']),shared_id)
                with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                    replies=list(pool.map(concurrent_register,range(3)))
                assert all(x['success'] for x in replies)
                assert len({x['data']['user_id'] for x in replies})==1
                with sqlite3.connect(db) as sql:
                    assert sql.execute('select count(*) from users').fetchone()[0]==5
                    assert sql.execute('select count(*) from registration_requests').fetchone()[0]==5
                    assert 'Mvp1-Pass-2026!' not in str(sql.execute('select password_hash,request_fingerprint from users join registration_requests using(user_id)').fetchall())
                with connect(port) as connection:
                    assert call(connection,'account.services.list',{'session_id':'ignored'})['error']['code']=='AUTH_REQUIRED'
                    assert call(connection,'system.ping',rid='invalid')['error']['code']=='PROTOCOL_ERROR'
                # A second server cannot own the same DB, even on another port.
                with socket.socket() as spare:
                    spare.bind(('127.0.0.1',0)); otherport=spare.getsockname()[1]
                second=subprocess.run([args.server,'--db',str(db),'--port',str(otherport)],capture_output=True,timeout=5)
                assert second.returncode!=0 and b'already owned' in second.stderr
                stop(p)
                print('Concurrent registration/replay, connection-bound sessions, exclusive DB ownership PASS')
                print('T06 T14 PASS: restart preserves users and idempotency')
            finally:
                if p.poll() is None: p.terminate(); p.wait(timeout=10)
        text=logfile.read_text(); assert 'Mvp1-Pass-2026!' not in text and '"password"' not in text
        print('T13 PASS: server logs contain no test passwords/auth bodies')
        print('ALL SERVICE/PROTOCOL ACCEPTANCE CHECKS PASSED')
if __name__=='__main__': main()
