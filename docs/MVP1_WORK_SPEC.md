# IMPlatform — MVP 1 开发任务书（供 ChatGPT Work 执行）

版本：1.0｜阶段：MVP 1｜环境：macOS + Qt Creator + Qt 6 Widgets + C++17 + CMake｜状态：允许开始 MVP 1 编码，不允许自行扩展到后续模块

## 0. 任务与工作方式

你（Work）是本项目的执行工程师。请先读取仓库现状、用户上传的《2025-面向对象程序课程设计.doc》及本任务书，再实现 MVP 1。仓库：`Joyce06joy/oop-project-joyce`（Private；需要已授权的 GitHub 访问）。若仓库无法访问，**不要假装读取成功，也不要修改其他仓库**；明确说明权限障碍，可在可用工作空间生成工程文件并报告未推送状态。默认不覆盖现有文件、不重写已有成果；先执行检查，再以最小增量实施。若仓库已有代码，先报告目录树、构建状况、拟复用/新增文件。

执行顺序：环境审计 → 最小工程骨架 → 领域与数据 → TCP Server → Qt Client → 集成测试 → README / 交付报告。每一步必须尽可能实际构建/运行/测试，不能将“看起来合理”冒充“已测试”。允许按实际工程需要微调内部实现，任何影响业务语义或接口协议的变更须在交付报告中解释。

## 1. 项目背景及全局冻结约束

项目是 C++ OOP 课程设计：模拟 QQ、微信、微博整合社交平台，在一台 Mac 本机运行一个服务器和任意多个 Qt 客户端（标准演示 3 个客户端）。课程核心：封装、组合、继承、动态多态；所有**自定义 C++ 类**名称必须以 `DXX` 结尾且拥有独立声明/实现文件（可对模板、纯 DTO、无状态枚举酌情处理，但不允许自定义类缺后缀）。Qt 可视化和真实 TCP 通信均为本项目的必做范围，即使原任务书视 Socket 为选做。保留 Mac + Qt Creator 环境；禁止直接使用仅限 Windows 的 WinSock2 代码。

长期功能边界（**不在 MVP 1 内开发，但架构不得阻碍**）：
- 统一注册身份；QQ/微博共享展示号码，微信独立号码；QQ/微信可手动绑定和解绑；统一密码登录并选择已开通服务；已开通其他服务经过确认可联动登录。
- 好友经申请与同意建立，同服务好友独立维护；好友删除/备注/查询；跨服务迁移推荐 + 同平台共同好友推荐；同一注册主体辨别归属，跨服务显式关联必须尊重手动绑定状态。
- QQ 群申请入群、管理员、临时子群（子群成员来自主群）；微信群成员均可邀请、仅群主管理；群规则经动态多态在 QQ/微信之间切换，成员及原管理员、子群数据保留，停用不兼容权限并可恢复。群原始服务归属不随管理规则切换。
- QQ/微信均有单聊、群聊；微博主要是好友管理；完整文本聊天历史、分页、离线同步；服务端 POSIX socket，客户端 QTcpSocket 异步，TCP 长度前缀 UTF-8 JSON；最终网络线程和数据库线程隔离。
- 客户端 UI 为 Qt Widgets 单主窗口、左侧 QQ/微信/微博切换，聊天 IM 风格 + 管理工作台混合样式；SQLite 仅由服务器访问。

## 2. MVP 1 边界

**MVP 1 只实现**：
1. `IMServer` 本机监听 `127.0.0.1:8888`，支持至少 3 个同时连接的 Qt 客户端；正常启动、关闭、断线检测。
2. `IMClient` Qt Widgets：连接状态、注册窗口、登录窗口、登录成功后的简易服务选择/账号资料界面；不要求完整主窗体导航。
3. 统一注册：输入唯一登录名、昵称、密码及确认密码、出生日期（可选）、所在地（可选），选择至少一种服务（QQ/WECHAT/WEIBO）；校验并保存；后台自动分配 `user_id`、展示 ID、UTC 注册时间；成功页面展示新账户 ID。
4. 服务编号：QQ 与微博共享同一 **编号空间**，同一用户的两个账号号码相同；微信独立编号空间；先开通微博后开通 QQ 仍复用共享编号（后续阶段）；注册时只有选择的服务生成开通账号。为确保没有 QQ/微博账号但只有微信用户后续能开通 QQ，建议在 users 上保存可空的 `shared_social_id`，或使用独立的共享号码分配表，保持事务性。
5. 统一账号密码登录：用 `login_name` + 密码验证服务器用户；成功后查询所有已开通服务并返回，允许选择进入的服务，服务器记录当前已启用的服务。后续联动确认/绑定行为预留接口，但 MVP 1 不必实现复杂 UI。
6. SQLite 持久化：服务器独占数据文件；事务提交成功后才报告注册成功，失败整体回滚；重启服务器后原数据存在。
7. 基础错误：字段校验、重复登录名、错误密码、服务器未启动、连接丢失、数据库不可写、重复请求、协议错误。正确输入不得因其他字段出错而清空。
8. 测试与交付：可构建源码、README、数据库 schema/迁移、测试用例和测试报告、3 用户可重置的演示数据生成机制（不会在正常启动时重置数据）。

**MVP 1 不实现**：好友、推荐、群管理、动态策略、单聊、群聊、消息历史、完整账号绑定、复杂设计系统。不要为未开发模块制作功能性假按钮，也不要声称完成全部课程任务。

## 3. 推荐代码结构（可依据仓库现状微调，说明理由）

```
IMPlatform/
  CMakeLists.txt
  README.md
  client/
    main.cpp
    ui/LoginWindowDXX.h/.cpp
    ui/RegisterWindowDXX.h/.cpp
    ui/ServiceSelectWindowDXX.h/.cpp
    network/TcpClientDXX.h/.cpp
  server/
    main.cpp
    network/TcpServerDXX.h/.cpp
    network/ClientSessionDXX.h/.cpp
    network/RequestDispatcherDXX.h/.cpp
    application/AuthServiceDXX.h/.cpp
    application/AccountServiceDXX.h/.cpp
    persistence/DatabaseDXX.h/.cpp
    persistence/AccountRepositoryDXX.h/.cpp
  core/
    UserDXX.h/.cpp
    AccountDXX.h/.cpp
  protocol/
    ProtocolDXX.h/.cpp
  database/schema.sql
  tests/
  demo/
  docs/
```

对每个自定义类应有单独 `.h` 和 `.cpp`；Qt Designer 的 `.ui` 文件可选。客户端与服务端可共享 protocol 和纯 C++ DTO；**客户端不得链接 SQLite 业务数据库或访问服务器本地数据库文件**。

CMake：顶层工程含两个可执行 target `IMServer` 和 `IMClient`，Qt 6 `Widgets` 与 `Network` 用于客户端；服务端优先只使用标准 C++/POSIX sockets/SQLite，以明确底层网络学习价值。采用 C++17。JSON 可使用仓库已有合法依赖；否则选择轻量公开库，说明版本和获取方式（优先 CMake 可复现依赖，避免运行时网络下载依赖）。确保 Qt Creator 可以打开根目录 CMakeLists.txt 构建并运行两个 target。

## 4. MVP 1 数据模型与持久化约束

最低表及字段（可以增加字段，不得删除语义）：

### users
- `user_id INTEGER PRIMARY KEY`（使用 64 位 C++ 整数）
- `login_name TEXT NOT NULL UNIQUE`（明确大小写规则：建议统一大小写不敏感，并在 SQL 施加相同约束）
- `password_hash TEXT NOT NULL`（加盐的专用密码哈希 KDF；禁止明文、简单 SHA256；优选 Argon2id 或平台上有可靠依赖的 PBKDF2-HMAC-SHA256，记录算法、盐和参数）
- `nickname TEXT NOT NULL`
- `birthday TEXT NULL`（ISO `YYYY-MM-DD`）
- `location TEXT NULL`
- `shared_social_id TEXT NULL UNIQUE`（QQ/微博共享展示号码，可按实际方案改为规范化号码分配表）
- `registered_at TEXT NOT NULL`（UTC ISO 8601）

### accounts
- `account_pk INTEGER PRIMARY KEY`
- `user_id INTEGER NOT NULL REFERENCES users(user_id)`
- `service_type TEXT NOT NULL CHECK(service_type IN ('QQ','WECHAT','WEIBO'))`
- `display_id TEXT NOT NULL`
- `status TEXT NOT NULL CHECK(status IN ('ACTIVE','CLOSED'))`
- `opened_at TEXT NOT NULL`
- `UNIQUE(user_id, service_type)`
- `UNIQUE(service_type, display_id)`

### id_counters（或等价事务安全方案）
- 两种编号分配器：`SOCIAL_SHARED` 与 `WECHAT`；展示编号示例 QQ/微博 `10001` 起，微信 `WX8001` 起。初始号码是演示值，可配置，但必须保证并发注册互不冲突且重复请求不多分配号码。

### registration_requests（注册幂等记录）
- `request_id TEXT PRIMARY KEY`
- `request_fingerprint TEXT NOT NULL`（请求字段规范化后计算的摘要；**密码不能以明文出现在日志或请求审计持久化中**）
- `user_id INTEGER NOT NULL REFERENCES users(user_id)`
- `created_at TEXT NOT NULL`
- 同一 `request_id` + 相同有效载荷重试，返回原成功结果；同 ID 不同载荷返回冲突错误。该记录与用户/账号在同一数据库事务提交。

### session management
- MVP 1 可以仅在服务端内存维护 `session_id -> user_id / active_services / connection_id`，断线失效、服务重启需重新登录。不要返回可以无限期复用的明文永久令牌。后续持久化会话若实现则需要过期与撤销机制。

必须开启 `PRAGMA foreign_keys=ON`，有可追踪 schema 版本；数据库只由服务器访问，数据库路径可以通过配置或命令行设置，默认路径应明确且可写。服务器写入由一个数据库工作线程串行处理；数据库连接在其所属线程创建和使用。**不能把密码、敏感登录凭证写入服务器日志。**

## 5. TCP 协议 v1（MVP 1 必须实现）

- 传输：仅本机回环 `127.0.0.1:8888`；不对外网绑定；此次是本机教学环境，不声称提供公网安全通信。
- 帧格式：前 4 字节无符号大端整数 `payload_length`，后跟 UTF-8 JSON 字节；最大 payload 64 KiB。处理半帧、合并帧、非法长度、非法 UTF-8 / JSON，不可假定一次 `recv` 就得到完整报文。
- 请求 JSON 统一格式：`{"kind":"request","type":"auth.register","request_id":"...","data":{...}}`。
- 响应 JSON 统一格式：`{"kind":"response","type":"auth.register","request_id":"...","success":true,"data":{...},"error":null}`。失败时 `success:false`，`error:{"code":"...","message":"..."}`，`data` 可为 `null`。
- `request_id` 用 UUID 或等价高熵唯一标识；注册幂等、重复发送和响应关联均以该值为依据。
- MVP1 的请求类型：`auth.register`、`auth.login`、`auth.logout`、`account.services.list`、`account.service.activate`、`system.ping`。无需新增未批准业务类型。
- 注册请求字段：`login_name`, `nickname`, `password`, `birthday` (nullable), `location` (nullable), `services`（列表，值仅 QQ/WECHAT/WEIBO）。客户端校验确认密码，不将确认密码作为服务端数据字段；服务端仍完整校验密码本身和其余字段。
- 注册成功响应：`user_id`, `registered_at`, `accounts:[{service_type,display_id}]`；服务器**仅在 SQLite commit 成功后**发送成功响应。
- 登录请求：`login_name`, `password`；成功响应：`user_id`, `nickname`, `services:[{service_type,display_id,active}]`, `session_id`（当前连接上下文内的临时标识）。对用户不存在和错误密码采用同一模糊错误码，避免泄露账号是否存在。
- 服务激活请求：`service_type`，需已登录且拥有该服务；服务端记录会话已激活状态并回应。
- 超时或丢失响应的注册请求不可立即认定失败；原 `request_id` 重试可安全取回原结果。
- 错误码至少：`VALIDATION_ERROR`, `LOGIN_NAME_TAKEN`, `AUTH_INVALID_CREDENTIALS`, `AUTH_REQUIRED`, `SERVICE_NOT_OPEN`, `REQUEST_ID_CONFLICT`, `PROTOCOL_ERROR`, `DATABASE_ERROR`, `SERVER_ERROR`。
- 日志记录网络事件和请求类型，不记录明文密码或完整 auth JSON。对异常请求不崩溃；明确断开连接和清理 session。

## 6. Qt Widgets UI v1

需要：
- 服务状态（连接中 / 已连接 / 断线）和重试连接按钮。
- 注册表单：登录名、昵称、密码、确认密码、生日、所在地、QQ/微信/微博复选框；至少选一个服务；详细字段错误提示，仅重置有问题的字段，不丢其他有效输入。
- 注册提交时禁用重复提交，并显示进行中；收到真实成功响应才跳转“注册成功”，展示 user_id 和各服务 display_id；可复制 ID。
- 登录表单：登录名与密码；失败只提示，不清除正确登录名；成功进入服务选择。
- 服务选择：展示当前开通的服务、ID；点击后请求服务器激活服务；成功显示最小主页面占位信息，不制作不工作但可点击的好友/群组菜单。
- 所有网络操作使用 `QTcpSocket` 的异步信号回调，不在 GUI 主线程调用阻塞式网络等待。不要从非 GUI 线程直接修改 QWidget。

## 7. 非功能要求与工程质量

- Mac + Qt Creator 可打开并使用 CMake 编译，编译警告尽量清零；禁止将仅 Windows 的 API 当作 Mac 代码。
- Qt GUI 不因服务器离线或慢速响应冻结。
- 显式边界：UI 不直接 SQL；Server 网络层不直接执行业务规则；Service 层不依赖 Qt Widgets；Domain 与 protocol 尽量纯 C++。
- 标准 C++ 类以 DXX 结尾；一个自定义类一个对应文件对（.h/.cpp）。保留合理注释解释线程所有权、事务边界、策略预留，不要冗长逐行注释。
- 服务端可响应 SIGINT 进行正常退出；socket 关闭和线程 join 不死锁。
- 登录凭据暂经 loopback 明文 TCP 传送仅允许本机教学演示，**禁止监听 0.0.0.0 或宣传为可公开部署**。使用真实公网前必须重新设计安全通信、密钥和会话机制。

## 8. 验收测试（必须报告 Pass/Fail/Not Run 与证据）

- T01 注册 Alice（QQ），返回唯一账号 ID；SQLite 存储成功。
- T02 注册 Bob（QQ、WECHAT、WEIBO），QQ/微博展示编号相等、微信独立。
- T03 注册 Carol（仅微信），成功分配微信编号，不虚构已开通 QQ。
- T04 正确密码登录并返回已开通服务，服务选择可激活。
- T05 错误密码不能登录，界面不清空正确用户信息。
- T06 重启 IMServer，已注册用户和服务信息仍存在。
- T07 模拟数据库不可写或写入异常，注册返回失败且无部分数据；不得显示成功。
- T08 同时启动至少三个 IMClient，登录不同用户，服务端正常处理、客户端无 UI 冻结。
- T09 同一个注册 `request_id` 和等价载荷重复发送，不能重复注册；同 `request_id` 不同载荷应拒绝。
- T10 服务器未启动时打开客户端，连接错误清晰可见并可重试。
- T11 TCP 报文拆成多个片段与连续合并发送，服务器正确解帧；非法帧安全拒绝。
- T12 SQL 外键生效；重复 login_name、重复服务账号被约束拒绝。
- T13 检查日志：不得出现明文密码。
- T14 服务器退出后重新启动，原数据库仍可用；正常启动不清空演示或用户数据。

至少为服务层/协议层提供自动化测试；GUI 手工测试要写明确操作与结果。如果 CI/GUI 运行受限制，标明未运行原因，禁止将未运行标为通过。

## 9. 必须交付

1. 可运行源码，顶层 CMake 工程，`IMServer` 与 `IMClient` 两个可执行 target。
2. `README.md`：Mac 上 Qt Creator 打开工程、安装依赖、构建、启动服务器、启动三个客户端、数据库路径、重置演示数据的方法。
3. `database/schema.sql` 及数据迁移或初始化方案；`demo/` 可重置演示数据，不得普通启动自动清空。
4. `tests/` 自动化测试 + 测试结果报告（每个 T01–T14 的状态）。
5. `docs/MVP1_IMPLEMENTATION_REPORT.md`：实际实现了什么、文件变化、架构调整、如何测试、已知缺陷、后续 MVP2 接口预留。
6. UML/ER 图本阶段仅需随代码保持同步的简化图或 Markdown Mermaid，不要求制作全项目终稿。

## 10. 本轮完成标准与停止条件

完成且确认可运行 MVP1 后停止本次任务，**不要自行开发好友、群组、聊天或多态模块**。交付构建/运行命令、关键文件位置、逐项测试状态和未解决问题。若因环境无法完成，交付已完成的最大可测试切片并标记未执行项目；不得声称完成未验证的功能。

---

## 给 Work 的一句话指令（可复制）

请按照本文件完整实施 IMPlatform MVP 1。先审计 GitHub 私有仓库 `Joyce06joy/oop-project-joyce`，如无权限明确报告而不要猜测；保持 macOS + Qt Creator + Qt 6 Widgets + C++17 + CMake，完成 Qt 注册/登录 → QTcpSocket → POSIX TCP Server → SQLite 事务与自动 ID 的最小闭环，落实 DXX 命名、异步非阻塞、幂等注册、异常处理及 T01–T14 验收，最后提交可构建源码、README、测试结果和实现报告。只做 MVP 1，不扩展其他模块。
