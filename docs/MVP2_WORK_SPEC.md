# IMPlatform — MVP 2 开发实施任务书（交付 ChatGPT Work）

版本：1.0 ｜阶段：MVP 2 ｜开发环境：macOS / Qt Creator / Qt 6 Widgets / C++17 / CMake ｜本轮仅新增账号服务扩展、好友、推荐与界面整合

## 0. 执行前提：审计 MVP 1，保留已有成果

Work 首先读取 GitHub 私有仓库 `Joyce06joy/oop-project-joyce` 的实际最新代码、README、schema、测试与 `docs/MVP1_IMPLEMENTATION_REPORT.md`（如存在）。用户已表示第一轮完成，但**本任务书没有独立核验现有仓库具体实现**。严禁猜测既有接口、表结构、构建结果或测试状态；以仓库中真实实现为准适配增量修改。

执行前输出一份简短审计报告：当前 commit、目录树、IMServer / IMClient 构建与运行方式、MVP1 协议和会话方式、数据库迁移策略、Qt 页面及网络回调机制、可复用类；指出与本文假定不一致的地方和兼容处理方法。若无法访问仓库，不得声称修改了仓库；应报告权限问题，并在可用工作空间完成最大可验证部分。不要重写或覆盖 MVP1 功能，不得自动清空已存在的数据库；迁移前备份本地开发数据库。代码修改使用独立 `feature/mvp2-friends` 分支（如已有 Git 工作流允许），完成后提交或交付 PR/补丁并说明状态。未经用户授权不要强推或合并默认分支。

## 1. 全局约束与范围

- 保持单机 `127.0.0.1:8888` TCP Server + 多个 Qt Widgets Client；POSIX Socket 服务端、QTcpSocket 异步客户端，TCP 4 字节大端帧长度 + UTF-8 JSON、最大帧 64 KiB（若 MVP1 已冻结兼容协议则优先兼容，规范记录新增字段）。
- 全部自定义 C++ 类名以 `DXX` 结尾，独立 `.h` 和 `.cpp` 文件；遵守封装、组合、分层，不为了表现继承而滥用继承。
- 数据只由服务器访问；UI 不执行 SQL，Network 不实现业务权限规则；所有业务结果由 Server 认证会话并授权，不能信任请求携带的 user_id / from_account_id。
- MVP2 必须完成：用户补开服务、查看服务列表；QQ–微信手动绑定/解绑；同服务好友搜索、申请、接受/拒绝、删除、备注、列表及资料；同平台共同好友；跨服务好友迁移推荐；Qt 导航页面；通过 TCP 主动推送好友申请与关系变更，离线后能查询未处理申请。
- 继续复用 MVP1 统一身份登录、服务激活。QQ 与微博共享展示号码但好友关系独立；微信拥有独立 ID。微博本轮支持好友功能，但不提供聊天。
- **本轮不得实施** 群组、子群、动态规则切换、单聊、群聊、消息历史、花哨的在线状态系统；不得提供可点击但无效的功能按钮。
- 第一轮所有测试与已有功能必须继续正常；若改动兼容性必须补回归测试。

## 2. 用户故事和完整业务语义

### 2.1 开通服务

- 已登录用户在账号页选择未开通服务，确认后提交；服务端必须验证认证用户身份。
- 用户的 QQ 与微博展示号码使用同一编号。如已有微博后补开 QQ，必须复用；如原来仅有微信，首次补开 QQ 或微博才安全分配共享号码。微信首次开通单独分配微信编号。
- 已开通服务重复提交返回已有账号而非产生副本；先关闭/再开通若仓库已支持应有明确定义；本轮不新增账号注销业务。
- 新开通服务需单独执行服务激活确认，不能将“已开通”混为“当前会话已激活”；开通成功前不能显示为成功。注册/服务开通编号分配必须遵守事务与并发一致性。

### 2.2 QQ–微信显式绑定

- 同一个 `user_id` 同时持有 QQ 与微信账号时，可以在账户设置中发起手动绑定；必须校验账号归属均为当前登录用户。本轮**不允许跨注册主体的任意 QQ/微信绑定**，避免复杂所有权证明和安全问题；如未来需跨用户绑定另行设计验证流程。
- 每侧最多一个有效绑定，一对一；重复绑定相同配对可幂等返回已绑定，冲突配对拒绝；解绑只删除绑定关系，不注销账号、不删除平台好友及已保存备注。
- 必须区分：内部 `user_id` 用于确认数据归属，`account_bindings` 用于**是否允许通过显式 QQ ↔ 微信映射进行跨服务推荐**。即使统一用户主体可识别，用户解绑后跨 QQ/微信的推荐映射不能继续依赖隐藏的同一主体自动恢复；微信与 QQ 的迁移推荐以有效绑定为前提。此条优先化解此前设计中身份识别和手动解绑的冲突。
- QQ/微博因共享社交号码体系，可通过同一 `user_id` 与展示编号进行映射（只对当前已开通账户）；所有推荐均仅使用已开通目标服务的账号，不自动替人开通。

### 2.3 好友搜索 / 添加 / 审核

- 仅查询所选服务已开通账号。按展示 ID 精确搜索、按昵称进行受限模糊搜索（分页与结果上限，例如每页 20 条）；不以昵称判断唯一身份。不得把 password_hash、session token 或后台主键无意义地暴露给 UI。
- 同服务内加好友前必须检验双方有效账号、不能加自己、不能重复已有好友或有效未决申请。
- 发送申请包含目标账号、可选短验证消息，状态 `PENDING`；对方在线时服务器主动推送事件，离线时下次进入申请页可取回。
- 接受/拒绝只有目标账号持有者（且相应服务已激活）可执行；接受在**同一事务**内将申请置为 `ACCEPTED` 并建立唯一无向好友关系；拒绝状态 `REJECTED` 且不建立好友。
- 允许针对同一对账户双向发申请：服务器必须定义确定行为，推荐在有对向待处理申请时提示先处理对向申请，避免双 `PENDING`；任何重试均不得制造重复关系。
- 必须对竞争条件（重复点击、两个窗口同时操作、申请接受和删除竞态）提供服务器事务/唯一性约束，统一错误码。已处理申请不得重复推进状态。
- 删除好友采用双向解除关系，双方当前好友列表相应更新；不会删除其他平台好友关系、申请审计及以后加入的聊天历史。删好友不是删用户。
- 修改备注是单向的：Alice 给 Bob 改备注不改变 Bob 昵称，也不改变 Bob 对 Alice 的备注。空备注表示清除，不删除好友。

### 2.4 同平台共同好友

- 给定已激活的平台、当前用户与另一个**同平台**账号，取双方好友内部账号集合的交集。前端只展示当前用户可见、查询允许的共同好友，返回数量及分页列表。账号必须按 `(service_type, account_pk)` 区分，不得将 QQ 与微博相同展示 ID 视为同一个平台账号。
- 推荐同平台可能认识的人：好友的好友，排除本人、已是好友、已存在待处理申请的账号；可按共同好友数量降序、展示 ID 作稳定二级排序。需要限制最大结果数。

### 2.5 跨服务好友迁移推荐

- 用户选择来源服务 S 和目标服务 T，必须均已开通且本会话已激活；若未激活提示激活，不自动帮用户更改会话状态。
- 遍历 S 的好友，将好友的**同一注册主体**上的目标平台账号进行可信映射；若映射涉及 QQ↔微信，必须存在该候选人的有效 QQ–微信绑定，同时本用户自己的 QQ–微信映射也需满足显式绑定条件以启用这项跨平台推荐；QQ↔微博可以通过共享主体编号映射。组合涉及微信的情况均须遵守有效绑定；不可用昵称或仅相同数字展示号猜测身份。
- 只推荐已开通 T、不是目标服务当前账号自身、与本人尚非 T 好友且没有待处理 T 好友申请的候选人；去重、分页、稳定排序。结果显示目标服务与目标展示 ID、推荐理由（如“你的 QQ 好友”）。
- 点击结果只是发起目标服务好友申请，绝不直接复制好友关系。解绑或取消服务后推荐实时变化。
- 明确区分“跨服务好友交集”：同一用户在来源与目标服务上已建立好友的真实主体交集；和“迁移推荐”：在来源已有、目标还没有的可映射账户。

## 3. 建议类/组件（以现有 MVP1 实际接口为准）

- `FriendshipDXX`、`FriendRequestDXX`：纯 C++ 领域实体及状态枚举（枚举不需 DXX 后缀）。
- `FriendServiceDXX`：搜索授权、申请状态机、接受/拒绝、删除、备注、列表。
- `RecommendationServiceDXX`：同平台共同好友、好友的好友推荐、跨服务好友交集与迁移推荐。
- `FriendRepositoryDXX`（或已有 Repository 扩展）：数据库查询与原子写入；数据操作必须在数据库工作线程执行。
- `AccountServiceDXX`：增加 `openService`、`bindQQWeChat`、`unbindQQWeChat`、`getBinding`、`listServices`；尽量保持 MVP1 方法签名兼容。
- `FriendPageDXX`：搜索、列表、申请、备注、删除确认。
- `FriendRequestPageDXX`：待处理申请及同意/拒绝。
- `RecommendationPageDXX`：来源/目标服务选择、共同好友和推荐结果。
- `AccountPageDXX`：补开服务、显示 ID、绑定/解绑。
- `MainWindowDXX`：统一侧栏 QQ/微信/微博切换与导航；可复用 MVP1 现有登录和服务选择 UI，不要盲目替换。
- `TcpClientDXX`：添加请求关联和服务端 event 分派接口；单一客户端连接复用，不为每次操作重连。
- `RequestDispatcherDXX`：新增类型路由，只负责解析/鉴权前置/调度和响应组装，不塞复杂 SQL。

建议接口语义（不是强制字面 C++ 签名）：`searchAccounts(service, query, cursor)`、`sendFriendRequest(target, message)`、`respondToFriendRequest(requestId, accept)`、`listFriends(service, cursor)`、`removeFriend(target)`、`setFriendRemark(target, remark)`、`getMutualFriends(service, target)`、`recommendSameService(service)`、`recommendCrossService(source,target)`、`openService(type)`、`bindAccounts()`、`unbindAccounts()`。所有服务接口应接收**由服务器会话解析的认证主体/激活服务上下文**，绝不信任客户端传来的 actor_user_id。

## 4. 数据库增量迁移与约束

根据实际 MVP1 的 migration 机制创建**新的增量迁移**，不可删除或重建 `users`、`accounts` 及其 ID 编号计数；正式迁移支持从已有 MVP1 数据无损升级、重复执行安全（通过版本表跟踪，禁止重复执行破坏性 SQL）。迁移前备份验证、外键开启且一致性检查通过。

至少增加以下数据结构（字段可依既有架构调整）：

### `account_bindings`
- `binding_id` PK；`qq_account_pk` FK accounts；`wechat_account_pk` FK accounts；`created_at` UTC。
- 分别对 `qq_account_pk`、`wechat_account_pk` 做 UNIQUE，保证一对一。
- 业务检查账户服务类型必须为 QQ/WECHAT、同一 `user_id`、有效开通；解绑删除对应记录（未来若需审计，可加 `revoked_at` 并制定 active-unique 约束）。

### `friend_requests`
- `request_id` PK（业务好友申请 ID，不等于 TCP 请求 correlation ID）；`service_type`；`sender_account_pk`、`target_account_pk` FK；`verification_message`；`status` CHECK(`PENDING`,`ACCEPTED`,`REJECTED`,`CANCELLED`)；`created_at`、`resolved_at` UTC。
- CHECK(sender != target)，状态变化从 PENDING 到终态且不可逆；同一无序账号对在同一服务最多一个活跃 PENDING，可使用规范化有序对或部分唯一索引实现。

### `friendships`
- `account_a`、`account_b` FK，CHECK(account_a < account_b)，联合 PK，`created_at` UTC；服务端操作建立关系前校验两账户在同一服务且已开通。

### `friend_remarks`
- `owner_account_pk`、`friend_account_pk` FK；`remark`；`updated_at` UTC；联合 PK；备注只有在对应好友关系存在时才可设置。删除好友时按事务清理双方备注（或明确不展示的历史行为）。

### 推荐索引
- `accounts(service_type,display_id)`（若 MVP1 已有唯一索引不重复创建），`accounts(user_id,service_type)`。
- `friend_requests(target_account_pk,status,created_at)`、`friend_requests(sender_account_pk,status)`。
- `friendships(account_a)`、`friendships(account_b)`；`friend_remarks(owner_account_pk)`。

数据库 SQL 对无法单独约束的跨表服务类型一致性、主体归属、一对一绑定必须由事务业务逻辑验证，并以自动化测试覆盖。保留合理一致性检查与 FK 检查。

## 5. TCP 协议 v2 扩展（兼容 MVP1）

复用 MVP1 `kind`/`type`/`request_id`/`data`、响应 `success/error`；所有事件 `kind:"event"`、有独立 `event_id` 和 `type`、`data`。原有 `auth.*`、`account.services.list`、`account.service.activate`、`system.ping` 不得失效。

新增请求建议：
- `account.service.open` — `{service_type}`
- `account.binding.get` — `{}`
- `account.binding.create` — `{}`（基于当前登录用户现有 QQ/微信账号；不可任意传入他人 ID）
- `account.binding.remove` — `{}`
- `friend.search` — `{service_type,query,limit,cursor}`
- `friend.list` — `{service_type,limit,cursor}`
- `friend.request.send` — `{service_type,target_display_id,verification_message}`
- `friend.request.pending` — `{service_type,limit,cursor}`
- `friend.request.respond` — `{friend_request_id,decision:"ACCEPT"|"REJECT"}`
- `friend.remove` — `{service_type,target_display_id}`
- `friend.remark.set` — `{service_type,target_display_id,remark}`
- `friend.mutual.list` — `{service_type,target_display_id,limit,cursor}`
- `friend.recommend.same` — `{service_type,limit,cursor}`
- `friend.recommend.cross` — `{source_service,target_service,limit,cursor}`
- `friend.cross.common` — `{source_service,target_service,limit,cursor}`

推送事件建议：`friend.request.new`、`friend.request.resolved`、`friend.relationship.changed`、`account.service.opened`、`account.binding.changed`。在线连接推送是 UI 刷新优化，不是业务事实存储：事件丢失时通过重新查询与断线重连恢复；重复事件客户端安全去重或幂等刷新。

协议响应具体 JSON 字段需在 `docs/PROTOCOL_V2.md` 列出至少一组成功与失败示例；尽量保留前向可扩展的 `data` 结构、稳定错误码。绝不可仅基于昵称操作账号；推荐结果返回 `service_type` 和 `display_id`、展示昵称及适当推荐原因。

错误码建议：`ACCOUNT_NOT_FOUND`, `SERVICE_NOT_OPEN`, `SERVICE_NOT_ACTIVE`, `BINDING_REQUIRED`, `BINDING_CONFLICT`, `FRIEND_SELF_FORBIDDEN`, `FRIEND_ALREADY_EXISTS`, `FRIEND_REQUEST_PENDING`, `FRIEND_REQUEST_NOT_FOUND`, `FRIEND_REQUEST_ALREADY_RESOLVED`, `FRIEND_NOT_FOUND`, `FRIEND_PERMISSION_DENIED`, `VALIDATION_ERROR`, `DATABASE_ERROR`, `AUTH_REQUIRED`。必须保留 MVP1 错误码与异常路径。

请求在 TCP 重试中可能重复：每个具有副作用的请求需要幂等处理，尤其是 `account.service.open`、绑定/解绑、申请发送、申请处理、删除和备注。优先复用 MVP1 幂等机制并扩充为通用 mutation request 记录（存储请求指纹与结果或已稳定业务 ID），或借助唯一约束 + 明确幂等结果；不能因服务端写入成功但响应丢失而重复发送申请。记录指纹时不泄漏凭证，不允许让相同 request_id 不同载荷重试被当作相同操作。正常读取/搜索无需强制持久化幂等记录。

## 6. Qt 界面与操作规范

- 登录后采用一个 `MainWindowDXX`，左侧 QQ/微信/微博空间切换，主区域以 `QStackedWidget` 等方案展示“好友”“申请”“推荐”“账号”页面。可渐进替换 MVP1 的简易服务选择页，必须保持登录/注册和服务激活流程有效。
- 显示每个服务真实展示 ID、是否开通/会话激活；未开通服务提示开通，已开通未激活服务提示激活确认，不能悄然切换到未授权业务。
- 搜索栏在无结果、输入错误和网络异常时保留查询词；分页或加载更多；每个搜索结果显示服务标识、展示 ID、昵称、当前好友/申请状态。
- 申请发送后显示 PENDING；被申请方在线立即看到通知，离线重新登录可以在申请页面查询；同意/拒绝请求等待服务器确认后才更新本地最终状态。
- 已存在好友可以修改自己的备注、删除好友（危险操作前确认），列表变化实时刷新；服务端拒绝时恢复原显示，不误报成功。
- 推荐页可选来源与目标；如果 QQ–微信绑定条件不足，清楚提示“请先绑定 QQ 与微信账号”，并可跳到设置；界面解释推荐理由及共同好友数量。推荐条目可点击发送目标服务好友申请。
- Qt 主线程不可调用同步等待、数据库操作或 Socket 阻塞读；只通过 `QTcpSocket` 异步事件；收到 server push 后刷新对应页、必要时显示轻提示，不强制弹模态对话框打断用户。
- 所有网络操作具有 loading/错误状态；关闭窗口时正确清理网络对象；消息无关页面不要强制重建整窗；正确表单字段不因错误丢失。
- 保持混合视觉风格：管理页偏工作台式列表/详情，保留后续聊天页扩展空间。未开发的群聊入口隐藏或明确禁用，不做假按钮。

## 7. 测试与验收（逐项 Pass / Fail / Not Run + 证据）

先运行 MVP1 原有 T01–T14 回归测试，记录任何回归。新增测试至少：

- M2-T01：MVP1 仅微信账号补开 QQ，获得共享号码；再开通微博与 QQ 号码一致；服务重复开通不产生第二账号。
- M2-T02：已有 QQ+微信的同一用户绑定成功，查询绑定存在，解绑成功；账号、好友数据不受影响；不同主体绑定被拒绝。
- M2-T03：QQ 与微博展示 ID 相同但在搜索、好友列表中严格分属不同服务。
- M2-T04：QQ Alice 搜索 QQ Bob，成功；搜索未开通 QQ 的人不会错误显示可添加 QQ。
- M2-T05：Alice 向 Bob 发送申请，数据库记录为 PENDING；Bob 在线时收到服务器 event（抓包/日志/客户端事件证据）。
- M2-T06：Bob 接受申请后，Alice 和 Bob 均能查询到一条无向好友关系；数据库只有一个 `(min_pk,max_pk)` 记录。
- M2-T07：Bob 拒绝申请后双方不成为好友；重复接受或拒绝已完成申请有稳定错误或幂等反馈。
- M2-T08：自己加自己、重复申请、重复好友、跨服务申请和未经授权操作均被服务端拒绝。
- M2-T09：Alice 修改 Bob 的备注，Bob 的昵称和反向备注保持不变；删除好友同步解除关系并处理备注。
- M2-T10：构造 A–B、A–C、B–C 等关系，计算同平台共同好友正确，好友的好友推荐排除自己、已加及待处理。
- M2-T11：Alice 的 QQ 好友 Bob 在可确认绑定情况下推荐 Bob 的微信账号；Alice 微信若已加 Bob，不再推荐；解绑后关联推荐不再出现。
- M2-T12：任一参与微信映射的账号未有效绑定、目标微信未开通、来源/目标服务未激活时，不泄漏/错误推荐，不自动开通或添加好友。
- M2-T13：双方在线时申请/接受/删除通知实时推送；目标离线时申请保存，重新登录后可查询并处理；事件重复不造成重复操作。
- M2-T14：网络断开、数据库异常/只读时不误报成功，UI 不冻结，输入与可恢复状态保留。
- M2-T15：重启 server 和客户端后，好友、申请、备注、绑定、服务开通记录持久化；普通启动不清空数据。
- M2-T16：多客户端并发请求、同一申请双重处理、双向同时申请、同一 mutation request_id 重试均不重复建立关系或错误推进状态。
- M2-T17：旧 SQLite MVP1 数据通过版本迁移无损升级，外键检查通过、未丢用户和登录能力。
- M2-T18：完整 Mac Qt Creator 构建，服务器 + 3 客户端本机演示可复现；编译测试和实际 GUI 行为分别说明证据。

优先自动化测试 domain/service/repository/protocol，GUI 部分列详细手工步骤并执行能执行的部分。严禁将未运行用例报告为通过。测试数据应可重置、操作应针对单独测试数据库；普通启动不得自动清空生产演示数据。

## 8. 交付物

1. 保留 MVP1 可编译运行源码 + MVP2 增量源码；顶层 Qt Creator/CMake 一键构建 IMServer 和 IMClient。
2. 新数据库 migration 与 `docs/DB_SCHEMA_V2.md`（关系、字段、约束和索引）。
3. `docs/PROTOCOL_V2.md`（新增请求/响应/事件、示例、错误码、幂等策略）。
4. `docs/MVP2_IMPLEMENTATION_REPORT.md`（工作摘要、commit/PR、文件变化、已完成与未完成、架构与语义变更、未解决问题、MVP3 预留）。
5. `tests/` 新增自动化测试与 `docs/MVP2_TEST_REPORT.md`，含 MVP1 回归 T01–T14 与本轮 M2-T01–T18 逐项 Pass/Fail/Not Run、执行命令、输出摘要。
6. `demo/` 可重置 Alice、Bob、Carol（推荐再加 David、Emily）账号及不同服务/好友演示场景，说明账号凭据只适用于本地演示；重置命令仅显式执行才清空测试数据。
7. README 更新：macOS 依赖安装、Qt Creator 构建、迁移、先启动服务端、三个客户端登录与好友申请实时演示步骤。
8. 更新 Mermaid UML/ER 图，至少覆盖账号/绑定、FriendService/RecommendationService、申请状态机、好友数据库表。不得把设计草图冒充与当前代码一致的实测架构。

## 9. Work 执行顺序与停止条件

1. 只读审计 MVP1 源码与测试，输出本轮增量实施清单和协议兼容计划。
2. 创建新迁移、仓储及相关测试；先验证 MVP1 数据无损升级。
3. 实现服务开通和绑定、解绑，并通过服务器接口测试。
4. 实现好友搜索、申请状态机、接受/拒绝、删除、备注；先跑业务测试。
5. 实现共同好友、同平台与跨服务推荐，覆盖绑定、去重与分页。
6. 复用同一 TCP 连接完成协议路由和 server event 推送。
7. 完成 Qt 主窗口、服务导航、好友、申请、推荐与账号页，并完整联调。
8. 运行全量回归与新增测试，记录真实证据，提交文档。
9. 到 MVP2 完成即停止；**不要自行开始 MVP3 群组、多态和 MVP4 聊天**。

Work 可以依据已实现 MVP1 的实际架构调整目录/类签名，但不得无说明地更改本文的业务语义、持久化保证、Socket 真实性、DXX 命名、Mac 技术栈和数据保留要求。如果 MVP1 本身未完整达到基线，先列出阻塞项，仅修复影响本轮必需的基础功能，并对未修部分明确列示。

---

## 给 Work 的直接执行指令

请将附件《IMPlatform MVP 2 开发实施任务书》作为本轮验收标准，在私有仓库 `Joyce06joy/oop-project-joyce` 上进行增量开发。首先只读审计实际 MVP1 源码、schema、Qt 窗口、网络协议、测试和已知缺陷；不要假设当前实现与规划完全一致。保持 macOS + Qt Creator + Qt 6 Widgets + C++17 + CMake + POSIX Socket Server + QTcpSocket Client + SQLite。完成补开服务、QQ/微信手动绑定、好友搜索及申请审核、删除备注、共同好友和跨服务推荐、Qt 导航及实时 Socket 通知。保证数据库迁移无损、旧功能回归、请求权限与幂等、界面异步和数据一致性。运行 MVP1 回归与 M2-T01–M2-T18，给出 Pass/Fail/Not Run 和证据；更新协议/ER/UML/README/实施报告。不要启动群组或聊天开发，不要声称未执行的测试通过。
