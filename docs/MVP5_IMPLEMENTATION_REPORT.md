# MVP5实施报告

日期2026-10-22。基线610bc7779165c942b58fc12e34654e028a142fc3，分支feature/mvp5-ui；仅增量Qt界面/资源、构建、测试和Mac打包。本轮server/database/protocol共45文件SHA256与基线一致，没有迁移、消息ID重排或协议字段改变。已有用户库不清理，演示只新建mvp5-demo。

## 实际交付

复用LoginWindowDXX这一QMainWindow，单已登录工作台+注册辅助窗口；压缩顶部用户/连接/服务激活区，保留左侧三服务和原0–5业务页。未开通/未激活/已激活分别反馈，微博隐藏聊天/群组，QQ/微信实际业务流程不变。主窗口1100×720、最小900×620，管理页可滚动抵达底部操作。

ThemeDXX集中应用系统字体和内嵌QSS，原创SVG/PNG/ICNS图标，版本0.5.0。登录/注册新增密码显隐、Tab顺序及错误焦点，正确字段保留；通用RequestPanel有idle/loading/success/error/unknown/offline状态，重复提交/原UUID机制沿用。空好友/申请/推荐/群页提供说明，微信规则隐藏管理员操作。

MessageBubbleDelegateDXX纯QTextLayout绘制左右气泡、时间/账号/中文消息状态及真实送达x/y，支持Unicode无空格换行、选择/键盘焦点/可访问文本；模型原UUID/消息ID角色保留。ChatPage只增量展示层，发送、ACK排队、幂等、同步及认证重连复用已有实现。没有已读图标或本地SQL。

发现并修复跨账号好友搜索模式残留：新用户登录时FriendPage::resetView清搜索模式、分页/结果；旧正确字段按原窗口逻辑清理。新增UI28真实复现后通过。推荐空状态保留“共0项”计数供用户和原测试判断，不把空提示伪装成结果项。

## 修改文件

- client/ui/ThemeDXX.h/.cpp与client/resources/theme.qss、ui.qrc：统一主题与显式静态资源初始化。
- client/resources/app-icon.svg、app-icon.png、IMPlatform.icns，scripts/generate_app_icon.py：原创图标、无第三方Python依赖可复现。
- LoginWindowDXX/RegisterWindowDXX：布局、密码显隐、焦点/Tab、窗口标题与图标。
- ServiceSelectWindowDXX、WorkspaceDXX：紧凑激活条、当前服务/账号提示、三服务导航、账号更换视图清理。
- RequestPanelDXX：可滚动body与状态属性；Friend/FriendRequest/Recommendation/Account/Group/Subgroup页面：任务行布局、空提示、角色/权限可视、文字可读。
- MessageModelDXX、ChatPageDXX：附加展示角色、中文状态、空态/权限说明、布局；新增MessageBubbleDelegateDXX.h/.cpp。新增自定义C++类均DXX且独立h/cpp。
- client/main.cpp只接入主题初始化；不改网络或聊天业务协议。
- CMakeLists：资源/两个UI类、图标/version0.5.0、Mvp5UiTestsDXX；保留原IMServer/IMClient/模板及13测试目标。
- tests/mvp5_ui_main.cpp：真实Server/Qt新增验收、900×620与1100×720、账号隔离/状态与气泡/反复切页。
- tests/mvp4_ui_main.cpp：只强化异步测试完成边界，原历史去重断言保留；先确认65条中的最后真实事件已到，再比较去重数量，避免将尚在传输的最后一条误认为重复。此项在cocoa测试先失败后修正，产品消息算法未变。
- scripts/package_macos.py、sanitize_evidence.py：独立stage部署/依赖/签名核验，机器路径/测试消息正文脱敏。
- demo/seed_mvp5.py：新专用三用户数据，显式--reset且已有库拒绝覆盖。
- docs/MVP5_BASELINE_AUDIT.md、MVP5_UI_SPEC.md、MVP5_TEST_REPORT.md、DEMO_MVP5.md、MACOS_PACKAGING.md、任务书/计划与证据，README更新。

## 构建、验证、打包

基线构建/13组实际通过152.08秒。本轮14/14组全部通过，0失败，154.30秒；原生结果及28项对应证据见MVP5_TEST_REPORT。三独立客户端使用真正部署包内IMClient，以真实TCP验证QQ/微信单聊、三人群、两人子群及故障恢复，未以同一窗口模拟通信。

macdeployqt只作用新deliverables目录的复制包，检查18个Mach-O真实非系统依赖与rpath目标、Qt Cocoa/图像插件、json-c、QRC/ICNS。ad-hoc签名后严格验证通过；arm64、版本0.5.0、最低系统26.4。Finder AppleScript实际启动新的包进程，并检查到本机8888ESTABLISHED连接，未登录/改用户数据库；原用户Server保持运行。完整业务用随机端口的临时Server隔离验证。

初次部署检查把universal Mach-O第二架构标题误当依赖，导致绝对路径误报；修复解析与安装ID规范化后通过，不将此误报当Qt库缺陷。部署结果仅代表本机，不声称公证、商店发布、Intel或旧系统/干净Mac可无警告运行。Server终端程序仍依赖本机OpenSSL/json-c等开发依赖。

## 已知边界及MVP6移交

主路径已用真实测试验证；Qt Creator完整菜单人工验收、另一台干净Mac和Gatekeeper互联网下载场景未执行，报告分别标注。Finder验证为真实自动化Finder打开，不是人工双击录像。新证据日志脱敏机器路径和测试消息正文，不修改Pass/Fail/ID/计数；原MVP1–4历史文件保留不重写。

游标、未确认消息UUID/草稿仍为本进程内，不扩展本地持久凭据。注册仍是独立辅助窗口，只有一个已登录主窗口。没有QML/Web重写、媒体、云部署或已读。MVP6应依据最终Theme/Delegate/RequestPanel与原网络/服务/消息模型更新实际类图/架构文档；本轮不提前撰写答辩总报告。

实施按P0–P6增量完成；Git交付合并为一次经最终完整回归的提交，未为中间验证状态补造历史提交。这与任务书建议的阶段提交粒度不同。提交保存在feature/mvp5-ui，不合并main或强推。源码/资源/文档/测试证据纳入Git，.app/数据库/构建目录只作本地交付；实际SHA及远端核验以最终交付链接和Git记录为准。
