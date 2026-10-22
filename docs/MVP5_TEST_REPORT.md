# MVP5测试报告

环境2026-10-22，macOS26.4 arm64，Qt6.12.0 Widgets/Network/Test，AppleClang21/C++17，CMake/Ninja，SDK SQLite3.51.0。基线610bc77，feature/mvp5-ui。所有功能测试使用真实临时本机Server/SQLite，独立端口；UI25只对已有8888Server新建空连接，无注册/登录/写库。

## 命令和证据

```sh
cmake -S . -B build/mvp5 -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos"
cmake --build build/mvp5 --parallel 6
ctest --test-dir build/mvp5 --output-on-failure
QT_QPA_PLATFORM=cocoa ./build/mvp5/Mvp5UiTestsDXX ./build/mvp5/IMServer ./build/mvp5/IMClient.app/Contents/MacOS/IMClient docs/test-evidence/mvp5-native
QT_QPA_PLATFORM=cocoa ./build/mvp5/Mvp4UiTestsDXX ./build/mvp5/IMServer ./deliverables/MVP5-macOS-arm64-final/IMClient.app/Contents/MacOS/IMClient docs/test-evidence/mvp5-packaged
```

基线13/13实际通过152.08秒，见mvp5-baseline-tests.log。新增Mvp5UiTests和全部旧13组总14组重新运行；最终 **14/14 PASS，0 FAIL，154.30秒**，见[mvp5-ctest-final.log](test-evidence/mvp5-ctest-final.log)和[逐测试完整输出](test-evidence/mvp5-ctest-details.log)。原生新增UI和打包后原生三进程另行执行且通过，见[原生UI](test-evidence/mvp5-ui-cocoa-final.log)、[部署包三进程](test-evidence/mvp5-packaged-ui-final.log)、[Finder](test-evidence/mvp5-finder-final.log)、[部署检查](test-evidence/mvp5-package-final.log)。日志位于test-evidence/mvp5-*，只脱敏机器路径/测试消息正文；原始输出仍在本次执行临时目录。Pass/Fail、消息ID和计数保持，不以截图代替Socket/SQL证明。

| 用例 | 结果 | 实际步骤/期望与证据 |
|---|---|---|
| UI01 无Server启动 | PASS | 新UI真实拒绝连接、状态可见，5ms心跳持续；非沙箱拒绝端口假通过 |
| UI02 启动后重试 | PASS | 启临时Server，点击重试，状态已连接 |
| UI03 错误密码 | PASS | 真实Server拒绝，账号保留、密码焦点；显隐切换同时验证 |
| UI04 空昵称/非法日期 | PASS | 注册空昵称定位，其他字段保留；QDateEdit拒绝未来/1900前日期输入 |
| UI05 注册真实ID | PASS | 真实注册COMMIT结果、重复提交禁用；原client_ui另覆盖重启登录/保存 |
| UI06 三服务切换 | PASS | 单QMainWindow、六页索引、当前服务/号码独立；Server日志无新连接/auth.login；QQ好友不出现在微信 |
| UI07 未开通引导 | PASS | Bob仅QQ，微信显示开通指引且好友页不可用，账号页仍可用 |
| UI08 绑定/解绑 | PASS | 重跑mvp2_ui的真实绑定/解绑/失败恢复及mvp2_acceptance |
| UI09 好友搜索空/有结果 | PASS | 新UI真实QQ好友/微信空集合/无匹配搜索；旧UI闭环复跑 |
| UI10 在线好友申请 | PASS | mvp2_ui两个真实Socket会话Qt推送；原native3进程验证真实隔离 |
| UI11 审核接受去重 | PASS | 原mvp2_ui/acceptance复跑，双方更新且幂等 |
| UI12 备注/删除与保存 | PASS | 原真实Qt备注/删除确认；服务端重启/持久化回归配合验证 |
| UI13 共同好友/跨服务理由 | PASS | mvp2_ui/acceptance实际同平台/跨服务查询、显式绑定；空态仍共0项 |
| UI14 QQ申请/管理员 | PASS | mvp3_ui真实申请审核/设撤，策略权限由Server决定 |
| UI15 微信邀请/群主管理 | PASS | mvp3_ui普通成员邀请/目标本人拒绝；微信管理员操作隐藏，Server越权拒绝原API测试 |
| UI16 子群有效成员 | PASS | mvp3_ui独立子群集合与增删、MVP4子群仅两成员接收，Carol沉默 |
| UI17 QQ→微信规则 | PASS | mvp3_ui实际切换/成员不丢/管理员停权/子群隐藏；API/SQL回归 |
| UI18 切回QQ | PASS | mvp3_ui恢复原管理员及子群、MVP4旧历史保留 |
| UI19 Alice→Bob私聊 | PASS | 部署包三独立IMClient PID，Bob真实event一次，Chinese/emoji气泡 |
| UI20 三人群聊 | PASS | 部署包原生三进程各一次接收，真实消息ID/Server SQL |
| UI21 离线重登 | PASS | 服务端Bob离线同步测试+Qt实际离线生产者/重新认证/消息恰一次合并，分段证据不冒称人工完整演示 |
| UI22 未知状态/原UUID | PASS | 原15秒超时测试及Qt透明TCP代理：COMMIT后丢响应，unknown→原UUID→仅一stored；中文映射另测试 |
| UI23 缩放/长消息 | PASS | 新UI精确900×620、1100×720，滚动底部操作可达；实际delegate窄视口长Unicode更高、纯文本绘制 |
| UI24 三独立并发 | PASS | 部署包每场景3×IMClient，不同PID/Socket/账号，不共享聊天模型 |
| UI25 Finder打开app | PASS（自动化） | Finder AppleScript exit0、启动新包进程1个、lsof见到8888ESTABLISHED；未触及已有用户Server与用户库；不是人工双击录像 |
| UI26 Server退出恢复 | PASS | 新UI真实停止/重启，状态横幅、心跳；原MVP4自动认证/同步复跑 |
| UI27 反复切页退出 | PASS | 新UI12轮切页后搜索仅一个真实响应，单主窗口；旧测试优雅关闭 |
| UI28 换账号无残留 | PASS | 新UI先Alice搜索/聊天再Bob登录，旧搜索模式/好友身份/草稿/消息清空；真实账户权限 |

以上PASS包含已明确标注的自动化组合证据，不代表全部人工逐条点验。Qt Creator菜单从零配置完整人工验收、人工录像、另一Mac/Intel/旧macOS、互联网下载Gatekeeper/公证场景NOT RUN，不伪装已测试。当前包只有本机ad-hoc签名。

## 失败记录与处理

- 初次新增测试在沙箱无本机端口权限时reserveTCP失败，属于环境限制；获准本机网络后运行，不算业务通过。
- 两个心跳断言曾把80/100ms内固定计数阈值设得高于QTest调度保证；改为有上限等待实际连续心跳，不降低非阻塞要求。
- 注册辅助窗口焦点测试需等待窗口激活；补实际激活等待。TcpClient的QTcpSocket是值成员非QObject子节点，改用Server无新连接/无auth.login的真实日志检查，不以findChild为空判产品错误。
- UI28真实暴露好友搜索模式跨用户残留，新增resetView清分页/模式/结果，先失败后通过；未改Server权限或SQL。
- 推荐空态首次改文案去掉“共0”，原mvp2_ui回归失败，恢复真实计数前缀并保留解释，旧断言未弱化。
- 打包检查两次把universal架构标题当依赖产生误报；修复otool解析，真实18Mach-O依赖/插件/资源/签名核验通过。
- 打包后cocoa旧历史去重测试曾在第65条event尚在传输时过早截图计数，后续正常到达被误作重复；保留去重断言，并先明确等待最后page-64真实事件。产品消息算法未改，部署包完整复测通过。

受保护server/database/protocol45文件逐字节未变；已有MVP1–4专用库版本/人数/quick_check保留，MVP5独立demo。详细包限制见MACOS_PACKAGING.md，操作复现见DEMO_MVP5.md。
