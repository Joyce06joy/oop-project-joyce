# MVP5 baseline audit

日期：2026-10-22，基线610bc7779165c942b58fc12e34654e028a142fc3，原feature/mvp4-chat且工作区干净。建立feature/mvp5-ui，不覆盖未提交工作或默认分支。本轮不修改server/database/protocol；开工保存45个受保护文件的SHA256，结束比较，并检查Git diff。

环境：macOS26.4(build25E246)，arm64；AppleClang21.0.0，Qt6.12.0，Qt自带CMake/Ninja，SDK SQLite3.51.0、json-c0.19、OpenSSL3.6.5。Qt安装来源为现有官方目录式安装，本轮不安装/升级依赖。实际基线构建通过，13/13组CTest通过，152.08秒，原始日志经机器路径脱敏后保存test-evidence/mvp5-baseline-*。

目标：IMServer、IMClient.app、保留的oop-project-Joyce模板、协议/纯域/Qt/集成测试。根CMake可Qt Creator打开。实际运行是IMServer --db独立路径 --port端口，另起多个IMClient --port同端口，不需要单实例锁。默认回环8888；Qt/client不访问SQLite。

| 模块 | 实际状态及证据 |
|---|---|
| MVP1 | 已实现注册/统一登录/自动ID/服务激活/数据库，原acceptance+client_ui复跑通过 |
| MVP2 | 已实现补开/QQ微信绑定/好友搜索申请审核删除备注/共同好友与跨服务推荐，mvp2_acceptance+mvp2_ui复跑通过 |
| MVP3 | 已实现群申请/邀请/角色/子群/真实多态切换，group_policy/group_database/mvp3_acceptance/mvp3_ui通过；历史Creator人工T22仍未运行 |
| MVP4 | 已实现真实TCP单聊/群聊/QQ子群/历史/成员epoch/ACK/原UUID恢复，mvp4_database/acceptance/ui/receipts通过；历史Creator人工T21仍未运行 |
| 原生三进程 | MVP4已有IMClient --chat-smoke真实LoginWindow+ChatPage和QProcess三独立PID证据；本轮必须重新运行，不用历史截图代替 |
| 打包 | 构建已生成.app，但未部署：otool显示开发环境Qt @rpath及外部json-c绝对依赖，不能当独立可分发包；本机macdeployqt可用 |

已读README及MVP1/2/3/4实施与测试报告、真实源代码/CMake/schema迁移链。实际主窗口是LoginWindowDXX(QMainWindow)，内含ServiceSelectWindowDXX+WorkspaceDXX，根MainWindowDXX只是保留模板，不新造第二个主窗口。注册为有父窗口的独立辅助窗口。原业务导航0好友/1申请/2推荐/3账号/4群组/5消息，QQ/微信开放后两项、微博隐藏；服务开通、连接认证与服务激活分别建模。

真实复用接口：RequestPanelDXX的read/mutate/loading/unknown同UUID重试，TcpClientDXX的响应request_id关联与event_id去重、断线退避/重新认证/服务恢复；Workspace按服务切换。ChatPageDXX有消息ID合并、ACK单队列、分页/草稿/未读、原UUID，MessageModelDXX目前纯文本行。Server poll网络线程和单DBworker分离，SQLite只Server；迁移已到v4。所有auth/account/friend/group/chat协议及错误码沿用PROTOCOL_V1至V4，本轮不改报文。

缺口：原登录/工作台占用纵向空间大，三个激活按钮竖排；管理布局和状态无集中theme；消息是纯文本行而非气泡，缺统一空状态/可视权限说明；无密码显隐、图标为空、版本仍0.1；需900×620/1100×720可用性与打包/Finder实测。原数据和业务不是待重写模块。

实施切片：集中QSS资源+紧凑主窗口/激活条与原侧栏；密码显隐/字段焦点/明确状态；管理页空状态和服务引导；纯Qt消息delegate与中文准确状态；新增UI测试；macdeployqt只操作deliverables中的复制包、检查全部Mach-O依赖/资源/插件、独立三进程及Finder启动；更新28项报告与演示。设计和文件所有权见plans/MVP5_CONTRACT.md。

主要风险：旧测试依赖objectName/页索引/激活入口，全部保留并重跑；气泡sizeHint与长Unicode/缩放需要真截图；UI不能改变stored/delivered/unknown语义或用本地状态授权；静态库QRC需显式初始化避免资源被裁；部署必须包含json-c而非只复制Qt；签名/公证与其他Mac兼容性不得推断。新日志只脱敏机器路径、不修改Pass/Fail事实，不记录真实用户凭据/私信。旧历史文档不重写。
