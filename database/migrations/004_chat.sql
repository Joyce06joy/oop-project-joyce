BEGIN IMMEDIATE;
CREATE TABLE conversations (
 conversation_pk INTEGER PRIMARY KEY, service_type TEXT NOT NULL CHECK(service_type IN ('QQ','WECHAT')),
 kind TEXT NOT NULL CHECK(kind IN ('PRIVATE','GROUP','SUBGROUP')), group_pk INTEGER REFERENCES groups(group_pk),
 subgroup_pk INTEGER UNIQUE REFERENCES subgroups(subgroup_pk), created_at TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK(status IN ('ACTIVE','DISBANDED')),
 CHECK((kind='PRIVATE' AND group_pk IS NULL AND subgroup_pk IS NULL) OR (kind='GROUP' AND group_pk IS NOT NULL AND subgroup_pk IS NULL) OR (kind='SUBGROUP' AND group_pk IS NOT NULL AND subgroup_pk IS NOT NULL))
);
CREATE UNIQUE INDEX conversation_group ON conversations(group_pk) WHERE kind='GROUP';
CREATE TABLE private_pairs (conversation_pk INTEGER PRIMARY KEY REFERENCES conversations(conversation_pk), service_type TEXT NOT NULL, account_a INTEGER NOT NULL REFERENCES accounts(account_pk), account_b INTEGER NOT NULL REFERENCES accounts(account_pk), CHECK(account_a<account_b), UNIQUE(service_type,account_a,account_b));
CREATE TRIGGER chat_private_pair_valid BEFORE INSERT ON private_pairs BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM conversations c JOIN accounts a ON a.account_pk=NEW.account_a JOIN accounts b ON b.account_pk=NEW.account_b WHERE c.conversation_pk=NEW.conversation_pk AND c.kind='PRIVATE' AND c.service_type=NEW.service_type AND a.service_type=c.service_type AND b.service_type=c.service_type AND a.status='ACTIVE' AND b.status='ACTIVE') THEN RAISE(ABORT,'invalid private participants') END;
END;
CREATE TRIGGER chat_private_pair_immutable BEFORE UPDATE ON private_pairs BEGIN SELECT RAISE(ABORT,'immutable private pair'); END;
CREATE TABLE conversation_members (epoch_pk INTEGER PRIMARY KEY,conversation_pk INTEGER NOT NULL REFERENCES conversations(conversation_pk),account_pk INTEGER NOT NULL REFERENCES accounts(account_pk), joined_at TEXT NOT NULL,left_at TEXT,joined_after_message_id INTEGER NOT NULL DEFAULT 0,left_at_message_id INTEGER,CHECK(joined_after_message_id>=0),CHECK(left_at_message_id IS NULL OR left_at_message_id>=joined_after_message_id),CHECK((left_at IS NULL)=(left_at_message_id IS NULL)));
CREATE UNIQUE INDEX conversation_open_epoch ON conversation_members(conversation_pk,account_pk) WHERE left_at_message_id IS NULL;
CREATE INDEX conversation_member_account ON conversation_members(account_pk,conversation_pk);
CREATE TABLE messages (message_pk INTEGER PRIMARY KEY AUTOINCREMENT,conversation_pk INTEGER NOT NULL REFERENCES conversations(conversation_pk),sender_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),request_id TEXT NOT NULL,operation_type TEXT NOT NULL,content TEXT NOT NULL CHECK(length(CAST(content AS BLOB)) BETWEEN 1 AND 4096),content_type TEXT NOT NULL DEFAULT 'TEXT' CHECK(content_type='TEXT'),created_at TEXT NOT NULL,UNIQUE(sender_account_pk,request_id,operation_type));
CREATE INDEX message_conversation ON messages(conversation_pk,message_pk);
CREATE INDEX message_request ON messages(sender_account_pk,request_id);
CREATE TABLE message_receipts(message_pk INTEGER NOT NULL REFERENCES messages(message_pk),recipient_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),delivered_at TEXT,read_at TEXT,PRIMARY KEY(message_pk,recipient_account_pk));
CREATE INDEX receipt_recipient ON message_receipts(recipient_account_pk,delivered_at);
INSERT INTO conversations(service_type,kind,group_pk,created_at,status) SELECT service_type,'GROUP',group_pk,created_at,status FROM groups;
INSERT INTO conversations(service_type,kind,group_pk,subgroup_pk,created_at,status) SELECT g.service_type,'SUBGROUP',g.group_pk,s.subgroup_pk,s.created_at,s.status FROM subgroups s JOIN groups g ON g.group_pk=s.parent_group_pk WHERE g.service_type='QQ';
INSERT INTO conversation_members(conversation_pk,account_pk,joined_at,left_at,left_at_message_id) SELECT c.conversation_pk,m.account_pk,m.joined_at,CASE WHEN c.status<>'ACTIVE' THEN c.created_at END,CASE WHEN c.status<>'ACTIVE' THEN 0 END FROM conversations c JOIN group_members m ON m.group_pk=c.group_pk WHERE c.kind='GROUP';
INSERT INTO conversation_members(conversation_pk,account_pk,joined_at,left_at,left_at_message_id) SELECT c.conversation_pk,m.account_pk,m.joined_at,CASE WHEN c.status<>'ACTIVE' THEN c.created_at END,CASE WHEN c.status<>'ACTIVE' THEN 0 END FROM conversations c JOIN subgroup_members m ON m.subgroup_pk=c.subgroup_pk WHERE c.kind='SUBGROUP';
CREATE TRIGGER chat_group_created AFTER INSERT ON groups BEGIN
 INSERT INTO conversations(service_type,kind,group_pk,created_at,status) VALUES(NEW.service_type,'GROUP',NEW.group_pk,NEW.created_at,NEW.status);
END;
CREATE TRIGGER chat_subgroup_created AFTER INSERT ON subgroups WHEN (SELECT service_type FROM groups WHERE group_pk=NEW.parent_group_pk)='QQ' BEGIN
 INSERT INTO conversations(service_type,kind,group_pk,subgroup_pk,created_at,status) VALUES('QQ','SUBGROUP',NEW.parent_group_pk,NEW.subgroup_pk,NEW.created_at,NEW.status);
END;
CREATE TRIGGER chat_group_join AFTER INSERT ON group_members BEGIN
 INSERT INTO conversation_members(conversation_pk,account_pk,joined_at,joined_after_message_id) SELECT conversation_pk,NEW.account_pk,NEW.joined_at,COALESCE((SELECT MAX(message_pk) FROM messages),0) FROM conversations WHERE kind='GROUP' AND group_pk=NEW.group_pk;
END;
CREATE TRIGGER chat_subgroup_join AFTER INSERT ON subgroup_members BEGIN
 INSERT INTO conversation_members(conversation_pk,account_pk,joined_at,joined_after_message_id) SELECT conversation_pk,NEW.account_pk,NEW.joined_at,COALESCE((SELECT MAX(message_pk) FROM messages),0) FROM conversations WHERE subgroup_pk=NEW.subgroup_pk;
END;
CREATE TRIGGER chat_group_leave AFTER DELETE ON group_members BEGIN
 UPDATE conversation_members SET left_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),left_at_message_id=COALESCE((SELECT MAX(message_pk) FROM messages),0) WHERE account_pk=OLD.account_pk AND left_at_message_id IS NULL AND conversation_pk IN (SELECT conversation_pk FROM conversations WHERE group_pk=OLD.group_pk);
END;
CREATE TRIGGER chat_subgroup_leave AFTER DELETE ON subgroup_members BEGIN
 UPDATE conversation_members SET left_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),left_at_message_id=COALESCE((SELECT MAX(message_pk) FROM messages),0) WHERE account_pk=OLD.account_pk AND left_at_message_id IS NULL AND conversation_pk IN (SELECT conversation_pk FROM conversations WHERE subgroup_pk=OLD.subgroup_pk);
END;
CREATE TRIGGER chat_group_disband AFTER UPDATE OF status ON groups WHEN NEW.status='DISBANDED' BEGIN
 UPDATE conversations SET status='DISBANDED' WHERE group_pk=NEW.group_pk;
 UPDATE conversation_members SET left_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),left_at_message_id=COALESCE((SELECT MAX(message_pk) FROM messages),0) WHERE left_at_message_id IS NULL AND conversation_pk IN (SELECT conversation_pk FROM conversations WHERE group_pk=NEW.group_pk);
END;
CREATE TRIGGER chat_subgroup_disband AFTER UPDATE OF status ON subgroups WHEN NEW.status='DISBANDED' BEGIN
 UPDATE conversations SET status='DISBANDED' WHERE subgroup_pk=NEW.subgroup_pk;
 UPDATE conversation_members SET left_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),left_at_message_id=COALESCE((SELECT MAX(message_pk) FROM messages),0) WHERE left_at_message_id IS NULL AND conversation_pk IN (SELECT conversation_pk FROM conversations WHERE subgroup_pk=NEW.subgroup_pk);
END;
PRAGMA user_version=4;
COMMIT;
