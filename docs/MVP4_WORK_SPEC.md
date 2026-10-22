# IMPlatform — MVP 4 开发实施任务书（交付 ChatGPT Work）

版本：1.0 ｜阶段：MVP 4 ｜主题：真实 TCP 单聊、群聊、SQLite 消息历史、离线同步、异步并发 ｜环境：macOS / Qt Creator / Qt 6 Widgets / C++17 / CMake

> 本轮将已有用户/好友/群组系统变成可用的即时通信平台。**本机多个独立 Qt 客户端通过真实 TCP Socket 经一个本地 Server 通信**；不允许共享内存、直接读对方 UI、定时轮询数据库或仅在单窗口模拟聊天来冒充真实通信。本轮不做图片/语音/文件和互联网部署。

## 0. 开工前必须审计真实仓库

- 仓库 `Joyce06joy/oop-project-joyce`；查看并记录 commit、MVP1–MVP3 构建结果、数据库 schema/migration、账号服务会话模型、好友关系/群成员/策略接口、TCP framing/request_id/event 格式、网络线程/DB worker、Qt 主界面、测试报告。**此任务书没有独立核实 MVP3 已实现**；若上轮未完成，明确阻塞并只修必要前提，切勿冒称已通过。
- 以实际仓库兼容性为准；建议 `feature/mvp4-chat` 分支，不清空历史数据库、不能覆盖已有表。若无法访问 GitHub，报告权限问题并交付本地可验证结果，未经授权不强推/合并。
- 保持 C++17、Qt Creator + Qt6 Widgets、CMake、Mac POSIX Socket Server、异步 QTcpSocket Client、SQLite 仅 Server 访问；所有自定义 C++ 类以 `DXX` 结尾且独立 `.h/.cpp`；保留 MVP1/2/3 协议与数据兼容。

## 1. 必须完成功能

1. **QQ/微信单聊**：仅同平台、已开通且本会话已激活的两个有效好友账号可开启/发送单聊；微博不提供即时聊天入口。
2. **QQ/微信群聊**：成员可在所在主群收发文本；QQ群子群聊天只开放在 QQ 管理策略且有效子群成员范围内；微信策略停用 QQ 子群（即使仍保留全部成员数据），不删除聊天历史。
3. **消息持久化**：文本、发送账号、所属会话、服务器时间、稳定 message ID；数据库事务成功后才返回 `stored`，失败不得显示“已发送成功”。
4. **在线实时推送**：Alice→Bob 的单聊和 Alice→群的群聊必须使用 Server 经不同 TCP 连接发送真实 event；对方客户端及时展示，避免 UI 线程被阻塞。
5. **离线与历史**：服务器保存离线期间消息；客户端重新登录后分页拉取历史、按照服务会话区分；可增量同步未收到的消息。所有文本历史均保留，默认分页展示，不一次性加载整个库。
6. **可靠性**：TCP 长度前缀分帧、部分发送/接收、乱序响应 request_id 关联、幂等重试去重、断线/重连恢复、正确处理重复 event、协议参数校验与最大正文长度。
7. **线程安全**：server 网络事件线程与 DB worker 分离；消息经过线程安全任务队列，网络线程负责 socket send 缓冲；Qt 主线程运行 UI 和非阻塞 QTcpSocket 事件。多客户端并发稳定。
8. **UI**：QQ/微信最近聊天列表、单聊和群聊页、文本输入和发送状态、历史向上滚动分页、未读提示、连接状态、失败/未知状态可恢复输入；可多开任意数量客户端并预置 3 个演示账号。
9. **测试/文档**：数据无损迁移，MVP1/2/3 回归，新增单元/集成/GUI 演示证据和开发报告。

## 2. 详细业务语义与权限

### 2.1 会话模型

- `PRIVATE`：同服务无向账号对 `(min(account_pk),max(account_pk))` 唯一对应一个私聊会话。两端均必须属于 QQ 或 WECHAT、已开通、为当前有效好友。分手后的既有历史不自动删除；新消息是否允许按当前好友关系判定（默认不可发）。
- `GROUP`：与一个有效主群唯一关联，群 `service_type` 决定消息服务，不随当前策略转换；成员在发送时必须有效且群未解散。
- `SUBGROUP`：与一个有效 QQ 临时讨论组关联，父群必须是 QQ 服务、当前策略 QQ（禁用微信策略下子群聊天），发送者需同时拥有有效主群及子群成员资格。不因策略切换删除已有历史。
- 强制服务隔离，不能给仅有同名账号或仅匹配展示 ID 的跨服务账号发消息；客户端 `from_account` 字段绝不作为认证依据。

### 2.2 服务器权限与历史可见性

- 所有操作用已认证 session + 当前已激活 service 判断 actor，不能由 Qt UI 是否禁用决定最终权限。
- 普通群成员能发送文本，但不可发送到不是自己成员的群；群解散后不允许新消息。
- 群消息历史：推荐按 `conversation_members` 的加入/退出区间授权，只能读自己有效成员时间段内产生的消息；历史区间需防止退出后通过旧会话 ID 读取未来消息。用户重新加入时形成新成员期次或等价的访问窗口，不向其泄漏非成员期间的消息。
- 单聊历史：参与会话的两个账号有权限读自身历史；即使已删除好友，仍可查看过去历史（但不能发新消息）。
- 群规则切换保留全部已保存的成员与消息数据；只是禁用新发送/操作入口。

### 2.3 消息状态（不可夸大 TCP 成功）

- `pending`：Qt 客户端本地排队/Socket 正在发送。
- `stored`：服务端完成 SQLite COMMIT，响应包含服务器消息 ID、时间。仅此后 UI 可显示“已保存”。
- `delivered`：接收端独立 ACK，Server 持久化接收回执后可回传发送方；TCP `send()` 返回成功**不能**算已送达。
- `read`：接收端明确报告读状态，Server 核实并持久化回执；**若未实现 UI 已读上报则不得显示已读**。
- `unknown`：超时/连接断开时无法判断 Server 是否已提交，客户端用原 request_id 查询或幂等重试；不能简单认为失败或生成新的消息重复发送。
- `failed`：服务器明确拒绝或可以确认没有提交时显示。
- 本轮最低必须支持 pending/stored/unknown/failed；delivered/read 如已设计则作为明确验收项完整实现，不能 UI 假装双勾。建议完整实现 delivered，read 如工作量超过可显式标为未实现且不显示。

### 2.4 离线恢复

- 在线事件推送与历史同步互补：在线 event 不是唯一持久数据依据；客户端记录各会话已知最大消息 ID/同步游标，重新登录或重连后从 DB 拉取遗漏消息。
- 收到同一 `message_id` 多次 event 或历史记录时，UI 只展示一次；服务器保存消息去重必须用 `(sender_account_pk,request_id,operation_type)` 的数据库唯一性。
- 支持断线自动退避重连（有上限与 UI 状态）、会话重新认证或恢复；禁止断线后继续把请求当已成功。
- 保证同一会话中消息以 server `message_id`/服务端时间稳定排序；不承诺跨会话严格时序。

## 3. 数据库新增与迁移 v4

- 新 schema 使用**增量 migration**，保留旧 users/accounts/friends/groups/group_members/subgroups/requests 和既有编号及会话。迁移前备份，运行 SQLite `foreign_key_check`；测试数据库可显式 reset，普通启动不得 wipe。
- `conversations`: `conversation_pk` PK, `service_type`, `kind` PRIVATE/GROUP/SUBGROUP, `group_pk` nullable FK, `subgroup_pk` nullable FK, `created_at`, `status`; 唯一 GROUP 对应 group、SUBGROUP 对应 subgroup；PRIVATE 单独存 canonical pair / 关联表并确保组合唯一。
- `private_conversation_participants`: `(conversation_pk,account_pk)`，恰好两有效同服务参与者；推荐 `private_pairs(conversation_pk,account_a,account_b)` 及 `CHECK(account_a<account_b)`、UNIQUE(service_type,account_a,account_b)（跨表服务合法性由服务器验证）。
- `conversation_members`: 角色/成员进入离开有效期，`conversation_pk`,`account_pk`,`joined_at`,`left_at`；同账号重新入群需能表示多个 membership epochs，避免覆盖旧区间。
- `messages`: `message_pk` PK, `conversation_pk` FK, `sender_account_pk` FK, `request_id`, `content` UTF-8, `content_type='TEXT'`, `created_at` UTC; UNIQUE(sender_account_pk,request_id,operation_type) 可按现有去重策略保存；`CHECK` 非空及长度限由服务端 UTF-8 字节验证。
- `message_receipts`: `(message_pk,recipient_account_pk)` 组合唯一, `delivered_at`,`read_at` nullable UTC；存每接收者状态；不允许其他用户伪造回执。
- 增量同步/离线游标可使用 `message_sync_cursors`，但不得以“已发送 event”代替 DB 对接收者的持久记录。
- 索引：`messages(conversation_pk,message_pk)`，`messages(sender_account_pk,request_id)`，`conversation_members(account_pk,conversation_pk)`，`message_receipts(recipient_account_pk,delivered_at)`；复核 SQLite 写锁与 DB worker 执行串行事务。
- 每条消息在发送端幂等、消息保存和必要回执初始行生成的事务中一致；写库 COMMIT 失败不返回存储成功。

## 4. TCP 协议 v4（与旧版兼容）

- 保留现有 length-prefix 大端 4 字节、UTF-8 JSON、64 KiB 帧上限、request_id 与统一 response/event 规范，禁止引入第二条偷偷直接传聊天的 Socket 协议。
- 新 Request 建议：`chat.private.open`, `chat.private.send`, `chat.group.send`, `chat.history.get`, `chat.conversation.list`, `chat.sync`, `chat.message.ack`, `chat.message.read`（已读有真实实现时启用）。
- Event 建议：`chat.message.new`, `chat.message.delivered`, `chat.message.read`, `chat.conversation.updated`。
- 服务端 `send` 请求校验 payload：`to_account_pk` 或目标展示 ID+service；`group_pk/subgroup_pk`；`content` UTF-8 文本；`request_id` 唯一；不能信任客户端 `sender_id`。
- 典型发送请求 envelope 示例（可根据仓库现有字段命名调整，但语义须一致）：

```json
{
  "type": "chat.private.send",
  "request_id": "req-alice-0001",
  "data": {"service": "QQ", "to_account": "10002", "content": "Hello Bob!"}
}
```

- 成功 response 仅在 DB COMMIT 后发送：`message_id`, `conversation_id`, `created_at`, `status: "stored"`；若服务器在提交后崩溃/响应丢失，重复同 request_id 返回原消息 ID 和已存储结果。
- Bob 收到 `chat.message.new` 事件，独立 `event_id/message_id` 用于去重，Bob 的消息来源来自服务器认证记录；Bob 的 ACK 必须由其本人会话认证且只对其授权会话有效。
- 历史请求使用 `conversation_id`, `before_message_id` 或 `after_message_id`, `limit <= 50`；严格身份鉴权、总量边界与稳定排序。
- 错误码至少：`CHAT_NOT_FRIENDS`, `CHAT_NOT_MEMBER`, `CHAT_CONVERSATION_NOT_FOUND`, `CHAT_POLICY_DISABLED`, `CHAT_MESSAGE_TOO_LARGE`, `CHAT_INVALID_CONTENT`, `CHAT_SEND_NOT_ALLOWED`, `CHAT_CURSOR_INVALID`, `SESSION_EXPIRED`, `DATABASE_ERROR`。
- 网络实现必须处理半包/粘包、partial send、write buffer、发送队列限流与断线清理；请求/响应可乱序，不得依赖一次 recv 对应一个 JSON。

## 5. 线程模型与 UI 响应约束

- Qt UI 线程管理 QWidget 并用 `QTcpSocket` 的 `connected/readyRead/disconnected/errorOccurred` 异步处理网络事件；不得在 GUI 线程调用 `waitForReadyRead()`、`sleep()` 或同步等待 DB 结果。
- Server POSIX poll 网络事件线程负责所有 client FD/read/write buffers；单独 DB worker 处理查询/持久化任务；通过线程安全队列提交处理并向网络线程投递结果。不要跨线程无锁访问 socket/client session。
- 当网络发送缓冲区爆满，定义每连接写队列上限和踢除/流控策略，避免单客户端无限占内存；不允许 DB worker 阻塞等待客户端读取。
- 同时在线多个客户端至少验证 Alice、Bob、Carol；断开其中一个不影响另外两个；服务器退出能安全关闭 socket、数据库连接、回收线程。
- Qt 页面离开/关闭时不得让异步回调悬挂；重连后不得重复注册信号连接造成消息出现两次。

## 6. 建议 C++ 类与 Qt 页面（从现有仓库演进）

| 建议类 | 职责 |
| --- | --- |
| `ConversationDXX` | 私聊/群聊/子群会话实体 |
| `MessageDXX` | message ID、sender、content、UTC time、状态 |
| `MessageServiceDXX` | 服务端鉴权、创建会话、消息保存、推送对象选择 |
| `MessageRepositoryDXX` | 消息/会话/回执/同步 DB 操作 |
| `ChatPageDXX` | 对话列表、消息气泡、发送框、历史分页 |
| `MessageModelDXX` | Qt 列表展示、按 message ID 去重、状态变更 |
| `ChatControllerDXX` | 客户端消息请求和 response/event 关联，UI 不直写 SQL |
| `TcpClientDXX` | 延用既有单 Socket、异步解析和事件信号 |
| `ConnectionManagerDXX` | Server 查找同一账号活跃连接及事件分发 |
| `DatabaseWorkerDXX` | 复用现有 DB worker，任务完成回调返回网络线程 |

公开接口语义可包括 `openPrivateConversation(target)`, `listConversations(service,cursor)`, `sendPrivateMessage(target,content,requestId)`, `sendGroupMessage(groupOrSubgroup,content,requestId)`, `getHistory(conversation,before,limit)`, `syncMessages(afterCursor)`, `ackDelivered(messageId)`，服务端一律从 session 推导 actor。可复用既有类，不应机械拆成大量空壳类。

## 7. Qt 交互页面需求

- 登录后左侧 QQ/微信有“消息”导航；微博不出现聊天页。中栏最近会话、右栏聊天消息与输入栏（建议 QSplitter）；显示登录平台账号。
- 联系人详情点击“发消息”打开/创建对应单聊；群详情“进入群聊”；子群仅在当前策略允许且用户有效成员时可进入。
- 消息显示 pending/stored/unknown/failed 状态，服务端 ACK 到来更新本地草稿项，而不是无条件追加第二条；接收消息按 ID 去重与排序；离线状态可明显看到。
- 进入会话先拉最近 50 条，向上滚动加载更早页；失败保留输入；更换聊天窗口保留未发送草稿（尽量实现，若不做说明）。
- 发送文本有长度上限提示、空白消息不能发送；若传输失败，仅展示准确状态和可安全重试操作。收到其他会话消息提示未读数量；不可在没有回执证据时显示“已读”。
- 全功能能在同一 Mac 打开 IMServer 和三个 IMClient 真实演示（每个 Qt 客户端独立进程/独立 TCP 连接）。

## 8. 测试用例（逐项 Pass / Fail / Not Run，附证据）

先回归全部现有 MVP1–MVP3 测试，再至少运行：

- M4-T01：三个独立 Qt 客户端与本地服务器建立真实不同 TCP 连接，正常登录。
- M4-T02：QQ Alice/Bob 已成好友，Alice 发文本，Bob 通过 server event 实时收到；服务器 DB 有唯一消息。
- M4-T03：微信好友单聊同样可用；微博发送/开启聊天请求被服务器拒绝。
- M4-T04：非好友、跨服务、未激活服务、伪造 sender 身份发送被拒，不能留下成功消息。
- M4-T05：QQ/微信群内成员发送消息，其他在线群成员各只收到一次；非群成员无法发送/读取。
- M4-T06：QQ 子群仅对子群有效成员广播，非子群成员不会看到消息。
- M4-T07：QQ 子群切微信策略时不能继续发送、但旧消息与成员数据保留；切回 QQ 后权限恢复。
- M4-T08：群成员退出或被踢后不能发送新群消息，且无法读取退出后的群消息。
- M4-T09：Bob 离线期间 Alice 发消息保存；Bob 重登后能同步所有漏收，顺序正确且不重复。
- M4-T10：重启服务端、客户端，完整文本历史仍可分页查询（含跨几百条消息的情况）。
- M4-T11：TCP 半包、粘包、partial send、多报文缓冲不丢消息/不错误解析。
- M4-T12：同一 request_id 重发不会重复保存/重复显示，收到原 message ID。
- M4-T13：服务端数据库写失败时不发 `stored`，UI 显示错误且 DB 无部分消息。
- M4-T14：DB COMMIT 后服务器响应丢失，客户端重试原 request_id 能查到原消息，不重复提交。
- M4-T15：多个客户端并发群发、单发，UI 不冻结，server 无死锁/竞态崩溃。
- M4-T16：服务器断开/重启，Qt 显示断开和恢复状态，重连后查漏补缺。
- M4-T17：历史查询越权（任意会话 ID、退群后新消息、跨平台）被拒，返回有限数据。
- M4-T18：正确的送达回执只能由目标账号确认，未 ACK 时不显示已送达；不伪造已读。
- M4-T19：UTF-8 中文/英文/emoji 正确保存和展示；超长文本/无效 JSON 被安全拒绝。
- M4-T20：旧数据库迁移后账户、好友、群组及管理员/子群规则无损，MVP1–3 全部回归通过。
- M4-T21：Qt Creator/CMake macOS 可构建、IMServer + 3×IMClient 本机可重复演示真实 QQ 单聊、微信单聊、群聊、子群、离线与历史。
- M4-T22：服务端优雅关闭，任务队列、Socket 和线程正确结束；慢客户端或大量消息有队列限流行为。

测试报告必须区分真实自动化、手工 GUI、模拟故障和未运行；不可只写“全部通过”无执行证据。socket 通讯至少使用 server/client 日志或集成脚本证明经网络而非本进程模拟。

## 9. 交付物

1. MVP4 增量源码，保留 MVP1–MVP3 可运行代码；git commit / PR / patch 状态；不得自行改默认分支历史。
2. `docs/MVP4_IMPLEMENTATION_REPORT.md`：审计、设计/改动文件、线程/通信数据流、已完成与遗留问题、下一轮 UI 优化建议。
3. `docs/DB_SCHEMA_V4.md`：conversation/message/receipt 表、索引、权限和历史访问区间、migration/备份恢复。
4. `docs/PROTOCOL_V4.md`：所有 chat request/response/event、错误码、幂等/重试/历史同步方式、帧边界。
5. `docs/MVP4_TEST_REPORT.md`：MVP1–MVP3 回归 + M4-T01–M4-T22 的逐项证据，测试命令与结果。
6. `docs/UML_MESSAGE_NETWORK.md`：消息类图、序列图（发送→事务→推送→ACK）、线程模型与信号槽路径。
7. `demo/`：可显式初始化 Alice/Bob/Carol、QQ/微信好友关系、群、子群及聊天测试用数据，提供恢复方法（普通启动不清空数据库）。
8. README：Mac 安装依赖、Qt Creator CMake 构建、Server 和多客户端启动、演示单聊/群聊/离线同步、常见 Socket 错误排查。

## 10. Work 实施顺序与停止条件

1. 只读审计 MVP1–MVP3 当前代码与执行情况；列出可复用 FriendService/GroupService/Policy/DB worker/Protocol 接口。
2. 实现增量 schema、会话模型和消息仓储，添加相关 DB 单测与迁移回归。
3. MessageService 服务器身份/好友/群成员/策略校验，消息存储及幂等，使用协议集成测试。
4. 接入 TCP 协议 v4，在线 push、客户端 ACK、离线补偿和历史分页；处理半包/粘包。
5. Qt `ChatPageDXX`、会话列表、消息列表、发送状态、网络断开重连。
6. 三客户端本地联调、并发和异常注入、所有 MVP1–MVP4 测试。
7. README/ER/UML/实施与测试报告更新。
8. MVP4 达标立即停止：**不提前做 MVP5 视觉重构或 MVP6 最终答辩报告**。

---

## 可复制给 Work 的直接执行指令

请按附件《IMPlatform MVP 4 开发实施任务书》在私有仓库 `Joyce06joy/oop-project-joyce` 上增量开发聊天模块，先审计 MVP1–MVP3 真实完成度及接口、数据库、Qt、协议、测试，不假设各轮都通过。维持 macOS + Qt Creator + Qt6 Widgets + C++17 + CMake + POSIX Socket Server + QTcpSocket Client + SQLite，所有自定义类名 DXX 后缀。实现 QQ/微信真实 TCP 单聊、群聊与 QQ 子群聊天、Server 端好友和群策略授权、消息 DB 持久化与幂等、在线推送、历史分页和离线重连同步；Qt 不阻塞，支持三进程客户端同机演示。新增无损 DB migration、v4 协议、消息和网络类图及测试。运行 MVP1–3 回归及 M4-T01–M4-T22，提供 Pass/Fail/Not Run 证据；明确不能声称 `send()` 成功即送达，不能未确认就显示已读。只完成 MVP4，不提前实施 MVP5/6。
