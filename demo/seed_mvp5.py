#!/usr/bin/env python3
"""Populate only dedicated MVP5 demo database through real TCP APIs."""
import argparse,fcntl,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from mvp2_helpers import Server,ok,befriend
PASSWORD='Mvp5-Demo-2026!'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--server',default=str(ROOT/'build/mvp5/IMServer'));parser.add_argument('--reset',action='store_true');args=parser.parse_args()
 db=ROOT/'data/mvp5-demo.sqlite3';db.parent.mkdir(exist_ok=True)
 with Path(str(db)+'.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  if db.exists() and not args.reset:raise SystemExit('MVP5 demo exists. Stop its server and explicitly use --reset to replace only this demo.')
  if args.reset:
   for suffix in ['','-wal','-shm']:Path(str(db)+suffix).unlink(missing_ok=True)
 server=Server(args.server,db)
 try:
  server.start();clients={};ids={}
  for name in ['Alice','Bob','Carol']:
   client=server.client();r=ok(client.call('auth.register',dict(login_name=name,nickname=name,password=PASSWORD,birthday=None,location='长春',services=['QQ','WECHAT','WEIBO'])))
   ok(client.call('auth.login',dict(login_name=name,password=PASSWORD)))
   for service in ['QQ','WECHAT','WEIBO']:ok(client.call('account.service.activate',dict(service_type=service)))
   clients[name]=client;ids[name]={a['service_type']:a['display_id'] for a in r['accounts']}
  a,b,c=[clients[x] for x in ['Alice','Bob','Carol']]
  for service in ['QQ','WECHAT']:
   befriend(a,b,ids['Bob'][service],service);befriend(a,c,ids['Carol'][service],service)
  qq=ok(a.call('group.create',dict(service_type='QQ',name='MVP5 QQ 演示群')))['group_pk']
  wx=ok(a.call('group.create',dict(service_type='WECHAT',name='MVP5 微信演示群')))['group_pk']
  for client,target in [(b,'Bob'),(c,'Carol')]:
   req=ok(client.call('group.join.apply',dict(group_pk=qq)))
   ok(a.call('group.request.respond',dict(request_pk=req['request_pk'],decision='ACCEPT')))
   req=ok(a.call('group.invite',dict(group_pk=wx,target_display_id=ids[target]['WECHAT'])))
   ok(client.call('group.request.respond',dict(request_pk=req['request_pk'],decision='ACCEPT')))
  ok(a.call('group.admin.set',dict(group_pk=qq,target_display_id=ids['Bob']['QQ'],enabled=True)))
  sub=ok(a.call('group.subgroup.create',dict(group_pk=qq,name='Alice与Bob临时讨论',member_display_ids=[ids['Bob']['QQ']])))['subgroup_pk']
  for service in ['QQ','WECHAT']:
   for index in range(1,4):ok(a.call('chat.private.send',dict(service_type=service,target_display_id=ids['Bob'][service],content=f'{service} 历史消息 {index}：你好 Bob 👋')))
  for group in [qq,wx]:ok(a.call('chat.group.send',dict(group_pk=group,content='欢迎 Alice / Bob / Carol：此消息已保存到服务器。')))
  ok(a.call('chat.group.send',dict(group_pk=qq,subgroup_pk=sub,content='仅 Alice 与 Bob 的 QQ 子群历史。')))
  for name,accounts in ids.items():print(name,accounts)
  print('Demo-only password:',PASSWORD);print('QQ group',qq,'WECHAT group',wx,'QQ subgroup',sub)
  print('Carol cannot read QQ subgroup; Alice/Bob/Carol share both main groups. Alice and Bob are QQ/WECHAT friends.')
  print('Seeded real TCP messages: 9. No read receipts. Database:',db)
 finally:server.close()
if __name__=='__main__':main()
