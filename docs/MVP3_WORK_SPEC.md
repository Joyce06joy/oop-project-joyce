# IMPlatform — MVP 3 开发实施任务书（交付 ChatGPT Work）

版本：1.0 ｜阶段：MVP 3 ｜主题：QQ群/微信群管理、成员权限、QQ 子群与运行时动态多态 ｜开发环境：macOS / Qt Creator / Qt 6 Widgets / C++17 / CMake

> 本轮在 MVP1、MVP2 真实实现的基础上增量开发。**不要开始消息单聊/群聊**：MVP4 才接入聊天。本轮所有群功能须通过真实 TCP 请求操作服务器 SQLite，不允许 UI 层假模拟。项目仍只在同一台 Mac 本地运行。

## 0. Work 执行前的仓库审计（必做）

- 仓库：`Joyce06joy/oop-project-joyce`（private）。首先查看最新 commit、目录树、Qt UI、CMake targets、数据库 schema/migrations、TCP 帧协议、认证与服务激活、MVP2 实现和测试报告。
- 用户表示 MVP1 已完成；本文件**未核验 MVP2 已完成，更未核验仓库真实代码**。如 MVP2 未完成，先报告阻塞并只修复 MVP3 所需基础能力，不能宣称 MVP2 已通过。
- 先输出审计摘要：现有类与复用接口、当前 DB 版本、请求/响应/event 格式、数据库线程与网络线程交互方式、构建命令、真实测试证据、计划改动文件清单。
- 新增 `feature/mvp3-groups` 分支（如果适配仓库 Git 工作流）；禁止清空原数据库或重建原 schema，迁移前备份本地演示数据库。没有仓库权限时明确报告，不得假称 push/commit 完成。
- 所有自定义类名 `DXX` 结尾并以独立 `.h/.cpp` 存放；维持 C++17、Qt 6 Widgets、CMake、POSIX Socket Server、QTcpSocket 异步客户端、SQLite Server-only、TCP 长度前缀 UTF-8 JSON，保留现有协议兼容性。数据和授权均由服务器负责。

## 1. 本轮范围与 Definition of Done

**必做：**
1. QQ/微信群创建、查询列表/详情、群编号（演示预置 1001–1006）、退出群、解散群、成员列表、踢人、群资料/公告修改。
2. QQ 群：用户申请加入；群主/管理员审批；群主任命、撤销管理员（不能任命自己或非法对象）；群主和管理员可踢普通成员和维护公告；QQ 临时子群独立成员集合，且始终为父群成员子集。
3. 微信群：当前群成员可邀请其他已开通同服务账号（受邀者确认加入）；**只有群主拥有管理特权**（踢人、公告修改等）；普通成员邀请不视为管理特权；不支持设置管理员或子群。
4. **动态多态**：同一 `GroupDXX` 使用抽象 `GroupPolicyDXX` 的虚方法，运行时在 `QQGroupPolicyDXX` / `WeChatGroupPolicyDXX` 间切换，成员、群主、stored_role、子群和历史元数据不丢；群的平台归属 `service_type` 永不改变。
5. QQ 原管理员角色在微信策略下保留于数据但失去管理权限；已有 QQ 子群在微信策略下停用并隐藏/明确标记，不可操作；恢复 QQ 策略后恢复权限与子群。对于原属微信群：切换到 QQ 规则时如何限制或启用子群，统一按策略执行，但服务归属仍为微信，任何成员都必须是该服务账号。
6. 全部操作端到端 Qt → TCP → Server → Service/Policy → SQLite；审批与规则切换通知通过真实 Socket event 推送给在线相关客户端；离线成员重连后查询服务端权威状态。
7. 数据库无损迁移，测试、文档、演示数据；MVP1/MVP2 回归。

**本轮不做：**私聊、群聊消息、离线聊天历史、媒体传输、互联网部署、复杂群层级或群转让。可为 MVP4 预留 conversation 关联接口，但不能自行建设未要求的聊天 UI。

## 2. 不可歧义的业务规则

### 2.1 群标识与服务

- `group_pk` 数据库内部全局主键；`group_number` 面向用户、可按平台设唯一约束 `(service_type,group_number)`；种子群可采用 1001–1006，正式新群号事务安全分配，不能假设固定六个群。
- 群属 QQ 或 WECHAT。微博本轮不支持群。每个群成员的 account PK 必须属于群的**原属平台**且是已开通账号。
- `policy_type` 可在 QQ/WECHAT 之间切换；表示权限逻辑，不是转换平台或迁移账号；不允许靠客户端自行指定有效权限。
- 群主是一个有效群成员（持久化 owner_account_pk 且与 group_members 保持一致）。创建群的账号自动加入并成为群主。
- 群公告和昵称修改权限：QQ 为群主/管理员，微信为仅群主；群名称修改、解散、策略切换仅群主。本轮不实现转让群主，群主不能直接退出而留下无主群，应提示先解散（或明确禁止退出）。

### 2.2 QQ 申请制

- 已开通 QQ 且不在群内的用户可提交 PENDING 入群申请。群主或当前策略授权的管理员可接受/拒绝。
- 接受需同事务写入成员和变更请求状态。重复接受、重复申请、并发审批不可引入重复成员。管理员不得批准微信策略下的 QQ 群申请。
- 切换至微信策略时原未决 QQ 入群申请保留，但不能通过旧审批逻辑继续完成；可显示为“策略变更后待处理/失效”，推荐服务端显式标记 `CANCELLED_POLICY_CHANGED` 或由查询显示不可处理，并在切回时明确处理策略（本版建议状态取消，不自动复活），不得默默自动加入。

### 2.3 微信邀请制

- 所有当前有效群成员可以邀请未入群的本服务账号；邀请创建 `PENDING` 邀请记录，由目标受邀账号本人接受/拒绝；发邀请者不能替目标直接确认加入。
- 普通成员不能踢人、授予管理员权限、修改公告、解散群或切换规则。
- 用户未开通所属群的平台，或跨平台账号参与群操作，一律拒绝。
- 切换为 QQ 策略时遵循 QQ 入群规则；已发出未处理邀请在策略切换后取消（或拒绝新确认），避免旧规则下越权入群。

### 2.4 踢人/退出/解散

- 禁止踢出群主；普通管理员不能踢群主或另一管理员（推荐权限规则）；群主可以踢管理员或普通成员；微信规则只允许群主踢非群主成员。
- 普通成员可主动退出；退出或被踢时应原子地清理其所有该群子群的有效成员关联，保留可审计记录（如设计已有历史状态）；不能删用户账号或其他群关系。
- 解散群仅群主发起，要求确认。可以采用软删除状态 `DISBANDED` 保留历史记录/日志；在完成前保证所有关系操作不可继续，避免悬空记录。

### 2.5 QQ 子群

- `SubgroupDXX` 是**临时讨论组**，不是完整独立主群，归属于唯一 parent group；创建权建议仅群主（前面权限交互原型已采用此规则）。可以新增/移除子群成员，但操作者授权要明确：本版由主群群主或子群创建者管理，若创建者已退出主群则无权限。
- 子群成员必须是有效主群成员；初始自动包含创建者。数据表外键和服务端事务共同保障，防止竞态。
- 主群应用微信管理策略时 `canCreateSubgroup=false` 且现有子群不可查询敏感内容、不可改成员、不可发消息（MVP4）；管理员与子群数据不删除。
- 父群解散时子群同步失效；成员退出父群时不可留在其子群。

### 2.6 动态多态规则（课程答辩重点）

- 采用 `GroupDXX` **组合**抽象 `GroupPolicyDXX`；两个具体策略**继承并 override**权限/流程方法；所有实际操作由 `GroupServiceDXX` 调用虚函数，不能堆成 `if (type==QQ)` 分支伪装多态。
- 基类至少包含：`canRequestJoin(context)`, `canInvite(context)`, `canApproveJoin(context)`, `canKick(actor,target,context)`, `canManageAnnouncement(context)`, `canCreateSubgroup(context)`；权限判断应使用身份、角色、目标、群状态，不能仅由 UI bool 参数控制。
- 切换执行：鉴权（仅群主）→ 检查预期 `policy_version` → DB 事务更新类型/版本并处理失效请求 → 提交成功 → 服务器更新策略对象并推送事件。若提交失败，保持旧策略和 UI 状态；若并发操作，使用同群版本校验或事务串行化保证新旧规则不交错越权。
- 不可序列化策略指针到 DB；重启时从 `policy_type` 使用工厂生成策略对象。保存/恢复后群成员集合与子群集合保持不变。

## 3. C++ 类与职责（按真实仓库接口适配）

| 建议类 | 职责 |
| --- | --- |
| `GroupDXX` | 群稳定实体，组合策略引用及成员集合；无 UI/SQL |
| `GroupMemberDXX` | account PK、stored role、joined_at |
| `GroupPolicyDXX` | 抽象虚方法与虚析构；纯业务规则 |
| `QQGroupPolicyDXX` | QQ 申请/审批/管理员/子群行为 |
| `WeChatGroupPolicyDXX` | 微信邀请/仅群主行为 |
| `GroupPolicyFactoryDXX` | 按 policy_type 创建策略，不必依赖 Qt |
| `GroupRequestDXX` | 请求/邀请状态机 |
| `SubgroupDXX` | 临时子群元数据和成员约束 |
| `GroupServiceDXX` | 所有群操作授权、规则调用、事务边界 |
| `GroupRepositoryDXX` | 群/成员/请求/子群的持久化接口/实现 |
| `GroupPageDXX` | Qt 群列表、详情、成员/申请/邀请 |
| `GroupPolicyDialogDXX` | 规则切换确认与影响展示 |
| `SubgroupPageDXX` | 子群列表与成员管理 |

接口语义建议：`createGroup(service,name)`、`getGroups(service)`、`getGroupDetail(group)`、`applyToJoin(group)`、`inviteMember(group,target)`、`respondGroupRequest(requestId,decision)`、`setAdmin(group,target,enabled)`、`kickMember(group,target)`、`leaveGroup(group)`、`disbandGroup(group)`、`setAnnouncement(group,text)`、`createSubgroup(group,name,memberIds)`、`updateSubgroupMembers(subgroup,changes)`、`switchPolicy(group,targetPolicy,expectedVersion)`。全部 actor 信息从服务器已认证/激活服务 session 获取；绝不可由客户端传入 actorRole 就生效。

## 4. 数据库迁移（严禁破坏 MVP1/MVP2 数据）

- 创建新版本 migration，并记录 schema version。迁移前对已有数据备份；迁移后执行 `PRAGMA foreign_key_check`，回归登录/好友/绑定。
- `groups`: `group_pk` PK，`service_type`，`group_number`，`name`，`owner_account_pk` FK，`policy_type`，`policy_version` 非负，`announcement`，`status`，`created_at`；唯一 `(service_type,group_number)`，合法枚举由 CHECK/业务确保。
- `group_members`: `(group_pk,account_pk)` 联合 PK/FK，`stored_role` OWNER/ADMIN/MEMBER，`joined_at`；确保 owner 在成员表且唯一有效 OWNER；应避免循环外键导致创建群困难（创建时同事务处理）。
- `group_requests`: `request_pk`，`group_pk`，`kind` APPLY/INVITE，`initiator_account_pk`，`target_account_pk`，`status` PENDING/ACCEPTED/REJECTED/CANCELLED_POLICY_CHANGED，`created_at`,`processed_at`；待处理唯一性通过索引/业务条件实现。
- `subgroups`: `subgroup_pk`，`parent_group_pk`，`name`，`creator_account_pk`，`status`，`created_at`。
- `subgroup_members`: `(subgroup_pk,account_pk)` 联合唯一，joined_at。必须用事务和约束/触发器保证有效子群成员是有效父群成员。
- `mutation_dedup`（如 MVP1/2 已有请求幂等存储则复用）：`actor` + `request_id` + operation type 的唯一键，重复提交同一请求返回相同结果，不能重新创建成员或子群。
- 数据读写只发生在服务器 DB worker；所有跨表不变量由同一任务/事务完成；编写必要索引：`groups(service_type,group_number)`、`group_members(account_pk)`、`group_requests(group_pk,status)`、`group_requests(target_account_pk,status)`、`subgroups(parent_group_pk)`。
- 若 MVP2 已存在 `groups` 旧表，则做兼容迁移和数据转换，不直接 DROP；必要时展示旧表到新表映射与回滚办法。

## 5. TCP 协议扩展 v3

- 复用现有统一 envelope、4 字节大端长度 + UTF-8 JSON、最大帧限制、request_id、authenticated session、status/error code，保持旧 `auth.*` 和 `friend.*` 命令兼容。
- 请求（以此前设计名称为建议，须和仓库协议约定统一）：`group.create`、`group.list`、`group.detail`、`group.join.apply`、`group.invite`、`group.request.list`、`group.request.respond`、`group.member.kick`、`group.member.leave`、`group.admin.set`、`group.announcement.update`、`group.disband`、`group.subgroup.create`、`group.subgroup.list`、`group.subgroup.members.update`、`group.policy.switch`。
- Event：`group.request.new`、`group.members.changed`、`group.policy.changed`、`group.subgroup.changed`、`group.disbanded`；离线后查询服务器状态，event 不作为唯一数据来源。仅向有权限的相关会话推送；不能泄漏其他群资料。
- 所有变更带 request_id 并保证幂等；`group.policy.switch` 请求携带 `group_pk`, `target_policy`, `expected_policy_version`，回应新版本以及确切状态；过期版本返回 `GROUP_VERSION_CONFLICT`。
- 错误码至少包括：`GROUP_NOT_FOUND`, `GROUP_NOT_MEMBER`, `GROUP_PERMISSION_DENIED`, `GROUP_ALREADY_MEMBER`, `GROUP_REQUEST_PENDING`, `GROUP_INVALID_TARGET`, `GROUP_POLICY_DISABLED`, `GROUP_VERSION_CONFLICT`, `GROUP_DISBANDED`, `DATABASE_ERROR`。
- 避免使用展示群号作为服务器内部唯一寻址键；如协议展示编号则还需 service_type。服务端检查每条请求的服务已开通且当前 session 已激活。

## 6. Qt 页面与用户流程

- 在 MVP2 `MainWindowDXX` 左侧 QQ / 微信服务空间中新增“群组”入口；微博不显示群功能。群列表可按关键字/编号查询，详情显示群主、人数、公告、当前规则、操作按钮与申请/邀请状态。
- QQ：申请→等待审核→审批通知→列表更新；管理员的权限由服务端返回，可用 UI 禁用不具备的入口但不能仅靠禁用完成鉴权。
- 微信：成员发邀请→目标用户待处理列表→目标接受→群成员实时更新；未经邀请不能通过直接构造网络请求加入。
- 群主高级设置有明确“切换群管理模式”入口，操作前展示：现有成员数量保持不变、原管理员权限暂时停用、子群暂时停用、待处理请求的处理规则。必须二次确认。
- 请求过程提供 Loading 和具体错误提示；失败不得假刷新为成功；正确输入不被清空；断线后提示连接状态并允许重试；支持窗口缩放和 Mac Retina。

## 7. 自动化与手工验收清单（必须逐项 Pass / Fail / Not Run + 证据）

先回归 MVP1、MVP2 现有测试，新增至少 20 项：

- M3-T01：QQ 创建群，自动加入群主，持久化后编号唯一。
- M3-T02：微信创建群，同样持久化；微博建群服务端拒绝。
- M3-T03：QQ群非成员申请、群主审批成功，成员只出现一次。
- M3-T04：QQ 管理员接受/拒绝申请；普通成员无法审批。
- M3-T05：微信普通成员邀请目标，目标自行接受后加入；目标拒绝不加入。
- M3-T06：微信群无邀请的直接加入被拒；非群主踢人被拒。
- M3-T07：QQ群主授予/撤销管理员；非群主改角色被拒。
- M3-T08：QQ群管理员可以踢普通成员，但不能踢群主/另一管理员。
- M3-T09：微信群仅群主能修改公告、踢人、解散；普通成员邀请权限不等于管理权限。
- M3-T10：群主不能不经处理直接退出留无主群；解散后其他群操作被拒。
- M3-T11：QQ 创建子群，独立成员列表且全为父群成员；非主群成员加入子群被拒。
- M3-T12：主群成员退出/被踢，所属子群有效成员关系同步清理。
- M3-T13：从 QQ 规则切为微信规则，主群成员数量与成员 ID 一致，管理员权限被停用，子群数据仍存在但不能操作。
- M3-T14：切回 QQ 规则，原管理员权限恢复且原子群成员恢复，不需要重新创建。
- M3-T15：重启 Server/Client 后 policy_type、stored_role、子群和成员数据一致，策略虚函数行为符合保存的类型。
- M3-T16：未经授权请求切换策略、伪造群主/角色、跨服务账号调用被拒。
- M3-T17：策略切换与踢人/审批并发，不会在新规则下执行旧权限操作；过期 version 拒绝。
- M3-T18：DB 只读或注入失败时创建群/接受申请/切换策略不误报成功，事务回滚并保持 UI 权威状态。
- M3-T19：多个客户端实时收到审批/邀请/成员/策略变化事件；离线重连后能加载正确状态。
- M3-T20：重复请求、双重审批、成员并发操作不重复写入；重复 request_id 返回一致结果。
- M3-T21：旧 MVP2 数据无损迁移且 MVP1 注册登录、MVP2 好友推荐/绑定仍正常。
- M3-T22：Mac Qt Creator/CMake 构建 IMServer/IMClient，三个客户端本地展示两种群规则和切换；记录可复现步骤/视频或日志证据。

测试中须证明**真正的虚函数动态派发**（例如同一 Group 对象切换策略后操作行为改变），不能只通过 UI 文案切换作为“多态”证据。包括 domain/DB/protocol 单元及集成测试；未跑 GUI 则 Not Run。

## 8. 交付清单

1. 增量源码与 git commit/PR/补丁状态，不能无授权强推或合并。
2. `docs/MVP3_IMPLEMENTATION_REPORT.md`：审计、涉及文件、功能对照、偏离原因、遗留问题、可供 MVP4 复用的群成员和权限查询接口。
3. `docs/DB_SCHEMA_V3.md`、新 migration 与数据恢复策略、ER 图（标出子群父群 FK 和角色）。
4. `docs/PROTOCOL_V3.md`：请求响应 event、错误码、版本冲突和幂等机制。
5. `docs/MVP3_TEST_REPORT.md`：M1/M2 回归以及 M3-T01–T22 逐项 Pass/Fail/Not Run、命令与证据。
6. `docs/UML_GROUP_POLICY.md` 或 Mermaid 图：抽象基类、两种策略、组合、多态调用链、群组服务与数据访问接口。
7. `demo/`：可显式重置的演示数据；例如 Alice 群主、Bob 管理员、Carol 普通成员、1001 QQ 主群、1004 微信群、QQ 临时子群。
8. README：Mac 启动与操作顺序、QQ 申请审批/微信邀请、切换前后权限验证与数据恢复说明。

## 9. Work 实施顺序与停止条件

1. 只读审计和兼容性差异记录；确定具体迁移与类接口。
2. GroupPolicy 纯 C++ 设计及虚函数测试，证明动态多态。
3. schema 增量迁移、群仓储及数据库事务测试。
4. 群、成员、申请/邀请、管理员与子群业务规则；先 API 测试。
5. 策略切换事务、版本控制、挂起请求处理和重启恢复。
6. TCP 新协议路由与在线 event 通知。
7. Qt 群组页面与高级策略切换 UI，真实联调。
8. 回归、并发、异常、演示数据、文档交付。
9. 达成 MVP3 即停止，**不得自行开发 MVP4 聊天**。

---

## 可复制给 Work 的直接执行指令

请阅读附件《IMPlatform MVP 3 开发实施任务书》，在仓库 `Joyce06joy/oop-project-joyce` 已实现的 MVP1/MVP2 基础上增量开发群组模块。先审计真实 commit、schema、协议、Qt 界面和测试，不要假设 MVP2 已完成。实现 QQ 申请/审批/管理员/临时子群、微信群邀请制和仅群主管理特权，并以 `GroupPolicyDXX` 抽象基类和 `QQGroupPolicyDXX`/`WeChatGroupPolicyDXX` 派生类实现真实动态多态；切换策略时必须保留成员/角色/子群数据且 DB 事务和并发校验正确。全部功能经 Qt→TCP→Server→SQLite，保持 Mac + C++17 + Qt Widgets + POSIX Socket 与 DXX 命名。新增 migration、协议 v3、Qt 群界面、推送和演示数据。执行旧测试回归及 M3-T01–M3-T22，逐项列出 Pass/Fail/Not Run 和证据；交付实现报告、ER/UML、README。不要自行开发聊天模块。
