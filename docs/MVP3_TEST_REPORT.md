# MVP3 测试报告

日期：2026-10-22（Asia/Shanghai）。测试对象：`feature/mvp3-groups`，基线MVP2 `53955f3`；全部业务测试用独立临时数据库/端口，不写默认用户库。

## 环境、执行及证据

Apple Silicon Mac，AppleClang21、Qt6.12.0 Widgets/Network/Test、C++17、CMake/Ninja、json-c0.19、OpenSSL3.6.5、SDK SQLite3.51.0，Python3.13。命令从仓库根运行：

```sh
export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build/mvp3 -G Ninja \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mvp3 --parallel 6
ctest --test-dir build/mvp3 --output-on-failure
python3 tests/mvp3_acceptance.py --server build/mvp3/IMServer
QT_QPA_PLATFORM=cocoa ./build/mvp3/Mvp3UiTestsDXX ./build/mvp3/IMServer docs/test-evidence/mvp3-native
python3 demo/seed_mvp3.py
```

最终CTest **9/9组Pass，0 Fail，85.08秒**（含MVP1/MVP2原5组与MVP3新增4组）；T17/T20额外各重复5次全部Pass。构建通过。Qt原生cocoa运行通过，保存三个窗口及策略切换截图；offscreen运行通过。SQLite故障采用测试专用触发器RAISE(ABORT)，不把生产库设为只读。限制网络的执行沙箱中经过允许本机Socket后运行，不将端口权限错误算作通过。

原始证据：

- [配置](test-evidence/mvp3-configure.log)、[构建](test-evidence/mvp3-build.log)、[最终CTest](test-evidence/mvp3-ctest-final.log)。
- [21项TCP首轮](test-evidence/mvp3-acceptance.log)、[解散通知新增回归修复前](test-evidence/mvp3-disband-red.log)、[修复后](test-evidence/mvp3-disband-green.log)。最终CTest再次运行包含该新增断言的完整TCP套件。
- [原生Qt](test-evidence/mvp3-ui-cocoa.log)、[offscreen Qt](test-evidence/mvp3-ui-offscreen.log)、[纯域ASan/UBSan](test-evidence/mvp3-domain-sanitizers.log)、[并发场景重复5次](test-evidence/mvp3-concurrency-repeat.log)。
- [演示种子真实API结果](test-evidence/mvp3-demo.log)、[再次运行拒绝覆盖及库摘要未变](test-evidence/mvp3-demo-preservation.log)。
- [Alice QQ](test-evidence/mvp3-native/alice-qq.png)、[Bob QQ](test-evidence/mvp3-native/bob-qq.png)、[Carol QQ](test-evidence/mvp3-native/carol-qq.png)、[同一QQ群切微信规则](test-evidence/mvp3-native/alice-wechat-policy.png)。截图来自原生Qt窗口grab，三个窗口在同一测试进程内分别持有独立TcpClient连接；不是三个独立IMClient进程人工操作。

## MVP1 / MVP2 回归

修改前 `build/mvp2` 原5组CTest实际5/5通过，56.55秒。群组实现后 `build/mvp3` 原5组全部再次通过，原协议与注册登录/编号、服务激活、好友/申请/审核/删除/备注、补开服务、绑定/共同好友/推荐、迁移备份、真实推送及Qt交互保留。旧两个Python脚本仅把当前schema版本期望2改3，未删除旧断言。

| 原测试组 | 结果 | 证据 |
|---|---|---|
| protocol | Pass | 最终CTest；严格JSON/UTF8、长度分帧/合帧 |
| acceptance | Pass | 最终CTest；MVP1真实TCP/数据库 |
| client_ui | Pass | 最终CTest；旧Qt注册/登录/断线/超时/重试 |
| mvp2_acceptance | Pass | 最终CTest；17项MVP2服务端验收 |
| mvp2_ui | Pass | 最终CTest；原好友/账号/推荐真实Qt |

MVP2历史T18中Qt Creator菜单及完整独立进程人工流程仍未补作，不把原自动回归误称人工验收通过。

## M3-T01–T22 逐项结果

| 项目 | 结果 | 实际断言与证据 |
|---|---|---|
| M3-T01 QQ建群/群主/编号持久唯一 | Pass | TCP test01；SQL两群独立编号与唯一OWNER |
| M3-T02 微信建群/微博拒绝 | Pass | test02；原属微信切QQ后申请/子群仍限微信账号 |
| M3-T03 申请审批不重复 | Pass | test03；重复申请拒绝、二次批准拒绝、集合只一份 |
| M3-T04 管理员审核/普通成员拒绝 | Pass | test04；伪造actor_role不生效；Qt管理员审核 |
| M3-T05 普通微信成员邀请/本人接受拒绝 | Pass | test05；邀请者不能代确认；Qt接受/拒绝链路 |
| M3-T06 微信直接申请/普通成员踢人拒绝 | Pass | test06，当前策略服务端鉴权 |
| M3-T07 设撤管理员/非法对象 | Pass | test07；Qt设撤、非主拒绝、不能任命群主 |
| M3-T08 管理员只踢普通成员 | Pass | test08；群主/其他管理员保护 |
| M3-T09 微信仅群主公告/踢人/解散 | Pass | test09；普通成员邀请权不产生管理权 |
| M3-T10 群主禁止直接退出/解散后拒绝 | Pass | test10；子群同步软失效；新增外部申请/邀请参与者真实Socket通知且无关人不获event |
| M3-T11 子群独立集合/父群子集 | Pass | test11；非法目标拒绝；DDL复合FK；Qt创建及成员列表 |
| M3-T12 退出/被踢清理子群 | Pass | test12；SQL级联关系与查询断言 |
| M3-T13 QQ→微信策略保留/停权 | Pass | test13；ID/数量/stored_role/子群SQL保留，旧权限拒绝；Qt即时禁用 |
| M3-T14 切QQ恢复权限/子群 | Pass | test14；恢复原ADMIN、子群与成员，不重建；Qt恢复 |
| M3-T15 重启恢复策略/数据/行为 | Pass | test15；Server重启+新连接认证查询，虚策略行为及原UUID持久重放 |
| M3-T16 伪造角色/非主切换/跨服务拒绝 | Pass | test16；未认证/未激活/未开通与无关成员隐私 |
| M3-T17 切换与踢人/审批并发 | Pass | test17；切换前合法完成允许，确认后旧权限拒绝，过期version冲突 |
| M3-T18 DB失败回滚/不误报 | Pass | test18故障注入建群/接受/切换，逐表含dedup一致且无event；Qt另注入真实建群SQL失败，未显示成功、保留输入、同UUID重试恰一群 |
| M3-T19 多会话推送/离线恢复 | Pass | test19；审批/邀请/成员/规则event、多会话、无关/退群用户隔离，离线重连查正确策略 |
| M3-T20 幂等/双审批/并发 | Pass | test20；覆盖所有群变更同UUID完整response相同、内容冲突拒绝、无重复event、同时审批只一成功 |
| M3-T21 真v2无损迁移/旧功能 | Pass | test21构造schema1+002真实v2库，9旧表逐行比较、快照版本/完整性与重启不覆盖、登录/好友/绑定推荐回归 |
| M3-T22 Qt Creator/CMake及三独立客户端人工展示 | **Not Run（部分已验证）** | Mac CMake构建Pass；原生cocoa三个窗口独立Socket自动流程Pass及截图；Qt Creator菜单实际构建和三个独立IMClient进程完整人工场景未执行，因此整项不标Pass |

T01–T21服务端项全部通过；T18的Qt故障显示额外通过。T22缺失项如上，未以自动化替代人工声明。没有未修复的已知Fail；曾观察到的失败及修复均保留证据。

## 额外验证与失败记录

- GroupPolicyTestsDXX：47项，含同一Group对象地址不变、抽象引用虚调用随QQ→微信→QQ变化、成员/角色/子群保留、无主成员和解散边界。普通构建及ASan/UBSan通过；证明的是实际动态派发，非文案切换。
- group_database.py：直接SQL测试无主群提交失败、OWNER唯一与保护、跨服务成员拒绝、子群复合父成员FK、级联退出、停用子群插入失败、foreign_key_check空。
- 新TCP测试先对旧MVP2二进制实际RED：group.create未知PROTOCOL_ERROR；真实v2迁移期望3得到2。域测试stub阶段RED：QQ outsider applies only；随后权限实现GREEN。
- 解散外部参与者新增断言初次FAIL TimeoutError，补接收者后GREEN，未扩大到无关人。
- Qt首轮FAIL groupSearch：测试在异步创建响应前立即点击禁用控件；改为等待其恢复可用后点击，业务断言保留。后续offscreen与cocoa通过。
- 最终全套首跑T17发生测试断言竞态：目标已在切换前被合法踢出，后续得到GROUP_INVALID_TARGET而测试只接受权限拒绝。新增一个不参与竞态且始终在群的普通成员作为切换后目标，严格断言其仍在群且管理员踢人被拒；未把GROUP_INVALID_TARGET加入宽松通过条件。首跑日志保留为mvp3-ctest-race-assertion.log，修正后重跑全套。
- 原生日志有Mac输入法mach port诊断，offscreen有propagateSizeHints诊断，进程退出0且业务断言通过，不将这些平台提示当业务失败。

## T22 人工复现步骤（尚未执行部分）

1. Qt Creator打开根CMakeLists，选择Qt6 Desktop Kit，构建目录build/mvp3；配置json-c/OpenSSL（README），构建IMServer/IMClient。
2. 生成专用demo（存在则先停止服务器、明确决定是否--reset），IMServer参数`--db ./data/mvp3-demo.sqlite3`。
3. 用三个独立终端各启动一次IMClient.app内可执行文件，分别Alice/Bob/Carol，激活QQ并进入群1001；Alice群主、Bob管理员、Carol成员。
4. Alice两次确认切微信规则，Bob公告/审核/踢人特权停用、子群隐藏，成员数量/ID不变；切回QQ恢复。David申请取消，不自动加入。
5. 切微信空间群1004，Carol本人确认Bob邀请，Bob仍只能邀人而无管理特权；重启Server并重新连接后核对保存状态。
6. 保留Qt Creator构建输出、三个独立进程PID与完整操作截图/录像，再将本项改为Pass。未完成前保留Not Run。
