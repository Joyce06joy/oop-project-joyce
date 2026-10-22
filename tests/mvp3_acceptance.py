#!/usr/bin/env python3
"""Independent MVP3 real TCP acceptance; disposable databases only.
Run: python3 tests/mvp3_acceptance.py --server build/mvp3/IMServer
T18 tests server rollback, not GUI error rendering. T22 is separately verified GUI work.
Dynamic virtual dispatch is additionally verified by the domain test executable.
"""
import argparse
import concurrent.futures
import socket
import sqlite3
import tempfile
import traceback
import uuid
from pathlib import Path
from mvp2_helpers import Server, ok, error, rows, no_private, registration, befriend

class Acceptance:
    def __init__(self, server, directory):
        self.s, self.directory = server, Path(directory)
        self.seq = 0
    def user(self, services=None):
        self.seq += 1
        name = 'm3user%d' % self.seq
        c, ids = self.s.user(name, services)
        return c, ids, name
    def sql(self, query, args=()):
        with sqlite3.connect(self.s.db) as db:
            return db.execute(query, args).fetchall()
    def execute(self, statement):
        with sqlite3.connect(self.s.db) as db: db.execute(statement)
    def create(self, c, service='QQ', name='Acceptance'):
        return ok(c.call('group.create', dict(service_type=service, name=name)))['group_pk']
    def detail(self, c, g):
        result = ok(c.call('group.detail', dict(group_pk=g)))
        no_private(result)
        return result
    def members(self, c, g):
        return {x['display_id']: x for x in self.detail(c,g)['members']}
    def mutation(self, c, kind, **data): return ok(c.call(kind, data))
    def apply(self, c, g): return self.mutation(c,'group.join.apply',group_pk=g)['request_pk']
    def respond(self, c, r, decision='ACCEPT'):
        return self.mutation(c,'group.request.respond',request_pk=r,decision=decision)
    def join(self, owner, member, g): return self.respond(owner,self.apply(member,g))
    def admin(self,c,g,target,enabled=True):
        return self.mutation(c,'group.admin.set',group_pk=g,target_display_id=target,enabled=enabled)
    def switch(self,c,g,policy):
        v=self.detail(c,g)['group']['policy_version']
        return self.mutation(c,'group.policy.switch',group_pk=g,target_policy=policy,expected_policy_version=v)
    def setup(self):
        a,ai,an=self.user(); b,bi,bn=self.user(); c,ci,cn=self.user()
        g=self.create(a); self.join(a,b,g); self.join(a,c,g)
        return a,ai,b,bi,c,ci,g
    def denied(self,c,kind,data,codes=('GROUP_PERMISSION_DENIED','GROUP_POLICY_DISABLED','GROUP_NOT_MEMBER')):
        response=c.call(kind,data)
        assert not response['success'] and response['error']['code'] in codes,response
    def drain(self,c):
        ok(c.call('system.ping'))
        return len(c.events)
    def no_event(self,c,mark,g):
        self.drain(c)
        assert not [e for e in c.events[mark:] if e.get('data',{}).get('group_pk')==g], c.events[mark:]
    def subgroup(self,a,g,ids):
        return self.mutation(a,'group.subgroup.create',group_pk=g,name='Subset',member_display_ids=ids)['subgroup_pk']
    def submembers(self,a,g,s):
        return {x['display_id'] for x in rows(a,'group.subgroup.list',group_pk=g,subgroup_pk=s)['items']}
    def test01(self):
        a,ai,_=self.user(); g=self.create(a); h=self.create(a)
        d=self.detail(a,g); assert g!=h and d['group']['group_number']!=self.detail(a,h)['group']['group_number']
        assert set(self.members(a,g))=={ai['QQ']} and self.members(a,g)[ai['QQ']]['stored_role']=='OWNER'
        assert self.sql('SELECT count(*) FROM groups WHERE group_pk IN (?,?)',(g,h))==[(2,)]
    def test02(self):
        a,ai,_=self.user(); g=self.create(a,'WECHAT')
        assert set(self.members(a,g))=={ai['WECHAT']}
        assert self.detail(a,g)['group']['policy_type']=='WECHAT'
        assert not a.call('group.create',dict(service_type='WEIBO',name='Forbidden'))['success']
        self.switch(a,g,'QQ'); b,bi,_=self.user(['WECHAT']); self.join(a,b,g)
        assert self.detail(a,g)['group']['service_type']=='WECHAT'
        assert set(self.members(a,g))=={ai['WECHAT'],bi['WECHAT']}
        s=self.subgroup(a,g,[bi['WECHAT']]); assert self.submembers(a,g,s)=={ai['WECHAT'],bi['WECHAT']}
    def test03(self):
        a,ai,_=self.user(); b,bi,_=self.user(); g=self.create(a); r=self.apply(b,g)
        error(b.call('group.join.apply',dict(group_pk=g)),'GROUP_REQUEST_PENDING')
        self.respond(a,r); error(a.call('group.request.respond',dict(request_pk=r,decision='ACCEPT')),'GROUP_REQUEST_ALREADY_RESOLVED')
        assert set(self.members(a,g))=={ai['QQ'],bi['QQ']}
    def test04(self):
        a,ai,b,bi,c,ci,g=self.setup(); self.admin(a,g,bi['QQ'])
        d,di,_=self.user(); r=self.apply(d,g)
        self.denied(c,'group.request.respond',dict(request_pk=r,decision='ACCEPT',actor_role='OWNER'))
        self.respond(b,r); e,ei,_=self.user(); self.respond(b,self.apply(e,g),'REJECT')
        assert di['QQ'] in self.members(a,g) and ei['QQ'] not in self.members(a,g)
    def wechat(self):
        a,ai,_=self.user(); b,bi,_=self.user(); g=self.create(a,'WECHAT')
        r=self.mutation(a,'group.invite',group_pk=g,target_display_id=bi['WECHAT'])['request_pk']; self.respond(b,r)
        return a,ai,b,bi,g
    def test05(self):
        a,ai,b,bi,g=self.wechat(); c,ci,_=self.user(); d,di,_=self.user()
        r=self.mutation(b,'group.invite',group_pk=g,target_display_id=ci['WECHAT'])['request_pk']
        self.denied(b,'group.request.respond',dict(request_pk=r,decision='ACCEPT'))
        self.respond(c,r); r=self.mutation(b,'group.invite',group_pk=g,target_display_id=di['WECHAT'])['request_pk']; self.respond(d,r,'REJECT')
        assert ci['WECHAT'] in self.members(a,g) and di['WECHAT'] not in self.members(a,g)
    def test06(self):
        a,ai,b,bi,g=self.wechat(); c,ci,_=self.user()
        self.denied(c,'group.join.apply',dict(group_pk=g)); self.denied(b,'group.member.kick',dict(group_pk=g,target_display_id=ai['WECHAT']))
    def test07(self):
        a,ai,b,bi,c,ci,g=self.setup(); self.admin(a,g,bi['QQ'])
        assert self.members(a,g)[bi['QQ']]['stored_role']=='ADMIN'
        self.denied(b,'group.admin.set',dict(group_pk=g,target_display_id=ci['QQ'],enabled=True))
        self.denied(a,'group.admin.set',dict(group_pk=g,target_display_id=ai['QQ'],enabled=True),('GROUP_INVALID_TARGET','GROUP_PERMISSION_DENIED'))
        self.admin(a,g,bi['QQ'],False); assert self.members(a,g)[bi['QQ']]['stored_role']=='MEMBER'
    def test08(self):
        a,ai,b,bi,c,ci,g=self.setup(); self.admin(a,g,bi['QQ']); self.admin(a,g,ci['QQ'])
        for target in (ai['QQ'],ci['QQ']): self.denied(b,'group.member.kick',dict(group_pk=g,target_display_id=target))
        d,di,_=self.user(); self.join(a,d,g); self.mutation(b,'group.member.kick',group_pk=g,target_display_id=di['QQ'])
        assert di['QQ'] not in self.members(a,g)
        self.mutation(a,'group.member.kick',group_pk=g,target_display_id=ci['QQ'])
    def test09(self):
        a,ai,b,bi,g=self.wechat()
        for kind,data in [('group.announcement.update',dict(announcement='forged')),('group.disband',{}),('group.admin.set',dict(target_display_id=bi['WECHAT'],enabled=True)),('group.name.update',dict(name='forged'))]:
            self.denied(b,kind,dict(group_pk=g,**data))
        self.mutation(a,'group.announcement.update',group_pk=g,announcement='authoritative')
        assert self.detail(a,g)['group']['announcement']=='authoritative'
        self.mutation(a,'group.member.kick',group_pk=g,target_display_id=bi['WECHAT'])
        self.mutation(a,'group.disband',group_pk=g)
    def test10(self):
        a,ai,_=self.user(); g=self.create(a)
        self.denied(a,'group.member.leave',dict(group_pk=g),('GROUP_PERMISSION_DENIED',))
        s=self.subgroup(a,g,[]); self.mutation(a,'group.disband',group_pk=g)
        error(a.call('group.announcement.update',dict(group_pk=g,announcement='bad')),'GROUP_DISBANDED')
        assert self.sql('SELECT status FROM subgroups WHERE subgroup_pk=?',(s,))==[('DISBANDED',)]
        # Pending outsiders must invalidate their authoritative request panels on disband.
        applicant,pi,_=self.user(); invited,ii,_=self.user(); unrelated,ui,_=self.user()
        for policy,outsider,target in [('QQ',applicant,pi['QQ']),('WECHAT',invited,ii['QQ'])]:
            pending_group=self.create(a)
            if policy=='QQ': request=self.apply(outsider,pending_group)
            else:
                self.switch(a,pending_group,'WECHAT')
                request=self.mutation(a,'group.invite',group_pk=pending_group,target_display_id=target)['request_pk']
            assert any(x['request_pk']==request for x in rows(outsider,'group.request.list',service_type='QQ')['items'])
            mark=self.drain(outsider); unrelated_mark=self.drain(unrelated)
            self.mutation(a,'group.disband',group_pk=pending_group)
            event=outsider.event('group.disbanded',mark); no_private(event)
            assert event['data']['group_pk']==pending_group
            assert not any(x['request_pk']==request for x in rows(outsider,'group.request.list',service_type='QQ')['items'])
            self.no_event(unrelated,unrelated_mark,pending_group)
    def test11(self):
        a,ai,b,bi,c,ci,g=self.setup(); d,di,_=self.user(); s=self.subgroup(a,g,[bi['QQ']])
        assert self.submembers(a,g,s)=={ai['QQ'],bi['QQ']}
        self.denied(a,'group.subgroup.members.update',dict(subgroup_pk=s,add_display_ids=[di['QQ']]),('GROUP_INVALID_TARGET',))
        self.denied(d,'group.subgroup.list',dict(group_pk=g,subgroup_pk=s))
        assert self.submembers(a,g,s)=={ai['QQ'],bi['QQ']}
    def test12(self):
        a,ai,b,bi,c,ci,g=self.setup(); s=self.subgroup(a,g,[bi['QQ'],ci['QQ']])
        self.mutation(b,'group.member.leave',group_pk=g); self.mutation(a,'group.member.kick',group_pk=g,target_display_id=ci['QQ'])
        assert self.submembers(a,g,s)=={ai['QQ']}
        assert self.sql('SELECT count(*) FROM subgroup_members WHERE subgroup_pk=?',(s,))==[(1,)]
    def policy_setup(self):
        a,ai,b,bi,c,ci,g=self.setup(); self.admin(a,g,bi['QQ']); s=self.subgroup(a,g,[bi['QQ']]); return a,ai,b,bi,c,ci,g,s
    def test13(self):
        a,ai,b,bi,c,ci,g,s=self.policy_setup(); before=self.members(a,g); saved=self.sql('SELECT * FROM subgroup_members WHERE subgroup_pk=?',(s,))
        d,di,_=self.user(); r=self.apply(d,g); self.switch(a,g,'WECHAT')
        assert set(self.members(a,g))==set(before) and self.members(a,g)[bi['QQ']]['stored_role']=='ADMIN'
        self.denied(b,'group.announcement.update',dict(group_pk=g,announcement='bad'))
        result=rows(a,'group.subgroup.list',group_pk=g,subgroup_pk=s); assert result['disabled'] and result['items']==[]
        self.denied(a,'group.subgroup.members.update',dict(subgroup_pk=s,add_display_ids=[ci['QQ']]))
        assert self.sql('SELECT * FROM subgroup_members WHERE subgroup_pk=?',(s,))==saved
        assert self.sql('SELECT status FROM group_requests WHERE request_pk=?',(r,))==[('CANCELLED_POLICY_CHANGED',)]
    def test14(self):
        a,ai,b,bi,c,ci,g,s=self.policy_setup(); before=self.submembers(a,g,s); d,di,_=self.user(); r=self.apply(d,g)
        self.switch(a,g,'WECHAT'); self.switch(a,g,'QQ')
        self.mutation(b,'group.announcement.update',group_pk=g,announcement='restored')
        assert self.submembers(a,g,s)==before
        error(a.call('group.request.respond',dict(request_pk=r,decision='ACCEPT')),'GROUP_REQUEST_ALREADY_RESOLVED')
        assert di['QQ'] not in self.members(a,g)
        self.switch(a,g,'WECHAT'); r=self.mutation(b,'group.invite',group_pk=g,target_display_id=di['QQ'])['request_pk']
        self.switch(a,g,'QQ'); self.switch(a,g,'WECHAT')
        error(d.call('group.request.respond',dict(request_pk=r,decision='ACCEPT')),'GROUP_REQUEST_ALREADY_RESOLVED')
    def test15(self):
        a,ai,b,bi,c,ci,g,s=self.policy_setup(); before=self.sql('SELECT * FROM subgroup_members WHERE subgroup_pk=?',(s,)); self.switch(a,g,'WECHAT')
        v=self.detail(a,g)['group']['policy_version']; rid=str(uuid.uuid4()); payload=dict(group_pk=g,announcement='restart replay')
        saved=a.call('group.announcement.update',payload,rid); ok(saved); self.s.stop(); self.s.start()
        a=self.s.login('m3user1'); b=self.s.login('m3user2')
        assert a.call('group.announcement.update',payload,rid)==saved
        assert self.detail(a,g)['group']['policy_version']==v and self.detail(a,g)['group']['policy_type']=='WECHAT'
        assert self.members(a,g)[bi['QQ']]['stored_role']=='ADMIN'
        self.denied(b,'group.announcement.update',dict(group_pk=g,announcement='bad'))
        assert self.sql('SELECT * FROM subgroup_members WHERE subgroup_pk=?',(s,))==before
        self.switch(a,g,'QQ'); assert self.submembers(a,g,s)=={ai['QQ'],bi['QQ']}
        self.mutation(b,'group.announcement.update',group_pk=g,announcement='restart restored')
    def test16(self):
        a,ai,b,bi,c,ci,g=self.setup()
        self.denied(b,'group.policy.switch',dict(group_pk=g,target_policy='WECHAT',expected_policy_version=0,actor_role='OWNER',owner_display_id=ai['QQ'],actor=1))
        self.denied(c,'group.announcement.update',dict(group_pk=g,announcement='forged',role='OWNER',actor_role='OWNER'))
        inactive=self.s.login('m3user1',['WECHAT']); error(inactive.call('group.detail',dict(group_pk=g)),'SERVICE_NOT_ACTIVE')
        d,di,_=self.user(['WECHAT']); assert not d.call('group.detail',dict(group_pk=g))['success']
        anon=self.s.client(); error(anon.call('group.list',dict(service_type='QQ')),'AUTH_REQUIRED')
    @staticmethod
    def race(functions):
        with concurrent.futures.ThreadPoolExecutor(max_workers=len(functions)) as pool: return list(pool.map(lambda f:f(),functions))
    def test17(self):
        a,ai,b,bi,c,ci,g=self.setup(); self.admin(a,g,bi['QQ']); d,di,_=self.user(); r=self.apply(d,g)
        survivor,si,_=self.user(); self.join(a,survivor,g)
        v=self.detail(a,g)['group']['policy_version']
        outcomes=self.race([lambda:a.call('group.policy.switch',dict(group_pk=g,target_policy='WECHAT',expected_policy_version=v)),lambda:b.call('group.member.kick',dict(group_pk=g,target_display_id=ci['QQ'],expected_policy_version=v)),lambda:self.s.login('m3user2').call('group.request.respond',dict(request_pk=r,decision='ACCEPT',expected_policy_version=v))])
        assert outcomes[0]['success'],outcomes
        for response in outcomes[1:]:
            if not response['success']: assert response['error']['code'] in ('GROUP_VERSION_CONFLICT','GROUP_REQUEST_ALREADY_RESOLVED','GROUP_POLICY_DISABLED','GROUP_PERMISSION_DENIED'),response
        # Operations after the acknowledged switch must obey the new policy.
        assert si['QQ'] in self.members(a,g)
        self.denied(b,'group.member.kick',dict(group_pk=g,target_display_id=si['QQ']))
        error(a.call('group.policy.switch',dict(group_pk=g,target_policy='QQ',expected_policy_version=v)),'GROUP_VERSION_CONFLICT')
        error(a.call('group.announcement.update',dict(group_pk=g,announcement='stale',expected_policy_version=v)),'GROUP_VERSION_CONFLICT')
    def test18(self):
        a,ai,b,bi,c,ci,g=self.setup(); d,di,_=self.user(); r=self.apply(d,g)
        scenarios=[('create','groups','INSERT','group.create',dict(service_type='QQ',name='rollback')),('accept','group_members','INSERT','group.request.respond',dict(request_pk=r,decision='ACCEPT')),('switch','groups','UPDATE','group.policy.switch',dict(group_pk=g,target_policy='WECHAT',expected_policy_version=self.detail(a,g)['group']['policy_version']))]
        tables=['groups','group_members','group_requests','subgroups','subgroup_members','group_counters','mutation_requests']
        for label,table,verb,kind,data in scenarios:
            before={t:self.sql('SELECT * FROM '+t) for t in tables}; marks=[self.drain(x) for x in (a,b,c,d)]
            self.execute("CREATE TRIGGER fail_m3 BEFORE %s ON %s BEGIN SELECT RAISE(ABORT,'injected SQL failure'); END"%(verb,table))
            rid=str(uuid.uuid4())
            try: error(a.call(kind,data,rid),'DATABASE_ERROR')
            finally: self.execute('DROP TRIGGER fail_m3')
            assert {t:self.sql('SELECT * FROM '+t) for t in tables}==before,label
            for client,mark in zip((a,b,c,d),marks): self.no_event(client,mark,g)
            ok(a.call(kind,data,rid))
    def test19(self):
        a,ai,_=self.user(); b,bi,bn=self.user(); outsider,oi,_=self.user(); a2=self.s.login('m3user1'); g=self.create(a)
        marks=[self.drain(x) for x in (a,a2,b,outsider)]; r=self.apply(b,g)
        for client,mark in zip((a,a2,b),marks): no_private(client.event('group.request.new',mark))
        self.no_event(outsider,marks[3],g); marks=[self.drain(x) for x in (a,a2,b)]
        self.respond(a,r)
        for client,mark in zip((a,a2,b),marks): client.event('group.members.changed',mark)
        mark=self.drain(b); self.switch(a,g,'WECHAT'); b.event('group.policy.changed',mark)
        c,ci,cn=self.user(); r=self.mutation(b,'group.invite',group_pk=g,target_display_id=ci['QQ'])['request_pk']; c.event('group.request.new')
        self.respond(c,r); self.mutation(b,'group.member.leave',group_pk=g); mark=self.drain(b); omark=self.drain(outsider)
        self.switch(a,g,'QQ'); self.no_event(b,mark,g); self.no_event(outsider,omark,g)
        ok(c.call('auth.logout')); c.close(); self.switch(a,g,'WECHAT'); c=self.s.login(cn)
        assert self.detail(c,g)['group']['policy_type']=='WECHAT'
        assert self.detail(outsider,g)['members']==[]
    def replay(self,c,kind,data):
        rid=str(uuid.uuid4()); first=c.call(kind,data,rid); ok(first); mark=self.drain(c)
        assert c.call(kind,data,rid)==first,kind
        self.drain(c); assert len(c.events)==mark,kind+' replay emitted event'
        error(c.call(kind,dict(data,changed_payload=True),rid),'REQUEST_ID_CONFLICT')
        return first['data']
    def test20(self):
        a,ai,_=self.user(); b,bi,_=self.user(); c,ci,_=self.user(); a2=self.s.login('m3user1')
        g=self.replay(a,'group.create',dict(service_type='QQ',name='all mutations'))['group_pk']
        r=self.replay(b,'group.join.apply',dict(group_pk=g))['request_pk']
        payload=dict(request_pk=r,decision='ACCEPT'); result=self.race([lambda:a.call('group.request.respond',payload),lambda:a2.call('group.request.respond',payload)])
        assert sum(x['success'] for x in result)==1,result
        r=self.replay(c,'group.join.apply',dict(group_pk=g))['request_pk']; self.replay(a,'group.request.respond',dict(request_pk=r,decision='ACCEPT'))
        for kind,data in [('group.admin.set',dict(target_display_id=bi['QQ'],enabled=True)),('group.announcement.update',dict(announcement='replay')),('group.name.update',dict(name='renamed'))]: self.replay(a,kind,dict(group_pk=g,**data))
        s=self.replay(a,'group.subgroup.create',dict(group_pk=g,name='replay subgroup'))['subgroup_pk']
        self.replay(a,'group.subgroup.members.update',dict(subgroup_pk=s,add_display_ids=[bi['QQ']]))
        self.replay(a,'group.member.kick',dict(group_pk=g,target_display_id=bi['QQ']))
        self.replay(c,'group.member.leave',dict(group_pk=g))
        v=self.detail(a,g)['group']['policy_version']; self.replay(a,'group.policy.switch',dict(group_pk=g,target_policy='WECHAT',expected_policy_version=v))
        r=self.replay(a,'group.invite',dict(group_pk=g,target_display_id=bi['QQ']))['request_pk']; self.replay(b,'group.request.respond',dict(request_pk=r,decision='ACCEPT'))
        # Concurrent same UUID is one mutation, with byte-for-byte replay.
        rid=str(uuid.uuid4()); data=dict(group_pk=g,announcement='concurrent'); result=self.race([lambda:a.call('group.announcement.update',data,rid),lambda:a2.call('group.announcement.update',data,rid)])
        assert result[0]==result[1] and result[0]['success'],result
        self.replay(a,'group.disband',dict(group_pk=g))
    def test21(self):
        a,ai,an=self.user(); b,bi,bn=self.user(); c,ci,cn=self.user(); befriend(a,b,bi['QQ']); befriend(b,c,ci['QQ'])
        ok(a.call('account.binding.create')); ok(b.call('account.binding.create'))
        ok(a.call('friend.remark.set',dict(service_type='QQ',target_display_id=bi['QQ'],remark='legacy retained')))
        rid=str(uuid.uuid4()); payload=dict(service_type='QQ',target_display_id=bi['QQ'],remark='legacy replay'); saved=a.call('friend.remark.set',payload,rid); ok(saved)
        regid=str(uuid.uuid4()); regdata=registration('legacyregister'); regresult=ok(self.s.client().call('auth.register',regdata,regid))
        tables=['users','accounts','id_counters','registration_requests','account_bindings','friend_requests','friendships','friend_remarks','mutation_requests']; snapshot={t:self.sql('SELECT * FROM '+t) for t in tables}
        legacy=self.directory/'legacy-v2.sqlite3'; root=Path(__file__).resolve().parents[1]
        with sqlite3.connect(legacy) as db:
            db.executescript((root/'database/schema.sql').read_text()); db.executescript(next((root/'database/migrations').glob('002*')).read_text()); db.execute('DELETE FROM id_counters')
            for table in tables:
                for record in snapshot[table]: db.execute('INSERT INTO '+table+' VALUES('+','.join('?' for _ in record)+')',record)
        migrated=Server(self.s.exe,legacy)
        try:
            migrated.start()
            with sqlite3.connect(legacy) as db:
                assert db.execute('PRAGMA user_version').fetchone()==(4,)
                for table,records in snapshot.items(): assert db.execute('SELECT * FROM '+table).fetchall()==records,table
                assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
            backups=list(self.directory.glob('legacy-v2.sqlite3*v2*')); assert backups,'no pre-migration v2 backup'
            checked=False
            for backup in backups:
                with sqlite3.connect(backup) as db:
                    if db.execute('PRAGMA user_version').fetchone()!=(2,): continue
                    for table,records in snapshot.items(): assert db.execute('SELECT * FROM '+table).fetchall()==records,table
                    assert db.execute('PRAGMA quick_check').fetchone()==('ok',); checked=True
            assert checked
            old=migrated.login(an); assert old.call('friend.remark.set',payload,rid)==saved
            assert ok(old.call('account.binding.get'))['bound']
            assert rows(old,'friend.list',service_type='QQ')['items'][0]['remark']=='legacy replay'
            assert any(x['display_id']==ci['QQ'] for x in rows(old,'friend.recommend.same',service_type='QQ')['items'])
            assert any(x['display_id']==bi['WECHAT'] for x in rows(old,'friend.recommend.cross',source_service='QQ',target_service='WECHAT')['items'])
            assert ok(old.call('auth.register',regdata,regid))==regresult
            self.create(old); preserved={p:p.read_bytes() for p in backups}; migrated.stop(); migrated.start(); migrated.login(an)
            assert all(p.read_bytes()==content for p,content in preserved.items())
        finally: migrated.close()

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--server',required=True); parser.add_argument('--only'); args=parser.parse_args()
    selected=sorted(set(map(int,args.only.split(',')))) if args.only else list(range(1,22)); failures=[]
    assert set(selected)<=set(range(1,22))
    for number in selected:
        with tempfile.TemporaryDirectory(prefix='im-mvp3-t%02d-'%number) as directory:
            server=Server(args.server,Path(directory)/'acceptance.sqlite3')
            try:
                server.start(); Acceptance(server,directory).__getattribute__('test%02d'%number)()
                print('M3-T%02d PASS'%number,flush=True)
            except Exception as exc:
                failures.append(number); print('M3-T%02d FAIL: %s: %s'%(number,type(exc).__name__,exc),flush=True); traceback.print_exc()
            finally: server.close()
    print('M3-T18 scope: server SQL rollback and authoritative query only; GUI failure handling requires separate UI evidence.')
    print('M3-T22 NOT RUN here: Qt Creator/CMake and three-client native UI are a separate acceptance scope.')
    print('%d passed; %d failed; %d not selected'%(len(selected)-len(failures),len(failures),21-len(selected)))
    return bool(failures)

if __name__=='__main__': raise SystemExit(main())
