# MVP4 基线审计

日期2026-10-22。真实起点3bc6b017cf3bfab8c6653815a925649d26a8b845，feature/mvp3-groups，原工作区干净。新建feature/mvp4-chat，不改main或前轮分支历史。

MVP3不是假定存在：已读取GroupServiceDXX/GroupRepositoryDXX、纯C++ GroupDXX+虚策略、003 SQL、实际Qt群组页面和上轮测试报告；上轮9/9通过的原始日志存在，本轮再次执行全部原9组作为基线，实际9/9通过、85.34秒，证据test-evidence/mvp4-baseline.log。MVP3 T22完整Qt Creator菜单/独立进程人工场景历史未完成，不能据此声称人工全通过。

现有复用：SocialRepositoryDXX::owned/target/friends服务及好友判断；GroupRepository重建原服务成员/子群；GroupPolicy为管理策略，不能用canInvite等管理权限冒充发言权限。MutationRepositoryDXX事务与UUID成功结果；RequestDispatcherDXX持有连接会话及active services并在DB工作线程发布通知。网络线程poll持有所有FD及读写缓冲，DBworker通过互斥队列收任务/交结果，从不直接send。当前64连接，每连接8个inflight、输出8帧上限，MVP4需核查event fanout队列边界。

协议：4字节大端长度、UTF8 JSON、64KiB；request/response带request_id，event带event_id；认证绑定TCP连接，不能凭payload sender/role/session_id冒用。Qt主线程使用非阻塞QTcpSocket+异步信号，已有15秒超时和原UUID恢复模式。当前TcpClient只有手动重连；本轮增量加入有上限退避重连/重新认证及同步，不在UI线程等待网络。

实际schema版本3，无聊天表。DatabaseDXX已有v1/v2自动备份、v1→2→3增量迁移。聊天004应增加会话、canonical私聊对、消息、回执及成员期次；入退群以message ID边界而非秒时间判断，避免同秒退群/重入泄漏。成员期次触发器与旧群业务事务原子运行，不逐处重写MVP3。

已有本机库：mvp1-demo=v1(3用户)、manual=v1(0用户)、mvp2-demo=v2(5用户)、mvp3-demo=v3(4用户)。开工已只读SQLite backup到data/backups/mvp4-preflight，每份quick_check=ok；原库未升级/清空。测试用临时库；新demo仅mvp4-demo。

工具仍为AppleClang21、Qt6.12.0、Qt自带CMake/Ninja，json-c0.19、OpenSSL3.6.5、SDK SQLite3.51.0。CMake独立build/mvp4；保留build/mvp3用于旧服务器RED对照。接口/文件任务与测试标准见plans/MVP4_CONTRACT.md及MVP4_WORK_SPEC.md。后续实际结果见MVP4_TEST_REPORT.md。
