#!/usr/bin/env python3
"""Real TCP/SQLite MVP4 acceptance. Every selected test owns a disposable database.
GUI M4-T01/T21 and Qt portions of T13/T15/T16 require separate native evidence.
"""
import argparse
import concurrent.futures
import json
import socket
import signal
import sqlite3
import struct
import tempfile
import time
import traceback
import uuid
from pathlib import Path
from mvp2_helpers import Server, ok, error, befriend, no_private
from mvp3_acceptance import Acceptance as GroupFixtures


class Acceptance(GroupFixtures):
    def private(self, a, b, bi, service='QQ'):
        befriend(a, b, bi[service], service)
        return ok(a.call('chat.private.open', dict(service_type=service, target_display_id=bi[service])))['conversation_id']

    def send_private(self, a, bi, content='hello', service='QQ', rid=None):
        return ok(a.call('chat.private.send', dict(service_type=service, target_display_id=bi[service], content=content), rid))

    def send_group(self, a, g, content='group', sub=None, rid=None):
        data = dict(group_pk=g, content=content)
        if sub is not None: data['subgroup_pk'] = sub
        return ok(a.call('chat.group.send', data, rid))

    def history(self, c, conv, **kwargs):
        result = ok(c.call('chat.history.get', dict(conversation_id=conv, **kwargs)))
        ids = [m['message_id'] for m in result['items']]
        assert ids == sorted(set(ids)), result
        assert len(ids) <= kwargs.get('limit', 50), result
        no_private(result)
        return result

    def ids(self, c, conv, **kwargs):
        return [m['message_id'] for m in self.history(c, conv, **kwargs)['items']]

    def denied_chat(self, c, kind, data, codes=None):
        result = c.call(kind, data)
        assert not result['success'], result
        if codes: assert result['error']['code'] in codes, result
        return result

    def new_events(self, c, mark):
        self.drain(c)
        return [e['data'] for e in c.events[mark:] if e['type'] == 'chat.message.new']

    def test02(self):
        a, ai, an = self.user(); b, bi, bn = self.user()
        conv = self.private(a,b,bi); mark = self.drain(b)
        msg = self.send_private(a,bi,'Alice → Bob 中文🙂')
        assert msg['conversation_id'] == conv and msg['status'] == 'stored'
        assert msg['sender_display_id'] == ai['QQ'] and msg['content_type'] == 'TEXT'
        event = b.event('chat.message.new', mark)['data']; assert event == msg, (event,msg)
        assert [x['message_id'] for x in self.new_events(b,mark)] == [msg['message_id']]
        assert self.sql('SELECT content FROM messages WHERE message_pk=?',(msg['message_id'],)) == [(msg['content'],)]
        no_private(msg)
        reverse = ok(b.call('chat.private.open',dict(service_type='QQ',target_display_id=ai['QQ'])))
        assert reverse['conversation_id'] == conv

    def test03(self):
        a,ai,_=self.user(); b,bi,_=self.user(); conv=self.private(a,b,bi,'WECHAT')
        mark=self.drain(b); msg=self.send_private(a,bi,'微信🙂','WECHAT')
        assert b.event('chat.message.new',mark)['data']['message_id']==msg['message_id']
        assert self.ids(b,conv)==[msg['message_id']]
        for kind in ('chat.private.open','chat.private.send'):
            self.denied_chat(a,kind,dict(service_type='WEIBO',target_display_id=bi['WEIBO'],content='no'))

    def test04(self):
        a,ai,an=self.user(); b,bi,_=self.user()
        self.denied_chat(a,'chat.private.send',dict(service_type='QQ',target_display_id=bi['QQ'],content='no'),('CHAT_NOT_FRIENDS',))
        self.private(a,b,bi)
        inactive=self.s.login(an,['WECHAT']); self.denied_chat(inactive,'chat.private.send',dict(service_type='QQ',target_display_id=bi['QQ'],content='no'),('SERVICE_NOT_ACTIVE',))
        self.denied_chat(a,'chat.private.send',dict(service_type='QQ',target_display_id=bi['WECHAT'],content='cross'))
        for field in ('sender_id','from_account','sender_account_pk'):
            self.denied_chat(a,'chat.private.send',dict(service_type='QQ',target_display_id=bi['QQ'],content='forged',**{field:bi['QQ']}),('VALIDATION_ERROR',))
        anon=self.s.client(); error(anon.call('chat.sync',dict(service_type='QQ',after_message_id=0)),'AUTH_REQUIRED')
        assert self.sql('SELECT count(*) FROM messages')==[(0,)]

    def test05(self):
        for service in ('QQ','WECHAT'):
            a,ai,_=self.user(); b,bi,_=self.user(); outsider,oi,_=self.user(); g=self.create(a,service)
            if service=='QQ': self.join(a,b,g)
            else: self.respond(b,self.mutation(a,'group.invite',group_pk=g,target_display_id=bi[service])['request_pk'])
            marks=[self.drain(x) for x in (b,outsider)]; msg=self.send_group(a,g)
            assert b.event('chat.message.new',marks[0])['data']['message_id']==msg['message_id']
            assert [m['message_id'] for m in self.new_events(b,marks[0])]==[msg['message_id']]
            assert self.new_events(outsider,marks[1])==[]
            self.denied_chat(outsider,'chat.group.send',dict(group_pk=g,content='no'),('CHAT_NOT_MEMBER',))
            self.denied_chat(outsider,'chat.history.get',dict(conversation_id=msg['conversation_id']))

    def test06(self):
        a,ai,b,bi,c,ci,g=self.setup(); sub=self.subgroup(a,g,[bi['QQ']]); marks=[self.drain(x) for x in (b,c)]
        msg=self.send_group(a,g,sub=sub); assert b.event('chat.message.new',marks[0])['data']['message_id']==msg['message_id']
        assert self.new_events(c,marks[1])==[]
        self.denied_chat(c,'chat.group.send',dict(group_pk=g,subgroup_pk=sub,content='no'),('CHAT_NOT_MEMBER',))
        self.denied_chat(c,'chat.history.get',dict(conversation_id=msg['conversation_id']))

    def test07(self):
        a,ai,b,bi,c,ci,g=self.setup(); sub=self.subgroup(a,g,[bi['QQ']]); msg=self.send_group(a,g,sub=sub)
        members=self.sql('SELECT * FROM subgroup_members WHERE subgroup_pk=?',(sub,)); self.switch(a,g,'WECHAT')
        self.denied_chat(b,'chat.group.send',dict(group_pk=g,subgroup_pk=sub,content='disabled'),('CHAT_POLICY_DISABLED',))
        self.denied_chat(b,'chat.history.get',dict(conversation_id=msg['conversation_id']),('CHAT_POLICY_DISABLED',))
        assert self.sql('SELECT * FROM subgroup_members WHERE subgroup_pk=?',(sub,))==members
        main=self.send_group(b,g,'main still QQ'); assert main['service_type']=='QQ'
        self.switch(a,g,'QQ'); assert self.ids(b,msg['conversation_id'])==[msg['message_id']]
        self.send_group(b,g,sub=sub)
        # Original WECHAT service never gains QQ subgroup chat through a policy switch.
        w=self.create(a,'WECHAT'); self.switch(a,w,'QQ'); ws=self.subgroup(a,w,[])
        self.denied_chat(a,'chat.group.send',dict(group_pk=w,subgroup_pk=ws,content='no'))

    def test08(self):
        a,ai,b,bi,c,ci,g=self.setup(); sub=self.subgroup(a,g,[bi['QQ']]); old=self.send_group(a,g,'old'); sold=self.send_group(a,g,'sub-old',sub)
        self.mutation(b,'group.member.leave',group_pk=g)
        gap=self.send_group(a,g,'gap'); sgap=self.send_group(a,g,'sub-gap',sub)
        assert self.ids(b,old['conversation_id'])==[old['message_id']]
        assert self.ids(b,sold['conversation_id'])==[sold['message_id']]
        self.denied_chat(b,'chat.group.send',dict(group_pk=g,content='no'),('CHAT_NOT_MEMBER',))
        self.join(a,b,g); self.mutation(a,'group.subgroup.members.update',subgroup_pk=sub,add_display_ids=[bi['QQ']])
        new=self.send_group(a,g,'new'); snew=self.send_group(a,g,'sub-new',sub)
        # Collapse second-resolution metadata after real TCP leave/rejoin actions.
        # IDs and generated epoch boundaries stay untouched; timestamps cannot hide a gap.
        with sqlite3.connect(self.s.db) as db:
            db.execute("UPDATE conversation_members SET joined_at='2026-10-22T00:00:00Z',left_at=CASE WHEN left_at IS NULL THEN NULL ELSE '2026-10-22T00:00:00Z' END WHERE conversation_pk IN (?,?)",(old['conversation_id'],sold['conversation_id']))
            db.execute("UPDATE messages SET created_at='2026-10-22T00:00:00Z' WHERE conversation_pk IN (?,?)",(old['conversation_id'],sold['conversation_id']))
        assert self.ids(b,old['conversation_id'])==[old['message_id'],new['message_id']]
        assert self.ids(b,sold['conversation_id'])==[sold['message_id'],snew['message_id']]
        self.mutation(a,'group.member.kick',group_pk=g,target_display_id=bi['QQ']); future=self.send_group(a,g,'future')
        assert future['message_id'] not in self.ids(b,old['conversation_id'])
        synced=ok(b.call('chat.sync',dict(service_type='QQ',after_message_id=0)))['items']
        assert not {gap['message_id'],sgap['message_id'],future['message_id']} & {m['message_id'] for m in synced}
        self.mutation(a,'group.disband',group_pk=g)
        self.denied_chat(a,'chat.group.send',dict(group_pk=g,content='disbanded'))
        assert self.ids(b,old['conversation_id'])==[old['message_id'],new['message_id']]
        epochs=self.sql('SELECT joined_after_message_id,left_at_message_id FROM conversation_members WHERE conversation_pk=? ORDER BY joined_after_message_id',(old['conversation_id'],))
        assert any(end is not None for start,end in epochs),epochs

    def test09(self):
        a,ai,_=self.user(); b,bi,bn=self.user(); conv=self.private(a,b,bi); b.close()
        expected=[self.send_private(a,bi,'offline %d'%i)['message_id'] for i in range(63)]
        b=self.s.login(bn); ids=[]; cursor=0
        while True:
            page=ok(b.call('chat.sync',dict(service_type='QQ',after_message_id=cursor,limit=17)))
            found=[m['message_id'] for m in page['items']]; assert len(found)<=17 and found==sorted(found)
            ids.extend(found); assert page['next_cursor']>=cursor
            cursor=page['next_cursor']
            if not page['has_more']: break
            assert found
        assert ids==expected and self.ids(b,conv,after_message_id=expected[-1])==[]
        assert ok(b.call('chat.sync',dict(service_type='WECHAT',after_message_id=0)))['items']==[]

    def test10(self):
        a,ai,an=self.user(); b,bi,bn=self.user(); conv=self.private(a,b,bi)
        expected=[self.send_private(a,bi,'page %d'%i)['message_id'] for i in range(217)]
        self.s.stop(); self.s.start(); b=self.s.login(bn); found=[]; before=None
        while True:
            page=self.history(b,conv,limit=29,**({} if before is None else {'before_message_id':before}))
            part=[m['message_id'] for m in page['items']]; found=part+found
            if not page['has_more']: break
            assert part; before=part[0]
        assert found==expected
        a=self.s.login(an); assert ok(a.call('chat.private.open',dict(service_type='QQ',target_display_id=bi['QQ'])))['conversation_id']==conv
        assert self.sql('PRAGMA foreign_key_check')==[]

    @staticmethod
    def frame(kind,data,rid):
        raw=json.dumps(dict(kind='request',type=kind,request_id=rid,data=data),ensure_ascii=False).encode()
        return struct.pack('!I',len(raw))+raw

    def test11(self):
        a,ai,_=self.user(); b,bi,_=self.user(); self.private(a,b,bi)
        ids=[str(uuid.uuid4()) for _ in range(6)]
        frames=[self.frame('chat.private.send',dict(service_type='QQ',target_display_id=bi['QQ'],content='split🙂%d'%i),rid) for i,rid in enumerate(ids)]
        # Prefix and UTF8 body deliberately split into single-byte TCP writes.
        for byte in frames[0]: a.socket.sendall(bytes([byte]))
        a.socket.sendall(b''.join(frames[1:]))
        deadline=time.monotonic()+15
        while not set(ids)<=a.responses.keys(): a.receive(max(.01,deadline-time.monotonic()))
        messages=[ok(a.responses.pop(rid)) for rid in ids]
        assert len({m['message_id'] for m in messages})==6
        assert self.sql('SELECT count(*) FROM messages')==[(6,)]

    def test12(self):
        a,ai,an=self.user(); b,bi,_=self.user(); conv=self.private(a,b,bi); rid=str(uuid.uuid4())
        payload=dict(service_type='QQ',target_display_id=bi['QQ'],content='one only'); saved=a.call('chat.private.send',payload,rid); ok(saved)
        mark=self.drain(b); assert a.call('chat.private.send',payload,rid)==saved
        assert self.new_events(b,mark)==[]
        error(a.call('chat.private.send',dict(payload,content='changed'),rid),'REQUEST_ID_CONFLICT')
        self.mutation(a,'friend.remove',service_type='QQ',target_display_id=bi['QQ'])
        assert a.call('chat.private.send',payload,rid)==saved
        self.denied_chat(a,'chat.private.send',payload,('CHAT_NOT_FRIENDS',))
        assert self.ids(a,conv)==[saved['data']['message_id']]
        self.s.stop(); self.s.start(); a=self.s.login(an)
        assert a.call('chat.private.send',payload,rid)==saved
        inactive=self.s.login(an,['WECHAT']); error(inactive.call('chat.private.send',payload,rid),'SERVICE_NOT_ACTIVE')
        anon=self.s.client(); error(anon.call('chat.private.send',payload,rid),'AUTH_REQUIRED')
        assert self.sql('SELECT count(*) FROM messages')==[(1,)]
        # Successful group retry survives permission loss too.
        g=self.create(a); self.join(a,b:=self.s.login('m3user2'),g); gr=str(uuid.uuid4()); gp=dict(group_pk=g,content='group retry')
        gs=b.call('chat.group.send',gp,gr); ok(gs); self.mutation(b,'group.member.leave',group_pk=g)
        assert b.call('chat.group.send',gp,gr)==gs

    def test13(self):
        a,ai,_=self.user(); b,bi,_=self.user(); self.private(a,b,bi); mark=self.drain(b)
        before={t:self.sql('SELECT * FROM '+t) for t in ('messages','message_receipts','mutation_requests')}
        self.execute("CREATE TRIGGER fail_m4 BEFORE INSERT ON message_receipts BEGIN SELECT RAISE(ABORT,'injected receipt failure'); END")
        rid=str(uuid.uuid4()); payload=dict(service_type='QQ',target_display_id=bi['QQ'],content='rollback')
        try: error(a.call('chat.private.send',payload,rid),'DATABASE_ERROR')
        finally: self.execute('DROP TRIGGER fail_m4')
        assert {t:self.sql('SELECT * FROM '+t) for t in before}==before
        assert self.new_events(b,mark)==[]
        ok(a.call('chat.private.send',payload,rid))

    def test14(self):
        a,ai,an=self.user(); b,bi,_=self.user(); self.private(a,b,bi); rid=str(uuid.uuid4()); payload=dict(service_type='QQ',target_display_id=bi['QQ'],content='lost response')
        a.socket.sendall(self.frame('chat.private.send',payload,rid))
        # The independent receiver event proves COMMIT; sender response is never read.
        event=b.event('chat.message.new')['data']; a.close()
        self.s.stop(); self.s.start(); a=self.s.login(an)
        replay=ok(a.call('chat.private.send',payload,rid)); assert replay['message_id']==event['message_id']
        assert self.sql('SELECT count(*) FROM messages WHERE request_id=?',(rid,))==[(1,)]

    def test15(self):
        a,ai,b,bi,c,ci,g=self.setup(); self.private(a,b,bi); self.private(c,b,bi)
        def work(client,index):
            return [self.send_group(client,g,'concurrent %d %d'%(index,i))['message_id'] if i%2 else self.send_private(client,bi,'private %d %d'%(index,i))['message_id'] for i in range(40)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(work,x,i) for i,x in enumerate((a,c))]; ids=sum([f.result(timeout=30) for f in futures],[])
        assert len(set(ids))==80 and self.sql('SELECT count(*) FROM messages')==[(80,)]
        assert self.sql('PRAGMA foreign_key_check')==[]; ok(b.call('system.ping'))

    def test16(self):
        a,ai,an=self.user(); b,bi,bn=self.user(); conv=self.private(a,b,bi); old=self.send_private(a,bi)
        self.s.stop(); self.s.start(); a=self.s.login(an); new=self.send_private(a,bi,'after restart'); b=self.s.login(bn)
        page=ok(b.call('chat.sync',dict(service_type='QQ',after_message_id=old['message_id'])))
        assert [m['message_id'] for m in page['items']]==[new['message_id']]
        assert self.ids(b,conv)==[old['message_id'],new['message_id']]

    def test17(self):
        a,ai,an=self.user(); b,bi,_=self.user(); c,ci,_=self.user(); conv=self.private(a,b,bi); self.send_private(a,bi)
        self.denied_chat(c,'chat.history.get',dict(conversation_id=conv))
        inactive=self.s.login(an,['WECHAT']); error(inactive.call('chat.history.get',dict(conversation_id=conv)),'SERVICE_NOT_ACTIVE')
        for data in (dict(before_message_id=-1),dict(after_message_id=-1),dict(before_message_id=1,after_message_id=0),dict(limit=0),dict(limit=51),dict(before_message_id='1')):
            self.denied_chat(a,'chat.history.get',dict(conversation_id=conv,**data),('CHAT_CURSOR_INVALID','VALIDATION_ERROR'))
        for cursor in ('-1','bad','1.5'):
            self.denied_chat(a,'chat.conversation.list',dict(service_type='QQ',cursor=cursor),('CHAT_CURSOR_INVALID','VALIDATION_ERROR'))
        for cursor in (-1,'0'):
            self.denied_chat(a,'chat.sync',dict(service_type='QQ',after_message_id=cursor),('CHAT_CURSOR_INVALID','VALIDATION_ERROR'))
        page=ok(a.call('chat.conversation.list',dict(service_type='QQ',limit=1)))
        assert len(page['items'])==1 and page['items'][0]['conversation_id']==conv

    def test18(self):
        a,ai,_=self.user(); b,bi,bn=self.user(); c,ci,_=self.user(); conv=self.private(a,b,bi); mark=self.drain(a); msg=self.send_private(a,bi)
        self.drain(a); assert not [e for e in a.events[mark:] if e['type']=='chat.message.delivered']
        assert self.sql('SELECT delivered_at FROM message_receipts WHERE message_pk=?',(msg['message_id'],))==[(None,)]
        for client in (a,c): self.denied_chat(client,'chat.message.ack',dict(message_id=msg['message_id']))
        inactive=self.s.login(bn,['WECHAT']); self.denied_chat(inactive,'chat.message.ack',dict(message_id=msg['message_id']),('SERVICE_NOT_ACTIVE',))
        mark=self.drain(a); ack=ok(b.call('chat.message.ack',dict(message_id=msg['message_id'])))
        assert ack['status']=='delivered'; event=a.event('chat.message.delivered',mark)['data']
        assert event['message_id']==msg['message_id'] and event['recipient_display_id']==bi['QQ'] and event['status']=='delivered'
        assert self.sql('SELECT delivered_at FROM message_receipts WHERE message_pk=?',(msg['message_id'],))[0][0]
        assert 'read' not in json.dumps(event)
        again=ok(b.call('chat.message.ack',dict(message_id=msg['message_id']))); assert again==ack
        self.denied_chat(b,'chat.message.read',dict(message_id=msg['message_id']))

    def test19(self):
        a,ai,_=self.user(); b,bi,_=self.user(); conv=self.private(a,b,bi)
        text='中English🙂\n'*200; msg=self.send_private(a,bi,text); assert self.history(b,conv)['items'][0]['content']==text
        self.send_private(a,bi,'x'*4096)
        for content,code in [('x'*4097,'CHAT_MESSAGE_TOO_LARGE'),('中'*1366,'CHAT_MESSAGE_TOO_LARGE'),('', 'CHAT_INVALID_CONTENT'),(' \t\n','CHAT_INVALID_CONTENT'),('abc\x00def','CHAT_INVALID_CONTENT')]:
            error(a.call('chat.private.send',dict(service_type='QQ',target_display_id=bi['QQ'],content=content)),code)
        for raw in (b'{broken',b'{"kind":"request","type":"system.ping","request_id":"badutf8","data":{"x":"\xff"}}'):
            malformed=self.s.client(); malformed.socket.sendall(struct.pack('!I',len(raw))+raw)
            try:
                response=malformed.receive(2); assert not response.get('success',False),response
            except (EOFError,ConnectionResetError): pass
        oversize=self.s.client(); oversize.socket.sendall(struct.pack('!I',65537))
        try: assert oversize.socket.recv(1)==b''
        except ConnectionResetError: pass
        ok(a.call('system.ping')); assert self.sql('SELECT count(*) FROM messages')==[(2,)]
        # Large valid texts must paginate by the encoded frame budget as well as row limit.
        for i in range(50): self.send_private(a,bi,('"'*1800)+('\\'*1000)+('🙂'*300))
        expected=[row[0] for row in self.sql('SELECT message_pk FROM messages ORDER BY message_pk')]
        collected=[]; before=None
        while True:
            page=self.history(b,conv,limit=50,**({} if before is None else {'before_message_id':before}))
            part=[m['message_id'] for m in page['items']]; assert part
            collected=part+collected
            if not page['has_more']: break
            before=part[0]
        assert collected==expected
        collected=[]; cursor=0
        while True:
            page=ok(b.call('chat.sync',dict(service_type='QQ',after_message_id=cursor,limit=50)))
            part=[m['message_id'] for m in page['items']]; collected.extend(part)
            if not page['has_more']: break
            assert part and page['next_cursor']>cursor; cursor=page['next_cursor']
        assert collected==expected

    def test20(self):
        a,ai,an=self.user(); b,bi,_=self.user(); befriend(a,b,bi['QQ']); g=self.create(a); self.join(a,b,g); self.admin(a,g,bi['QQ']); sub=self.subgroup(a,g,[bi['QQ']]); self.switch(a,g,'WECHAT')
        root=Path(__file__).resolve().parents[1]; legacy=self.directory/'legacy-v3.sqlite3'
        # Build a genuine version3 schema from the original migrations, not v4 relabeled.
        with sqlite3.connect(legacy) as db:
            db.executescript((root/'database/schema.sql').read_text())
            for number in ('002','003'): db.executescript(next((root/'database/migrations').glob(number+'*')).read_text())
            tables=[x[0] for x in db.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")]
            snapshot={table:self.sql('SELECT * FROM '+table) for table in tables}
            db.execute('PRAGMA foreign_keys=OFF')
            triggers=list(db.execute("SELECT name,sql FROM sqlite_master WHERE type='trigger'"))
            for name,sql in triggers: db.execute('DROP TRIGGER '+name)
            for table in tables: db.execute('DELETE FROM '+table)
            for table,records in snapshot.items():
                for record in records: db.execute('INSERT INTO '+table+' VALUES('+','.join('?' for _ in record)+')',record)
            for name,sql in triggers: db.execute(sql)
            assert db.execute('PRAGMA user_version').fetchone()==(3,)
        migrated=Server(self.s.exe,legacy)
        try:
            migrated.start()
            with sqlite3.connect(legacy) as db:
                assert db.execute('PRAGMA user_version').fetchone()==(4,)
                for table,records in snapshot.items(): assert db.execute('SELECT * FROM '+table).fetchall()==records,table
                assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
                assert db.execute('PRAGMA quick_check').fetchone()==('ok',)
            backups=list(self.directory.glob('legacy-v3.sqlite3*v3*')); assert backups,'no schema3 backup'
            assert any(sqlite3.connect(path).execute('PRAGMA user_version').fetchone()==(3,) for path in backups)
            old=migrated.login(an); assert self.detail(old,g)['group']['policy_type']=='WECHAT'
            self.switch(old,g,'QQ'); assert self.submembers(old,g,sub)=={ai['QQ'],bi['QQ']}
            # Existing memberships are seeded and immediately usable after migration.
            assert ok(old.call('chat.group.send',dict(group_pk=g,subgroup_pk=sub,content='migrated')))['status']=='stored'
        finally: migrated.close()

    def test22(self):
        a,ai,an=self.user(); b,bi,bn=self.user(); conv=self.private(a,b,bi)
        slow=self.s.login(bn); slow.socket.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024)
        # Send > kernel receive/send capacity with a receiver which never reads.
        start=time.monotonic()
        for batch in range(250):
            requests=[str(uuid.uuid4()) for _ in range(6)]
            slow_frames=[self.frame('chat.private.send',dict(service_type='QQ',target_display_id=bi['QQ'],content=('%04d'%(batch*6+i))+'x'*4000),rid) for i,rid in enumerate(requests)]
            a.socket.sendall(b''.join(slow_frames))
            while not set(requests)<=a.responses.keys(): a.receive(8)
            for rid in requests: ok(a.responses.pop(rid))
        assert time.monotonic()-start<45,'DB worker blocked on slow peer'
        healthy=self.s.login(bn); page=self.history(healthy,conv,limit=1); assert len(page['items'])==1
        # Drain the intentionally stalled connection after saturation: EOF proves eviction.
        slow.socket.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1048576)
        slow.socket.settimeout(3); closed=False; total=0
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            try:
                block=slow.socket.recv(65536)
                if not block: closed=True; break
                total+=len(block)
            except ConnectionResetError: closed=True; break
        assert closed,('slow receiver was not disconnected',total)
        assert self.sql('SELECT count(*) FROM messages')==[(1500,)]
        ok(healthy.call('system.ping'))
        # Terminate with active sockets and queued frames, before fixture cleanup.
        healthy.socket.sendall(b''.join(self.frame('system.ping',{},str(uuid.uuid4())) for _ in range(6)))
        started=time.monotonic(); self.s.process.send_signal(signal.SIGTERM)
        self.s.process.wait(timeout=10)
        assert self.s.process.returncode==0 and time.monotonic()-started<10
        self.s.stop()


def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--server',required=True); parser.add_argument('--only'); args=parser.parse_args()
    supported=list(range(2,21))+[22]; selected=sorted(set(map(int,args.only.split(',')))) if args.only else supported
    assert set(selected)<=set(supported),'T01/T21 require independent native Qt evidence'
    failures=[]
    for number in selected:
        with tempfile.TemporaryDirectory(prefix='im-mvp4-t%02d-'%number) as directory:
            server=Server(args.server,Path(directory)/'acceptance.sqlite3')
            try:
                server.start(); getattr(Acceptance(server,directory),'test%02d'%number)()
                print('M4-T%02d PASS (real TCP / temporary SQLite)'%number,flush=True)
            except Exception as exc:
                failures.append(number); print('M4-T%02d FAIL: %s: %s'%(number,type(exc).__name__,exc),flush=True); traceback.print_exc()
            finally:
                try: server.close()
                except Exception:
                    if number not in failures: failures.append(number)
                    traceback.print_exc()
    print('M4-T01/T21 NOT RUN here: independent native Qt processes / Qt Creator evidence is separate.')
    print('T13/T15/T16 scope here: server failure/concurrency/restart only; GUI states need native evidence.')
    print('%d passed; %d failed; %d not selected'%(len(selected)-len(failures),len(failures),len(supported)-len(selected)),flush=True)
    return bool(failures)

if __name__=='__main__': raise SystemExit(main())
