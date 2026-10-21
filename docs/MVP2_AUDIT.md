# MVP2 开始前的实际审计

基线 commit：`e22f0fc9c40ebb98f4e75168de66b0ca36b0217d`。本地工作区开始时干净；GitHub API 核实 main 为相同提交。GitHub 当前仓库元数据可访问；历史“私有库无法访问”是 MVP1 初次交付时的情况，不作为当前事实。开发分支：`feature/mvp2-friends`，不合并默认分支。

MVP1 实际目录：client/network、client/ui（Login/Register/ServiceSelect），protocol（Json/Protocol），server/network（POSIX TcpServer）、application（AuthService/RequestDispatcher/Password）、persistence（Database/Statement），database/schema.sql、tests（protocol/acceptance/client_ui）、demo/seed.py、文档。原根 mainwindowdxx 是保留的空模板目标，不是业务主窗口。

构建使用已安装的 Qt 6.12.0 for macOS、Clang 21、Qt 自带 CMake/Ninja，json-c 0.19/OpenSSL 3.6.5/SQLite SDK 3.51.0。修改前实际运行 build/mvp1 三组 CTest：3/3 通过，19.37 秒。服务端 --db/--port，仅绑定127.0.0.1；IMClient 为 macOS .app，支持--port。MVP2独立构建目录build/mvp2，保留旧构建用于基线对照。

MVP1 线协议为4字节大端+UTF8 JSON，64KiB，kind=request/response，六种操作；连接序号绑定内存会话，session_id不能跨连接当令牌。客户端异步QTcpSocket，以UUID关联响应，有15秒超时；注册未确认重试保留原ID。SQLite由唯一工作线程使用，网络线程poll收发；原注册幂等registration_requests与事务保留。

实际开发库：data/mvp1-demo.sqlite3（v1、3用户），data/manual.sqlite3（v1、0用户）。已用SQLite backup快照到data/backups/mvp2-preflight并quick_check；没有清空、迁移或停止用户正在使用的manual库。新服务器在真正打开v1库进行升级时会再做自动、不覆盖的迁移前备份。

差异及兼容策略：
- MVP1没有通用mutation幂等和server event；新增mutation_requests和kind=event，原六种请求与信封不失效。
- 原DB只有user_version=1初始化；新增002迁移，保留原schema.sql用于v1迁移测试，不重建原users/accounts/counters/registration_requests。新空库先初始化v1再应用v2。
- 旧ServiceSelect窗口继续承担登录后主区域与原激活控件，增量嵌入WorkspaceDXX与四个页面；不盲目改写注册、登录或根模板MainWindowDXX。
- 原自动测试只有一处固定schema版本断言需要从1更新为2，其他旧断言保留；另以原v1 SQL构造数据库验证真正无损升级，不能仅以新建库冒充迁移通过。
- 原已知限制（小规模串行KDF、loopback明文、注册请求ID仅内存）保留并说明；新增页面对mutation的不确定结果也保留原ID/载荷重试。
