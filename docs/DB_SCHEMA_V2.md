# 数据库 schema v2

迁移文件：`database/migrations/002_friends.sql`。启动使用 `PRAGMA user_version`：0空库先初始化原v1再应用v2；1先备份/校验再迁移；2不重复执行；其他版本拒绝。不会通过DROP/重建 users/accounts/id_counters/registration_requests 升级，也不重置编号。新SQL在CMake生成内嵌资源，不依赖启动工作目录。

## 备份与恢复

服务端获得数据库旁独占flock后，检查quick_check与foreign_key_check；已有v1库通过sqlite3_backup生成完整一致的快照（包含已提交WAL内容），备份名 `<path>.v1-backup.sqlite3`；若存在则用 `-1`、`-2` 等新名，使用O_EXCL防覆盖。备份校验成功才执行迁移。备份失败则拒绝升级。迁移在BEGIN IMMEDIATE/COMMIT中进行，异常rollback；旧表和旧业务数据留存。再次启动已v2库不生成新备份、不再次建表。

如需人工回退，先停止持有数据库的服务器，保留当前v2文件及WAL/SHM作为副本，再以已验证v1快照配置另一个`--db`路径运行旧MVP1服务器；不要将旧服务端指向v2库。本轮不自动降级，不自动覆盖原库。

执行前实际对本地data/manual.sqlite3及mvp1-demo.sqlite3另做只读快照到data/backups/mvp2-preflight；自动验收使用独立构造的v1库，验证迁移前后用户、密码派生值、号码、计数器和注册幂等逐字一致且仍可登录。未把仅新建v2库冒充迁移测试。

## 原表保持

- users：64位user_id、NOCASE唯一login_name、带盐KDF、昵称/生日/所在地、可空共享号码、UTC注册时间。
- accounts：用户FK、服务类型/号码/status/开通时间；UNIQUE(user_id,service_type)与UNIQUE(service_type,display_id)。
- id_counters：SOCIAL_SHARED、WECHAT两空间，不重置。
- registration_requests：原注册request_id、指纹、用户FK与时间，继续支持旧注册重放。

## 新表、约束与索引

| 表 | 字段与关键约束 |
|---|---|
| account_bindings | binding_id PK；qq_account_pk、wechat_account_pk各UNIQUE/FK；created_at。INSERT触发器检查服务类型、ACTIVE与同user_id；不可UPDATE，解绑显式DELETE。 |
| friend_requests | request_id业务UUID PK；service_type CHECK；sender/target账号FK且不相同；verification_message；PENDING/ACCEPTED/REJECTED/CANCELLED CHECK；created_at/resolved_at状态一致。 |
| friendships | account_a、account_b FK联合PK，CHECK(a<b)，created_at；INSERT触发器检查同服务/ACTIVE；禁止改变已建关系的配对。 |
| friend_remarks | owner/friend账号FK联合PK，remark、updated_at；仅关系存在才可INSERT，不能UPDATE更换所属账号；删除好友触发清理双方备注。 |
| mutation_requests | request_id全局PK，user_id FK，request_type、request_fingerprint、result_json、created_at；成功结果与业务事务同提交。结果JSON含仅服务端使用的通知目标元数据，不对客户端暴露。 |

friend_requests 的部分唯一表达式索引 `(service_type,min(sender,target),max(sender,target)) WHERE status='PENDING'` 防止双向同时产生两个未决申请。目标/状态/时间与发送者/状态有查询索引。friendships联合主键覆盖account_a，另建account_b反向索引。friend_remarks联合主键覆盖owner；不重复增加等效索引。

`friend_edges` 是无向好友的双向只读视图，为共同好友与推荐SQL提供清晰查询边界。账户原唯一索引继续复用。

申请UPDATE触发器只允许 PENDING→终态，且身份、服务、验证消息与创建时间不可变；终态不可重新推进。接受在同事务插入一个规范化无向关系并更新申请；拒绝只更新申请。关系删除保留申请审计，清除两侧备注。账号/用户不会因此删除。

跨表授权由 SocialRepositoryDXX/FriendServiceDXX/AccountServiceDXX 在数据库工作线程验证，触发器构成额外一致性保障，不代替服务端会话校验。UI不访问SQLite。
