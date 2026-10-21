# MVP2 增量实施与接口合同

基于 e22f0fc 的 MVP1，用户提供任务书为本轮规格；保留旧入口、注册/登录、服务选择与激活。Native inline 实施，按固定协议边界并行开发服务端、Qt 页面、自动验收，文件所有权隔离后统一构建验证。不开发群/聊天。独立 feature/mvp2-friends 分支，不合并 main。

## 实施顺序
1. 基线运行、审计数据库，只读备份现有开发库。
2. v1→v2 新迁移：服务器迁移前 SQLite backup，事务建新增表/索引/触发器，外键/完整性检查；不重建原表。
3. AccountServiceDXX、FriendServiceDXX、RecommendationServiceDXX，MutationRepositoryDXX 包裹事务与持久幂等；数据库依然只在原工作线程使用。
4. dispatcher 依据连接身份及 active 服务授权；新增 takeEvents()，结果出队时真实网络发送 event，事件不计入在途请求数量。
5. TcpClientDXX 新增 eventReceived，事件去重；原响应/超时 API 保留。原 ServiceSelectWindowDXX 增量嵌入工作区（原 mainwindowdxx 模板保留）。
6. M2 18项自动验收与 Qt 操作；MVP1 原测试只调整预期 schema 版本为2，不删断言；迁移测试独立构造v1。
7. Mac原生、Qt Creator 能执行的实测、文档/图/演示、独立审查；只提交 feature 分支。

## JSON合同（UI/测试据此实现）
保留 MVP1 信封。新增请求类型/输入完全使用任务书第5节。所有带 service_type 的好友操作需服务已经开通且当前连接激活，跨服务需两侧已激活。

- account.service.open -> {account:{service_type,display_id},services:[{service_type,display_id,active}]}; 新开服务不自动激活。
- account.binding.get/create/remove -> {bound:bool,qq_display_id:string|null,wechat_display_id:string|null}。create/remove仅基于认证用户的账号，拒绝传入其他账号/user_id。
- friend.search/list/mutual.list/recommend.same/recommend.cross/cross.common -> {items:[{service_type,display_id,nickname,remark,relationship, ...}],total:int,next_cursor:string|null}。relationship 为 SELF/FRIEND/INCOMING_PENDING/OUTGOING_PENDING/NONE；共同好友数 mutual_count，推荐 reason；列表项可附 birthday/location/registered_at，后台 account_pk/user_id 不返回。
- 所有分页 limit 1..20（默认20），cursor 非负十进制 offset 字符串（默认0），稳定按 display_id 排序；同平台推荐按 mutual_count DESC, display_id 排序。
- friend.request.send -> {friend_request_id:string,status:"PENDING"};verification_message 可空字符串，最多240 UTF8字节。
- friend.request.pending -> {items:[{friend_request_id,service_type,sender_display_id,sender_nickname,target_display_id,target_nickname,direction:"INCOMING"|"OUTGOING",verification_message,created_at}],total,next_cursor}; 同时列收/发未决申请，按 created_at,request_id 排序；UI只为INCOMING提供接受/拒绝。
- friend.request.respond -> {friend_request_id,status:"ACCEPTED"|"REJECTED"}，从记录推导服务并验证目标所有权/激活。
- friend.remove -> {removed:true}; friend.remark.set -> {remark:string}，备注最多160 UTF8字节。
- event: {kind:"event",type,event_id,data}。类型采用任务书建议；data.service_type、friend_request_id或change可选。账号事件给该用户全部在线连接；好友事件只给相关用户已激活该服务的连接。事件触发幂等刷新，事实通过重新查询恢复。
- 不存在/未开通目标账号 ACCOUNT_NOT_FOUND；未激活 SERVICE_NOT_ACTIVE；自身申请 FRIEND_SELF_FORBIDDEN；已有关系 FRIEND_ALREADY_EXISTS；对向/同向未决 FRIEND_REQUEST_PENDING；非目标响应 FRIEND_PERMISSION_DENIED；已处理不同correlation重试 FRIEND_REQUEST_ALREADY_RESOLVED。
- 涉及微信的映射要求查询本人有效QQ–微信绑定，候选人也需绑定；包括 WECHAT↔WEIBO，由QQ中介绑定证明，不用昵称猜测。查询本人缺绑定 BINDING_REQUIRED；候选缺绑定/目标未开通则排除。跨服务交集也遵守相同映射可见性。
- 所有 mutation 由(user_id,type,规范JSON data)指纹+全局request_id持久幂等，成功结果与修改同事务；同ID不同用户/type/data REQUEST_ID_CONFLICT。重放不重复事件。错误不持久化，可修复后同ID重试。先授权再重放。
