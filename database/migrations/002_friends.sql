BEGIN IMMEDIATE;
CREATE TABLE account_bindings (
 binding_id INTEGER PRIMARY KEY,
 qq_account_pk INTEGER NOT NULL UNIQUE REFERENCES accounts(account_pk),
 wechat_account_pk INTEGER NOT NULL UNIQUE REFERENCES accounts(account_pk),
 created_at TEXT NOT NULL
);
CREATE TRIGGER binding_valid_insert BEFORE INSERT ON account_bindings BEGIN
 SELECT CASE WHEN NOT EXISTS (
  SELECT 1 FROM accounts q JOIN accounts w ON q.user_id=w.user_id
  WHERE q.account_pk=NEW.qq_account_pk AND q.service_type='QQ' AND q.status='ACTIVE'
   AND w.account_pk=NEW.wechat_account_pk AND w.service_type='WECHAT' AND w.status='ACTIVE'
 ) THEN RAISE(ABORT,'invalid binding') END;
END;
CREATE TRIGGER binding_no_update BEFORE UPDATE ON account_bindings BEGIN
 SELECT RAISE(ABORT,'replace binding explicitly');
END;
CREATE TABLE friend_requests (
 request_id TEXT PRIMARY KEY,
 service_type TEXT NOT NULL CHECK(service_type IN ('QQ','WECHAT','WEIBO')),
 sender_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 target_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 verification_message TEXT NOT NULL DEFAULT '',
 status TEXT NOT NULL CHECK(status IN ('PENDING','ACCEPTED','REJECTED','CANCELLED')),
 created_at TEXT NOT NULL,
 resolved_at TEXT,
 CHECK(sender_account_pk<>target_account_pk),
 CHECK((status='PENDING' AND resolved_at IS NULL) OR (status<>'PENDING' AND resolved_at IS NOT NULL))
);
CREATE UNIQUE INDEX friend_pending_pair ON friend_requests (
 service_type,min(sender_account_pk,target_account_pk),max(sender_account_pk,target_account_pk)
) WHERE status='PENDING';
CREATE INDEX friend_inbox ON friend_requests(target_account_pk,status,created_at,request_id);
CREATE INDEX friend_outbox ON friend_requests(sender_account_pk,status);
CREATE TRIGGER request_valid_insert BEFORE INSERT ON friend_requests BEGIN
 SELECT CASE WHEN NOT EXISTS (
  SELECT 1 FROM accounts s JOIN accounts t ON s.service_type=t.service_type
  WHERE s.account_pk=NEW.sender_account_pk AND t.account_pk=NEW.target_account_pk
   AND s.service_type=NEW.service_type AND s.status='ACTIVE' AND t.status='ACTIVE'
 ) THEN RAISE(ABORT,'invalid request accounts') END;
END;
CREATE TRIGGER request_terminal BEFORE UPDATE ON friend_requests BEGIN
 SELECT CASE WHEN OLD.status<>'PENDING' OR NEW.status='PENDING'
  OR OLD.request_id<>NEW.request_id OR OLD.sender_account_pk<>NEW.sender_account_pk
  OR OLD.target_account_pk<>NEW.target_account_pk OR OLD.service_type<>NEW.service_type
  OR OLD.verification_message<>NEW.verification_message OR OLD.created_at<>NEW.created_at
 THEN RAISE(ABORT,'invalid request transition') END;
END;
CREATE TABLE friendships (
 account_a INTEGER NOT NULL REFERENCES accounts(account_pk),
 account_b INTEGER NOT NULL REFERENCES accounts(account_pk),
 created_at TEXT NOT NULL,
 PRIMARY KEY(account_a,account_b), CHECK(account_a<account_b)
);
CREATE INDEX friendship_reverse ON friendships(account_b);
CREATE TRIGGER friendship_valid BEFORE INSERT ON friendships BEGIN
 SELECT CASE WHEN NOT EXISTS (
  SELECT 1 FROM accounts a JOIN accounts b ON a.service_type=b.service_type
  WHERE a.account_pk=NEW.account_a AND b.account_pk=NEW.account_b
   AND a.status='ACTIVE' AND b.status='ACTIVE'
 ) THEN RAISE(ABORT,'invalid friendship accounts') END;
END;
CREATE TRIGGER friendship_immutable BEFORE UPDATE ON friendships BEGIN
 SELECT RAISE(ABORT,'friendship pair is immutable');
END;
CREATE VIEW friend_edges AS
 SELECT account_a AS owner,account_b AS friend FROM friendships
 UNION ALL SELECT account_b AS owner,account_a AS friend FROM friendships;
CREATE TABLE friend_remarks (
 owner_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 friend_account_pk INTEGER NOT NULL REFERENCES accounts(account_pk),
 remark TEXT NOT NULL,
 updated_at TEXT NOT NULL,
 PRIMARY KEY(owner_account_pk,friend_account_pk), CHECK(owner_account_pk<>friend_account_pk)
);
CREATE TRIGGER remark_valid BEFORE INSERT ON friend_remarks BEGIN
 SELECT CASE WHEN NOT EXISTS (
  SELECT 1 FROM friendships WHERE account_a=min(NEW.owner_account_pk,NEW.friend_account_pk)
   AND account_b=max(NEW.owner_account_pk,NEW.friend_account_pk)
 ) THEN RAISE(ABORT,'remark requires friendship') END;
END;
CREATE TRIGGER remark_update_valid BEFORE UPDATE ON friend_remarks BEGIN
 SELECT CASE WHEN OLD.owner_account_pk<>NEW.owner_account_pk OR OLD.friend_account_pk<>NEW.friend_account_pk
 THEN RAISE(ABORT,'remark pair is immutable') END;
END;
CREATE TRIGGER friendship_clear_remarks AFTER DELETE ON friendships BEGIN
 DELETE FROM friend_remarks WHERE
  (owner_account_pk=OLD.account_a AND friend_account_pk=OLD.account_b)
  OR (owner_account_pk=OLD.account_b AND friend_account_pk=OLD.account_a);
END;
CREATE TABLE mutation_requests (
 request_id TEXT PRIMARY KEY,
 user_id INTEGER NOT NULL REFERENCES users(user_id),
 request_type TEXT NOT NULL,
 request_fingerprint TEXT NOT NULL,
 result_json TEXT NOT NULL,
 created_at TEXT NOT NULL
);
PRAGMA user_version=2;
COMMIT;
