#!/usr/bin/env python3
"""Seed only a dedicated MVP3 demo, through the real TCP API."""
import argparse,fcntl,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from mvp2_helpers import Server,ok,befriend
PASSWORD='Mvp3-Pass-2026!'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--server',default=str(ROOT/'build/mvp3/IMServer'));parser.add_argument('--reset',action='store_true');args=parser.parse_args()
    db=ROOT/'data/mvp3-demo.sqlite3';db.parent.mkdir(exist_ok=True)
    with Path(str(db)+'.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if db.exists() and not args.reset:raise SystemExit('MVP3 demo exists. Stop its server and explicitly use --reset to replace only this demo.')
        if args.reset:
            for suffix in ['', '-wal','-shm']:Path(str(db)+suffix).unlink(missing_ok=True)
    server=Server(args.server,db)
    try:
        server.start(); clients={};ids={}
        for name in ['Alice','Bob','Carol','David']:
            client=server.client();registered=ok(client.call('auth.register',dict(login_name=name,nickname=name,password=PASSWORD,birthday=None,location='长春',services=['QQ','WECHAT','WEIBO'])))
            ok(client.call('auth.login',dict(login_name=name,password=PASSWORD)))
            for service in ['QQ','WECHAT','WEIBO']:ok(client.call('account.service.activate',dict(service_type=service)))
            clients[name]=client;ids[name]={a['service_type']:a['display_id'] for a in registered['accounts']}
        alice,bob,carol,david=[clients[n] for n in ['Alice','Bob','Carol','David']]
        befriend(alice,bob,ids['Bob']['QQ']);ok(alice.call('account.binding.create'));ok(bob.call('account.binding.create'))
        groups={}
        for service in ['QQ','WECHAT']:
            for number in range(1001,1007):
                group=ok(alice.call('group.create',dict(service_type=service,name=f'{service} 演示群 {number}')))
                assert group['group_number']==str(number)
                groups[(service,number)]=group['group_pk']
        qq=groups['QQ',1001];wechat=groups['WECHAT',1004]
        for client in [bob,carol]:
            request=ok(client.call('group.join.apply',dict(group_pk=qq)))
            ok(alice.call('group.request.respond',dict(request_pk=request['request_pk'],decision='ACCEPT')))
        ok(alice.call('group.admin.set',dict(group_pk=qq,target_display_id=ids['Bob']['QQ'],enabled=True)))
        child=ok(alice.call('group.subgroup.create',dict(group_pk=qq,name='QQ 临时讨论组',member_display_ids=[ids['Carol']['QQ']])))
        request=ok(alice.call('group.invite',dict(group_pk=wechat,target_display_id=ids['Bob']['WECHAT'])))
        ok(bob.call('group.request.respond',dict(request_pk=request['request_pk'],decision='ACCEPT')))
        invitation=ok(bob.call('group.invite',dict(group_pk=wechat,target_display_id=ids['Carol']['WECHAT'])))
        pending=ok(david.call('group.join.apply',dict(group_pk=qq)))
        detail=ok(alice.call('group.detail',dict(group_pk=qq)))
        assert detail['group']['member_count']==3 and len(detail['members'])==3
        assert ok(alice.call('group.subgroup.list',dict(group_pk=qq,subgroup_pk=child['subgroup_pk'])))['total']==2
        for name,accounts in ids.items():print(name,accounts)
        print('Demo-only password:',PASSWORD)
        print('QQ group 1001:',qq,'Alice OWNER / Bob ADMIN / Carol MEMBER; child:',child['subgroup_pk'])
        print('WECHAT group 1004:',wechat,'Alice OWNER / Bob MEMBER; Carol invitation:',invitation['request_pk'])
        print('David QQ pending apply:',pending['request_pk'],'policy switching cancels it without removing members/roles/subgroup.')
        print('Both platforms have explicit demo numbers 1001–1006; normal startup never resets them.')
        print('Database:',db)
    finally:server.close()
if __name__=='__main__':main()
