# MVP4 测试报告

日期：2026-10-22（Asia/Shanghai）。分支feature/mvp4-chat，起点3bc6b017。环境：Apple Silicon Mac、AppleClang21、Qt6.12.0 Widgets/Network/Test、C++17、Qt自带CMake/Ninja、json-c0.19、OpenSSL3.6.5、SDK SQLite3.51.0、Python3。测试只用独立临时数据库及本机临时端口；demo只用mvp4-demo。

## 执行方法

```sh
export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build/mvp4 -G Ninja \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mvp4 --parallel 6
ctest --test-dir build/mvp4 --output-on-failure
python3 tests/mvp4_acceptance.py --server build/mvp4/IMServer
QT_QPA_PLATFORM=cocoa ./build/mvp4/Mvp4UiTestsDXX \
  ./build/mvp4/IMServer ./build/mvp4/IMClient.app/Contents/MacOS/IMClient \
  docs/test-evidence/mvp4-native
python3 demo/seed_mvp4.py
```

本地Socket测试在允许回环网络的执行环境实际运行；未将沙箱端口权限错误当通过。GUI默认CTest offscreen，另实际运行cocoa并保存真实Qt截图。native测试QProcess启动真正IMClient可执行文件，不是同一进程内共享聊天模型。

## 证据索引

- [MVP1–3修改前基线9/9](test-evidence/mvp4-baseline.log)，85.34秒。
- [配置](test-evidence/mvp4-configure.log)、[构建](test-evidence/mvp4-build.log)。
- [TCP完整20项](test-evidence/mvp4-tcp-final.log)、[长正文/分帧/慢端专项](test-evidence/mvp4-tcp-targeted.log)、[活跃Socket中SIGTERM专项](test-evidence/mvp4-tcp-sigterm.log)。
- [数据库迁移/epoch专项](test-evidence/mvp4-database.log)。
- [原生Qt独立进程](test-evidence/mvp4-ui-cocoa.log)、[offscreen Qt](test-evidence/mvp4-ui-offscreen.log)。
- [演示种子](test-evidence/mvp4-demo.log)、[旧库版本/人数/完整性及拒绝覆盖](test-evidence/mvp4-data-preservation.log)。
- [旧服务器RED](test-evidence/mvp4-old-server-red.log)、[首轮TCP夹具错误](test-evidence/mvp4-tcp-first.log)、[首轮旧UI回归失败](test-evidence/mvp4-regression.log)。保留失败记录，最终结果另见最终CTest日志。

截图示例：[Alice真实私聊](test-evidence/mvp4-native/M4-QQ-private-中文😀0.png)、[Bob收到QQ消息](test-evidence/mvp4-native/M4-QQ-private-中文😀1.png)、[三人群中Carol](test-evidence/mvp4-native/M4-QQ-group2.png)、[两人子群中Bob](test-evidence/mvp4-native/M4-QQ-subgroup1.png)、[微信单聊](test-evidence/mvp4-native/M4-WECHAT-private0.png)。完整12张对应4场景×3进程，日志逐一记录不同PID、READY、message ID与接收次数；子群/私聊Carol无相应接收记录。

## MVP1–MVP3 回归

开工前原9组通过；新增迁移后保留原协议、注册/密码/编号/服务激活、补开/绑定/好友推荐与群管理/策略/子群，原测试未删除。仅三份Python脚本当前user_version期望3→4，旧备份原版本断言保留。

| 原组 | 验证范围 |
|---|---|
| protocol | 严格UTF8/JSON、半包/粘包、异常帧与响应 |
| acceptance / client_ui | MVP1真实Server/Qt、持久化、超时/断线、原UUID |
| mvp2_acceptance / mvp2_ui | 好友、开通、绑定、共同好友、跨平台推荐和实际Qt流程 |
| group_policy / group_database | 真正虚函数动态派发、owner/subset/外键 |
| mvp3_acceptance / mvp3_ui | 21项群API、推送、策略切换/重启及真实Qt |

最终全套包含以上9组和mvp4_database、mvp4_acceptance、mvp4_ui、mvp4_receipts共13组。最终实际 **13/13组Pass、0 Fail，154.31秒**，见[最终CTest](test-evidence/mvp4-ctest-final.log)与[逐组原始输出](test-evidence/mvp4-ctest-details.log)。

## M4-T01–T22 逐项结果

| 项目 | 结果 | 实际断言与边界 |
|---|---|---|
| T01 三个独立Qt客户端真实连接登录 | Pass | 原生测试每轮启动3个不同PID的IMClient，经LoginWindow登录/服务激活；日志READY及message ID |
| T02 QQ好友单聊、唯一SQL消息 | Pass | TCP test02及3进程QQ私聊，Bob经event收到一次，SQL唯一 |
| T03 微信好友单聊/微博拒绝 | Pass | TCP test03及3进程微信场景，Weibo API拒绝 |
| T04 非好友/跨服务/未激活/伪造sender | Pass | TCP test04，拒绝无成功消息 |
| T05 QQ/微信群成员聊天、非成员隔离 | Pass | TCP test05；原生三人QQ群各接收一次 |
| T06 QQ子群只对有效子群成员 | Pass | TCP test06；原生Alice/Bob接收、Carol沉默 |
| T07 QQ子群策略停用、旧数据恢复 | Pass | TCP test07，切微信拒发/敏感查询；切回内容/成员保留 |
| T08 退群/踢出不能发、不能读未来 | Pass | TCP test08，ID窗口及退群/重入空档；文本时间归一到同秒仍严格过滤 |
| T09 Bob离线消息、重登sync完整有序 | Pass | TCP test09，after游标、多页去重/顺序 |
| T10 重启及数百条历史分页 | Pass | TCP test10，重启新连接取历史完整；Qt历史合并/更早页 |
| T11 半包/粘包/部分发送 | Pass | 原protocol+TCP test11，逐字节/合并帧、真实有界队列；超inflight属于限流而非丢帧 |
| T12 同UUID原消息、不重复存显 | Pass | TCP test12持久结果/无重发event，Qt模型合并消息ID及requestID |
| T13 DB写失败无stored/部分消息 | Pass | TCP test13测试触发器注入且无成功event；Qt同故障failed并恢复原正文 |
| T14 COMMIT成功响应丢失、原UUID重试 | Pass | TCP test14断连后原UUID恢复；Qt真实TCP故障代理补充unknown→同UUID→唯一stored行 |
| T15 多客户端并发、不冻结/崩溃 | Pass | TCP test15并发；Qt心跳计时器在恢复过程中继续运行 |
| T16 Server断开重启、Qt恢复与同步 | Pass | TCP test16及原生Qt自动认证/服务恢复，离线历史由服务器权威查询；原生测试在Alice恢复前由独立TCP生产者发送真实离线消息，恢复后恰一次合并，且GUI心跳继续 |
| T17 历史越权/跨服务/离群新消息 | Pass | TCP test17及epoch专项，有界limit/cursor与非法会话拒绝 |
| T18 真实接收者ACK/不伪造已读 | Pass | TCP test18；receipt专项私聊0/1→1/1、群部分1/2、重启保持；无read接口/UI |
| T19 中文/emoji/超长/非法报文 | Pass | TCP test19及原生中文emoji；50条接近4KiB且大量转义字符历史/sync按字节预算分页，不丢不重 |
| T20 真实旧库无损迁移与回归 | Pass | TCP test20真实schema1+002+003，逐表比较；DDL专项populatedv3保留4窗口及原数据；原9组回归 |
| T21 Qt Creator/CMake及本机完整人工演示 | **Not Run（部分已验证）** | CMake构建Pass、原生3进程自动QQ/微信单聊/QQ群/子群Pass，Qt Creator菜单完整人工操作及整套人工离线历史场景未执行，整项不标Pass |
| T22 优雅退出、慢端限流 | Pass | TCP test22向不读取接收端发1500×4004字节，实测EOF/reset，其他连接ping/history正常、1500SQL消息完整；活跃请求时SIGTERM退出0且<10秒 |

T01/T21不由PythonTCP脚本替代GUI声明；T13/T15/T16的服务端及Qt侧证据分开记录。测试触发器/透明TCP代理是有标注的故障注入，不是业务通道替代品。没有实现或声称read。

## 实际失败与修复

1. 旧MVP3 binary遇chat.*返回PROTOCOL_ERROR，新增测试先RED，后实现。
2. 初次T11夹具写了30条coalesced请求，超过原服务8inflight约束；改为6条保持真正粘包检查，限流由T22专门验证，未放宽产品上限。
3. 50×4096正文历史会超64KiB，审查后按真实JSON编码56KiB预算分页；含引号/反斜线/emoji的长正文专项通过，不以增大帧上限掩盖。
4. 首轮旧MVP2 UI出现target-platform pending失败；新增聊天页面重复后台查询造成竞争，去除无变化setAccount重复刷新并对list加inflight去重，保留旧用例及错误证据，修复后再完整回归。
5. 跨服务迟到open/sync响应通过service/generation检查防止选择错会话/跳过游标；ACK超时和自动认证超时可重新恢复，退出登录不自动重新登录。
6. 审查指出离线历史一次ACK50条会触发8inflight限流：补充离线批量历史Qt测试，ACK排队有界并由响应推进；未提高服务器限制来迁就客户端。修复前实测“50-message history ACK burst disconnected Bob”，日志mvp4-ui-ack-red.log，修复后断言50条历史、连接存活及50条持久送达。
7. 最近会话原遍历QHash丢失服务端排序，真实private→group→private切换置顶测试先FAIL，再按最新message ID和会话ID降序排序、保留迟到摘要的最大ID，测试GREEN；证据mvp4-ui-recent-red.log及最终Qt日志。
8. 最初持久回执未出现在历史结果，新增recipient_count/delivered_count，专项先KeyError RED，后私聊/群部分计数与重启GREEN。

## 未运行与限制

Qt Creator菜单人工构建、三个进程完整人工操作录像没有执行，T21保持Not Run。MVP3历史人工T22也不追溯改写。自动原生截图虽是真实程序，不能等同这些人工步骤。无公网/媒体/read功能，未测试这些范围。

客户端进程关闭不保存未确认发送的UUID/正文或草稿；重开可查服务器历史，未知发送须先查历史防手工重复。游标为进程内，防不同数据库复用同用户ID导致错误跳过。消息/receipt历史均由Server持久保存。
