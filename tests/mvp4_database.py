#!/usr/bin/env python3
"""Migration/epoch regression, no user databases touched."""
import sqlite3,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
db=sqlite3.connect(':memory:')
for name in ['schema.sql','migrations/002_friends.sql','migrations/003_groups.sql']:
 db.executescript((root/'database'/name).read_text())
# Membership boundaries must be message IDs even when all timestamps match.
for i in range(1,4):
 db.execute("insert into users(user_id,login_name,password_hash,nickname,registered_at) values(?,?,'hash',?,'same-second')",(i,f'u{i}',f'U{i}'))
 db.execute("insert into accounts values(?,?,'QQ',?,'ACTIVE','same-second')",(i,i,str(10000+i)))
db.execute("begin") if not db.in_transaction else None
db.execute("insert into groups(group_pk,service_type,group_number,name,owner_account_pk,policy_type,created_at) values(1,'QQ','1001','G',1,'QQ','same-second')")
for i,role in [(1,'OWNER'),(2,'MEMBER')]:db.execute("insert into group_members values(1,?,?, 'same-second')",(i,role))
db.execute("insert into subgroups values(1,1,'S',1,'ACTIVE','same-second')")
for i in [1,2]:db.execute("insert into subgroup_members values(1,1,?,'same-second')",(i,))
db.execute("insert into friendships values(1,2,'same-second')")
db.commit()
old_tables=['users','accounts','friendships','groups','group_members','subgroups','subgroup_members']
before={table:db.execute('select * from '+table).fetchall() for table in old_tables}
db.executescript((root/'database/migrations/004_chat.sql').read_text())
assert db.execute('pragma user_version').fetchone()[0]==4
assert all(before[table]==db.execute('select * from '+table).fetchall() for table in old_tables)
assert db.execute('select count(*) from conversation_members').fetchone()[0]==4
assert not db.execute('pragma foreign_key_check').fetchall()
print('MVP4 populated v3 migration preserves old rows and seeds windows PASS')
c=db.execute("select conversation_pk from conversations where kind='GROUP'").fetchone()[0]
sc=db.execute("select conversation_pk from conversations where kind='SUBGROUP'").fetchone()[0]
def message(n):
 db.execute("insert into messages(conversation_pk,sender_account_pk,request_id,operation_type,content,created_at) values(?,1,?,'chat.group.send','text','same-second')",(c,str(n)))
 return db.execute('select last_insert_rowid()').fetchone()[0]
def visible():
 return [r[0] for r in db.execute('select m.message_pk from messages m where exists(select 1 from conversation_members e where e.conversation_pk=m.conversation_pk and e.account_pk=2 and m.message_pk>e.joined_after_message_id and (e.left_at_message_id is null or m.message_pk<=e.left_at_message_id)) order by m.message_pk')]
m1=message(1)
db.execute('delete from group_members where group_pk=1 and account_pk=2')
assert db.execute('select count(*) from subgroup_members where account_pk=2').fetchone()[0]==0
assert db.execute('select left_at_message_id from conversation_members where conversation_pk=? and account_pk=2',(sc,)).fetchone()[0]==m1
m2=message(2)
db.execute("insert into group_members values(1,2,'MEMBER','same-second')")
m3=message(3)
assert visible()==[m1,m3],visible()
db.execute("update groups set status='DISBANDED' where group_pk=1")
assert db.execute('select count(*) from conversation_members where left_at_message_id is null').fetchone()[0]==0
assert db.execute('select count(*) from accounts').fetchone()[0]==3
assert not db.execute('pragma foreign_key_check').fetchall()
db.commit()
print('MVP4 same-second membership epochs, parent cascade, disband, old data PASS')
