#!/usr/bin/env python3
"""Direct schema invariants; independent in-memory databases, no user data."""
import sqlite3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def denied(db,sql,parameters=()):
    try:
        db.execute(sql,parameters); db.commit()
    except sqlite3.IntegrityError:
        db.rollback()
    else:
        raise AssertionError('constraint accepted an invalid operation: '+sql)
def main():
    with sqlite3.connect(':memory:') as db:
        db.execute('PRAGMA foreign_keys=ON')
        for relative in ['database/schema.sql','database/migrations/002_friends.sql','database/migrations/003_groups.sql']:
            db.executescript((ROOT/relative).read_text())
        assert db.execute('PRAGMA user_version').fetchone()[0]==3
        db.execute("INSERT INTO users(user_id,login_name,password_hash,nickname,registered_at) VALUES(1,'schema_test','unused','Schema','2026-10-21T00:00:00Z')")
        for pk,service,number in [(1,'QQ','10001'),(2,'WECHAT','WX8001'),(3,'QQ','10002'),(4,'QQ','10003')]:
            # One account/service per user, preserving original unique constraints.
            if pk>2:
                db.execute("INSERT INTO users(user_id,login_name,password_hash,nickname,registered_at) VALUES(?,?,'unused','Schema','now')",(pk,'schema'+str(pk)))
            db.execute("INSERT INTO accounts(account_pk,user_id,service_type,display_id,status,opened_at) VALUES(?,?,?,?, 'ACTIVE','now')",(pk,1 if pk<=2 else pk,service,number))
        db.commit()
        create="INSERT INTO groups(group_pk,service_type,group_number,name,owner_account_pk,policy_type,created_at) VALUES(1,'QQ','1001','Parent',1,'QQ','now')"
        # Deferrable ownership permits insertion in the same transaction, never
        # a committed owner-less group.
        denied(db,create)
        assert db.execute('SELECT count(*) FROM groups').fetchone()[0]==0
        db.execute(create)
        db.execute("INSERT INTO group_members VALUES(1,1,'OWNER','now')")
        db.execute("INSERT INTO group_members VALUES(1,3,'MEMBER','now')")
        db.commit()
        denied(db,"INSERT INTO group_members VALUES(1,2,'MEMBER','now')")
        denied(db,"INSERT INTO group_members VALUES(1,4,'OWNER','now')")
        denied(db,"UPDATE group_members SET stored_role='MEMBER' WHERE account_pk=1")
        denied(db,"DELETE FROM group_members WHERE account_pk=1")
        db.execute("INSERT INTO subgroups VALUES(1,1,'Child',1,'ACTIVE','now')")
        db.execute("INSERT INTO subgroup_members VALUES(1,1,1,'now')")
        db.execute("INSERT INTO subgroup_members VALUES(1,1,3,'now')")
        db.commit()
        denied(db,"INSERT INTO subgroup_members VALUES(1,1,4,'now')")
        denied(db,"INSERT INTO subgroup_members VALUES(1,999,3,'now')")
        db.execute('DELETE FROM group_members WHERE group_pk=1 AND account_pk=3');db.commit()
        assert db.execute('SELECT account_pk FROM subgroup_members ORDER BY account_pk').fetchall()==[(1,)]
        db.execute("UPDATE groups SET policy_type='WECHAT',policy_version=1 WHERE group_pk=1");db.commit()
        assert db.execute('SELECT count(*) FROM subgroups').fetchone()[0]==1
        assert db.execute('SELECT count(*) FROM subgroup_members').fetchone()[0]==1
        db.execute("INSERT INTO group_members VALUES(1,3,'MEMBER','now')");db.commit()
        denied(db,"INSERT INTO subgroup_members VALUES(1,1,3,'now')")
        denied(db,"UPDATE groups SET service_type='WECHAT' WHERE group_pk=1")
        assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
    print('PASS schema v3: deferred owner membership, platform/role invariants, parent subset FK/cascade, policy preservation, immutable service')
if __name__=='__main__':main()
