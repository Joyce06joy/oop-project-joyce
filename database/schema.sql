PRAGMA foreign_keys=ON;
BEGIN IMMEDIATE;
CREATE TABLE users (
 user_id INTEGER PRIMARY KEY,
 login_name TEXT NOT NULL COLLATE NOCASE UNIQUE,
 password_hash TEXT NOT NULL,
 nickname TEXT NOT NULL,
 birthday TEXT,
 location TEXT,
 shared_social_id TEXT UNIQUE,
 registered_at TEXT NOT NULL
);
CREATE TABLE accounts (
 account_pk INTEGER PRIMARY KEY,
 user_id INTEGER NOT NULL REFERENCES users(user_id),
 service_type TEXT NOT NULL CHECK(service_type IN ('QQ','WECHAT','WEIBO')),
 display_id TEXT NOT NULL,
 status TEXT NOT NULL CHECK(status IN ('ACTIVE','CLOSED')),
 opened_at TEXT NOT NULL,
 UNIQUE(user_id,service_type),
 UNIQUE(service_type,display_id)
);
CREATE TABLE id_counters (
 namespace TEXT PRIMARY KEY CHECK(namespace IN ('SOCIAL_SHARED','WECHAT')),
 next_value INTEGER NOT NULL CHECK(next_value>0)
);
INSERT INTO id_counters VALUES ('SOCIAL_SHARED',10001),('WECHAT',8001);
CREATE TABLE registration_requests (
 request_id TEXT PRIMARY KEY,
 request_fingerprint TEXT NOT NULL,
 user_id INTEGER NOT NULL REFERENCES users(user_id),
 created_at TEXT NOT NULL
);
PRAGMA user_version=1;
COMMIT;
