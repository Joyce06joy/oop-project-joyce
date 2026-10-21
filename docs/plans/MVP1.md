# MVP 1 实施计划

用户提供的 IMPlatform_MVP1_Work_Implementation_Spec.md 为本轮规格，优先于旧 qt-chat-design（其 Qt 服务端、1 MiB 帧、聊天等方案本轮不适用）。本轮按用户要求直接实施，不另行扩展范围。

1. 审计：确认已有模板可构建、Qt/编译器/依赖、GitHub 访问、原课程要求。保留原 main/window/UI，根 CMake 增量加入两个新 target。
2. 先写自动验收：真实 TCP 请求、SQLite 验证、重启、幂等、碎片与非法帧、写失败、日志检查；先确认未实现时失败。
3. 共享协议：纯 C++ JsonDXX（json-c 0.19 RAII），ProtocolDXX（64 KiB 帧/严格 UTF-8 JSON）。无运行时依赖下载。
4. 数据与应用：DatabaseDXX/StatementDXX 独占 SQLite、版本1 schema、事务、PBKDF2-HMAC-SHA256；AuthServiceDXX 校验与注册/登录；RequestDispatcherDXX 管理每连接临时会话。
5. 网络：TcpServerDXX 使用 POSIX 非阻塞 poll；单一工作线程创建及使用数据库、串行业务；网络线程处理分帧/部分发送/断线。使用连接序号防止 fd 复用混淆。
6. Qt：TcpClientDXX 异步请求关联与超时；LoginWindowDXX、RegisterWindowDXX、ServiceSelectWindowDXX 各独立文件；提交禁用、保留输入、注册重试复用请求 ID；无好友/群/聊天。
7. 实际构建，自动服务/协议/Qt UI 验收，运行三个独立客户端。区分自动 UI 测试与人工视觉检查。
8. README、T01–T14 报告、架构/ER、演示生成脚本，最终检查范围、命名、警告、diff。仅本地交付，不擅自推送。

验证使用独立临时数据库，不接触旧课程项目数据；无 schema 的非空数据库拒绝初始化。演示重置必须指定新路径或显式 --reset。
