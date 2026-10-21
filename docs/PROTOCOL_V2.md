# IMPlatform 协议 v2 扩展

复用[MVP1](PROTOCOL.md)的4字节大端长度+UTF-8 JSON，最大payload 65536字节。六个原请求、kind=request/response、request_id、success/data/error均保留；system.ping原protocol_version仍为1表示帧/原信封兼容，本文件v2表示业务扩展。无群组/聊天请求。严重帧错误关闭连接，非法信封/类型返回PROTOCOL_ERROR。

所有request_id为16–64位ASCII字母/数字/连字符/下划线；Qt使用UUID。所有账号操作依据服务器连接的认证主体，不读payload中actor_user_id/user_id作为授权来源。session_id只标识本连接会话，不是可跨连接使用的令牌。

## 请求与成功data

下表省略公共信封。`S`为QQ/WECHAT/WEIBO，好友账号均以(service_type,display_id)辨别，QQ/微博同号也严格分平台。好友读取/写入需已开通且本连接已激活对应服务；跨服务需两侧已激活。绑定设置需已登录，不要求提前激活。

| type | data参数 | 成功响应data |
|---|---|---|
| account.service.open | service_type | account:{service_type,display_id},services:[{service_type,display_id,active}] |
| account.binding.get | {} | bound,qq_display_id,wechat_display_id（未开通一侧为null） |
| account.binding.create | {}，拒绝任何外部账号/主体参数 | 同get；仅当前主体的QQ与微信 |
| account.binding.remove | {} | 同get；只解绑，不销号/删好友 |
| friend.search | service_type,query,limit?,cursor? | items,total,next_cursor |
| friend.list | service_type,limit?,cursor? | items,total,next_cursor |
| friend.request.send | service_type,target_display_id,verification_message? | friend_request_id,status:PENDING |
| friend.request.pending | service_type,limit?,cursor? | items:[申请条目],total,next_cursor |
| friend.request.respond | friend_request_id,decision:ACCEPT或REJECT | friend_request_id,status:ACCEPTED或REJECTED |
| friend.remove | service_type,target_display_id | removed:true |
| friend.remark.set | service_type,target_display_id,remark | remark；空/null清除 |
| friend.mutual.list | service_type,target_display_id,limit?,cursor? | items,total,next_cursor |
| friend.recommend.same | service_type,limit?,cursor? | items（mutual_count、reason）,total,next_cursor |
| friend.recommend.cross | source_service,target_service,limit?,cursor? | items（reason）,total,next_cursor |
| friend.cross.common | source_service,target_service,limit?,cursor? | items（reason）,total,next_cursor |

账号条目字段：service_type,display_id,nickname,birthday,location,registered_at,remark,relationship。relationship为SELF/FRIEND/INCOMING_PENDING/OUTGOING_PENDING/NONE；不返回后台account_pk/user_id、密码派生值、会话token。申请条目字段：friend_request_id,service_type,sender_display_id,sender_nickname,target_display_id,target_nickname,direction(INCOMING/OUTGOING),verification_message,created_at；业务申请ID与TCP correlation ID是两个不同标识。

## 边界和业务语义

- limit为JSON整数1..20，默认20。cursor为0..1000000的十进制字符串或null/缺省（起点0），next_cursor为下一offset字符串或null。首屏可传"0"，空字符串非法。读取使用SQL分页；账号按display_id稳定排序，推荐同平台按共同好友数降序、display_id次序，申请按created_at/request_id次序。offset分页针对查询时快照，不承诺并发变化期间跨页冻结。
- query为1..80 UTF8字节。展示ID精确匹配，或昵称受限文字LIKE（转义%/_/反斜线，不让用户传任意通配规则）。验证消息最多240字节，备注最多160字节；所有文本拒绝内嵌NUL。
- 同平台自加/已有好友/未决申请分别拒绝；若已存在对向PENDING，须先处理它。只有目标主体且服务已激活可接受/拒绝；PENDING不可重入，终态不可逆。接受事务内建立一条min/max规范化无向关系；删除双向解除并清双方备注，保留申请审计。
- QQ/微博映射必须为同一主体且共享展示号，两边目标账号已经开通。任何涉及微信的映射（包括微信↔微博）都要求查询本人及候选人有效QQ/微信绑定；本人缺绑定返回BINDING_REQUIRED，候选缺绑定或目标未开通直接排除。昵称和仅相同数字永远不是映射证明。推荐排除本人、目标服务已有好友、双向待决申请，点击只发目标服务申请。cross.common只返回双方平台都已有的可映射好友，不能与迁移推荐混淆。
- 补开服务重复调用返回已有账号；微信-only首次开通QQ/微博才分配共享号，已有微博再开QQ复用；新服务不自动激活。已关闭账号不在本轮恢复。重复绑定同配对/重复解绑无副作用。

## 持久幂等与失败恢复

所有新增写操作（open/bind/remove绑定、send/respond/remove好友/remark）以全局request_id记录在mutation_requests；规范JSON按对象key排序，指纹含认证user_id、type和data。相同ID不同主体/type/内容返回REQUEST_ID_CONFLICT；相同载荷返回原成功结果且不再执行、不重复发事件。失败结果不入幂等记录，可纠正外部错误后用原ID重试。当前连接的权限/服务激活仍在重放前验证。

事务同时提交业务数据和完整成功结果（含内部通知元数据，但只把result返回给客户端）。账户open的services是原成功响应快照，重放后客户端应重新account.services.list获取当前连接的真实active；不能把旧快照当成自动激活。读取没有强制持久幂等。原auth.register继续使用原registration_requests/KDF指纹，不变更旧注册含义。

响应超时/断线不等于写入失败；Qt页面锁住原输入并保留UUID/载荷，重连、同主体重新登录、激活所需服务后可点“重试原操作”。不重用新UUID重复制造业务。UI持久重启或换主体不保存凭证/未确认写入；可重新查询真实结果。

## 事件

事件信封：kind:"event",event_id（服务端随机128位）,type,data，无request_id。

| type | 触发与接收方 |
|---|---|
| friend.request.new | 新PENDING提交后，发送者与接收者的该服务已激活在线连接 |
| friend.request.resolved | 接受/拒绝提交后，两方该服务已激活在线连接 |
| friend.relationship.changed | 接受/删除向两方，备注修改仅向备注所属用户；该服务须激活 |
| account.service.opened | 实际新增账号后，该主体所有在线连接 |
| account.binding.changed | 实际绑定状态改变后，该主体所有在线连接 |

好友事件data含service_type，申请事件含friend_request_id/status，关系事件change为ADDED/REMOVED/REMARK；账户open含service_type/display_id，绑定事件含bound及本人两个display_id。无后台主键和他人凭证。事件只在commit后排入原网络结果队列，由真正POSIX send发往现有TCP连接；event不扣减在途request计数。断线与旧连接序号严格隔离。

客户端维护最多1024个event_id去重，合并刷新（100ms）避免事件风暴重建整窗；事实始终在数据库，离线目标再次查询pending即可恢复。事件不作为操作重放或持久消息，不承诺断线期间投递、应用级ACK或无限离线事件历史。

## 示例（协议说明值）

申请成功：
```json
{"kind":"request","type":"friend.request.send","request_id":"c90b8275-8898-4a62-af14-16fbd95361e3","data":{"service_type":"QQ","target_display_id":"10002","verification_message":"同学你好"}}
{"kind":"response","type":"friend.request.send","request_id":"c90b8275-8898-4a62-af14-16fbd95361e3","success":true,"data":{"friend_request_id":"be34d63114da0b8f629ed271d610792a","status":"PENDING"},"error":null}
{"kind":"event","event_id":"c3596506c66e431fac6bb88d16e65c89","type":"friend.request.new","data":{"service_type":"QQ","friend_request_id":"be34d63114da0b8f629ed271d610792a","status":"PENDING"}}
```

未激活：
```json
{"kind":"response","type":"friend.list","request_id":"e80f7841-615a-4dc0-95e4-b0206b0c3ca5","success":false,"data":null,"error":{"code":"SERVICE_NOT_ACTIVE","message":"请先确认激活该服务"}}
```

搜索成功：
```json
{"kind":"response","type":"friend.search","request_id":"9e1371cf-c53b-4a4e-907b-11f895b0a760","success":true,"data":{"items":[{"service_type":"QQ","display_id":"10002","nickname":"Bob","birthday":null,"location":null,"registered_at":"2026-10-21T12:00:00Z","remark":"","relationship":"NONE"}],"total":1,"next_cursor":null},"error":null}
```

## 错误码

保留所有MVP1错误码。新增/继续使用：ACCOUNT_NOT_FOUND、SERVICE_NOT_OPEN、SERVICE_NOT_ACTIVE、BINDING_REQUIRED、BINDING_CONFLICT、FRIEND_SELF_FORBIDDEN、FRIEND_ALREADY_EXISTS、FRIEND_REQUEST_PENDING、FRIEND_REQUEST_NOT_FOUND、FRIEND_REQUEST_ALREADY_RESOLVED、FRIEND_NOT_FOUND、FRIEND_PERMISSION_DENIED、VALIDATION_ERROR、REQUEST_ID_CONFLICT、DATABASE_ERROR、AUTH_REQUIRED。无效服务/分页/字段为VALIDATION_ERROR；SQL失败/存储失败为DATABASE_ERROR，异常帧为PROTOCOL_ERROR并可关闭连接。
