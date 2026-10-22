BEGIN IMMEDIATE;
CREATE TABLE group_counters (
 service_type TEXT PRIMARY KEY CHECK(service_type IN ('QQ','WECHAT')),
 next_value INTEGER NOT NULL CHECK(next_value>0)
);
INSERT INTO group_counters VALUES('QQ',1001),('WECHAT',1001);
CREATE TABLE groups (
 group_pk INTEGER PRIMARY KEY,
 service_type TEXT NOT NULL CHECK(service_type IN ('QQ','WECHAT')),
 group_number TEXT NOT NULL,
 name TEXT NOT NULL,
 owner_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 policy_type TEXT NOT NULL CHECK(policy_type IN ('QQ','WECHAT')),
 policy_version INTEGER NOT NULL DEFAULT 0 CHECK(policy_version>=0),
 announcement TEXT NOT NULL DEFAULT '',
 status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK(status IN ('ACTIVE','DISBANDED')),
 created_at TEXT NOT NULL,
 UNIQUE(service_type,group_number),
 FOREIGN KEY(group_pk,owner_account_pk) REFERENCES group_members(group_pk,account_pk) DEFERRABLE INITIALLY DEFERRED
);
CREATE TABLE group_members (
 group_pk INTEGER NOT NULL REFERENCES groups(group_pk),
 account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 stored_role TEXT NOT NULL CHECK(stored_role IN ('OWNER','ADMIN','MEMBER')),
 joined_at TEXT NOT NULL,
 PRIMARY KEY(group_pk,account_pk)
);
CREATE UNIQUE INDEX group_single_owner ON group_members(group_pk) WHERE stored_role='OWNER';
CREATE INDEX group_members_account ON group_members(account_pk,group_pk);
CREATE TRIGGER group_owner_valid BEFORE INSERT ON groups BEGIN
 SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM accounts WHERE account_pk=NEW.owner_account_pk
  AND service_type=NEW.service_type AND status='ACTIVE') THEN RAISE(ABORT,'invalid group owner') END;
END;
CREATE TRIGGER group_identity_immutable BEFORE UPDATE ON groups BEGIN
 SELECT CASE WHEN NEW.group_pk<>OLD.group_pk OR NEW.service_type<>OLD.service_type
  OR NEW.group_number<>OLD.group_number OR NEW.owner_account_pk<>OLD.owner_account_pk
  OR NEW.created_at<>OLD.created_at OR OLD.status='DISBANDED'
 THEN RAISE(ABORT,'immutable group identity') END;
END;
CREATE TRIGGER group_member_valid BEFORE INSERT ON group_members BEGIN
 SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM groups g JOIN accounts a ON a.service_type=g.service_type
  WHERE g.group_pk=NEW.group_pk AND g.status='ACTIVE' AND a.account_pk=NEW.account_pk AND a.status='ACTIVE'
  AND ((NEW.stored_role='OWNER' AND a.account_pk=g.owner_account_pk)
   OR (NEW.stored_role<>'OWNER' AND a.account_pk<>g.owner_account_pk)))
 THEN RAISE(ABORT,'invalid group member') END;
END;
CREATE TRIGGER group_member_role_valid BEFORE UPDATE ON group_members BEGIN
 SELECT CASE WHEN NEW.group_pk<>OLD.group_pk OR NEW.account_pk<>OLD.account_pk OR OLD.stored_role='OWNER'
  OR NEW.stored_role='OWNER' OR NEW.joined_at<>OLD.joined_at
 THEN RAISE(ABORT,'invalid member role change') END;
END;
CREATE TABLE group_requests (
 request_pk INTEGER PRIMARY KEY,
 group_pk INTEGER NOT NULL REFERENCES groups(group_pk),
 kind TEXT NOT NULL CHECK(kind IN ('APPLY','INVITE')),
 initiator_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 target_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 status TEXT NOT NULL CHECK(status IN ('PENDING','ACCEPTED','REJECTED','CANCELLED_POLICY_CHANGED')),
 created_at TEXT NOT NULL,
 processed_at TEXT,
 CHECK((status='PENDING' AND processed_at IS NULL) OR (status<>'PENDING' AND processed_at IS NOT NULL))
);
CREATE UNIQUE INDEX group_pending_target ON group_requests(group_pk,target_account_pk) WHERE status='PENDING';
CREATE INDEX group_requests_group ON group_requests(group_pk,status,request_pk);
CREATE INDEX group_requests_target ON group_requests(target_account_pk,status,request_pk);
CREATE TRIGGER group_request_valid BEFORE INSERT ON group_requests BEGIN
 SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM groups g JOIN accounts i ON i.service_type=g.service_type
  JOIN accounts t ON t.service_type=g.service_type WHERE g.group_pk=NEW.group_pk AND g.status='ACTIVE'
  AND i.account_pk=NEW.initiator_account_pk AND t.account_pk=NEW.target_account_pk
  AND i.status='ACTIVE' AND t.status='ACTIVE'
  AND NOT EXISTS (SELECT 1 FROM group_members m WHERE m.group_pk=g.group_pk AND m.account_pk=t.account_pk)
  AND ((NEW.kind='APPLY' AND g.policy_type='QQ' AND i.account_pk=t.account_pk)
   OR (NEW.kind='INVITE' AND g.policy_type='WECHAT' AND EXISTS
    (SELECT 1 FROM group_members m WHERE m.group_pk=g.group_pk AND m.account_pk=i.account_pk))))
 THEN RAISE(ABORT,'invalid group request') END;
END;
CREATE TRIGGER group_request_terminal BEFORE UPDATE ON group_requests BEGIN
 SELECT CASE WHEN OLD.status<>'PENDING' OR NEW.status='PENDING' OR NEW.request_pk<>OLD.request_pk
  OR NEW.group_pk<>OLD.group_pk OR NEW.kind<>OLD.kind OR NEW.initiator_account_pk<>OLD.initiator_account_pk
  OR NEW.target_account_pk<>OLD.target_account_pk OR NEW.created_at<>OLD.created_at
 THEN RAISE(ABORT,'invalid group request transition') END;
END;
CREATE TABLE subgroups (
 subgroup_pk INTEGER PRIMARY KEY,
 parent_group_pk INTEGER NOT NULL REFERENCES groups(group_pk),
 name TEXT NOT NULL,
 creator_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 status TEXT NOT NULL CHECK(status IN ('ACTIVE','DISBANDED')),
 created_at TEXT NOT NULL,
 UNIQUE(subgroup_pk,parent_group_pk)
);
CREATE INDEX subgroup_parent ON subgroups(parent_group_pk,status,subgroup_pk);
CREATE TABLE subgroup_members (
 subgroup_pk INTEGER NOT NULL,
 parent_group_pk INTEGER NOT NULL,
 account_pk INTEGER NOT NULL,
 joined_at TEXT NOT NULL,
 PRIMARY KEY(subgroup_pk,account_pk),
 FOREIGN KEY(subgroup_pk,parent_group_pk) REFERENCES subgroups(subgroup_pk,parent_group_pk),
 FOREIGN KEY(parent_group_pk,account_pk) REFERENCES group_members(group_pk,account_pk) ON DELETE CASCADE
);
CREATE TRIGGER subgroup_valid BEFORE INSERT ON subgroups BEGIN
 SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM groups g JOIN group_members m ON m.group_pk=g.group_pk
  WHERE g.group_pk=NEW.parent_group_pk AND g.status='ACTIVE' AND g.policy_type='QQ'
  AND m.account_pk=NEW.creator_account_pk AND m.stored_role='OWNER')
 THEN RAISE(ABORT,'invalid subgroup creator') END;
END;
CREATE TRIGGER subgroup_member_valid BEFORE INSERT ON subgroup_members BEGIN
 SELECT CASE WHEN NOT EXISTS (SELECT 1 FROM subgroups s JOIN groups g ON g.group_pk=s.parent_group_pk
  WHERE s.subgroup_pk=NEW.subgroup_pk AND s.parent_group_pk=NEW.parent_group_pk AND s.status='ACTIVE'
  AND g.status='ACTIVE' AND g.policy_type='QQ') THEN RAISE(ABORT,'subgroup disabled') END;
END;
PRAGMA user_version=3;
COMMIT;
