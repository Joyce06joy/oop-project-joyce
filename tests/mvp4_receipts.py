#!/usr/bin/env python3
"""Persisted delivery counts through real TCP and a server restart."""
import argparse,tempfile
from pathlib import Path
from mvp2_helpers import Server,ok
from mvp4_acceptance import Acceptance

def counts(message,delivered,recipients):
 assert message['status']=='stored',message
 assert message['delivered_count']==delivered,message
 assert message['recipient_count']==recipients,message
 assert 'read_count' not in message,message

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--server',required=True);args=parser.parse_args()
 with tempfile.TemporaryDirectory(prefix='im-mvp4-receipts-') as directory:
  s=Server(args.server,Path(directory)/'receipts.sqlite3')
  try:
   s.start();t=Acceptance(s,directory)
   a,ai,an=t.user();b,bi,bn=t.user();conv=t.private(a,b,bi)
   m=t.send_private(a,bi,'persisted receipt');counts(m,0,1)
   counts(t.history(a,conv)['items'][0],0,1)
   ok(b.call('chat.message.ack',{'message_id':m['message_id']}))
   counts(t.history(a,conv)['items'][0],1,1)
   s.stop();s.start();a=s.login(an);b=s.login(bn)
   counts(t.history(a,conv)['items'][0],1,1)
   c,ci,cn=t.user();g=t.create(a,'QQ');t.join(a,b,g);t.join(a,c,g)
   group=t.send_group(a,g,'partially delivered');counts(group,0,2)
   ok(b.call('chat.message.ack',{'message_id':group['message_id']}))
   counts(t.history(a,group['conversation_id'])['items'][0],1,2)
   s.stop();s.start();a=s.login(an)
   counts(t.history(a,group['conversation_id'])['items'][0],1,2)
   print('MVP4 persisted delivery counts PASS: private 0/1→1/1; group 1/2; restart retains both')
  finally:s.close()
if __name__=='__main__':main()
