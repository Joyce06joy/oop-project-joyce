# MVP4_IMPLEMENTATION_REPORT

日期：2026-10-22（Asia/Shanghai）。基线 `3bc6b017cf3bfab8c6653815a925649d26a8b845`，独立分支 `feature/mvp4-chat`。本轮增量实现文本即时通信，不开发媒体、公网部署、已读或下一轮视觉重构。

## 审计与兼容

见[MVP4_AUDIT.md](MVP4_AUDIT.md)。开工实际复跑MVP1–MVP3原9组全部通过，85.34秒，非依据前轮文字假定。MVP3历史T22完整Qt Creator菜单/独立进程人工验收尚未执行，本轮不冒称补过。已有四个本机库只读SQLite backup并校验，原库未升级或清空，新demo独立mvp4-demo。v4正常打开旧库时仍先生成版本对应快照，再增量迁移。

复用已有连接会话/服务激活、SocialRepository好友与账号、MutationRepository成功结果幂等、RequestDispatcher事件队列、POSIX poll与DBworker、QTcpSocket异步信号；旧好友/群组业务未重写。原服务平台不因管理策略切换改变。

## 已实现

- QQ/微信同平台好友单聊，canonical无向对唯一；非好友/跨服务/未激活/伪造sender拒绝。删除好友保留过去私聊历史，禁止新发送。
- QQ/微信主群聊天、原属QQ的临时子群聊天。主群需当前有效成员；子群同时需有效父/子群成员、父群QQ策略且原平台QQ。群主/管理员不绕过子群成员资格。
- SQLite持久文本、认证发送账号、稳定message ID、服务器UTC；消息/初始回执/成功幂等结果原子COMMIT后才stored和event。
- 实际不同TCP连接收发推送，离线sync与分页历史；ID去重，提交后响应丢失原UUID重试不重复写入/推送。
- 多期成员窗口以message ID而非秒级时间授权，退出后不能查看未来、重入不能读取离群空档。旧消息/角色/子群随策略切换保留；微信策略停用QQ子群聊天，切回恢复。
- 真实接收者独立ACK，落库delivered_at；历史/消息提供recipient_count、delivered_count，群部分送达不冒称全群；没有read协议或已读标签。
- Qt消息页、最近会话、好友/群/子群入口、文本输入、pending/stored/unknown/failed、原UUID重试、每会话草稿、未读提示、历史向上分页、跨event/history合并。
- 有上限的异步重连、内存会话重新登录和服务恢复。主线程无同步Socket等待，恢复中UI计时器继续运行。
- 服务端每连接8个inflight、64连接、约512KiB输出/待投递结果限制；慢客户端断开，不阻塞DBworker。Qt Socket发送队列约1MiB限额，停止时正常关闭FD、排空已入队任务、join线程。

## 文件变化与职责

| 文件 | 内容 |
|---|---|
| database/migrations/004_chat.sql、MigrationV4DXX.h.in | conversation/pair/message/receipt/epoch与成员触发器 |
| server/persistence/DatabaseDXX.cpp | v4版本与v3等旧版备份/迁移兼容 |
| server/persistence/MessageRepositoryDXX.{h,cpp} | 会话/消息SQL、canonical pair摘要、发送资格/窗口可见性与回执计数 |
| server/application/MessageServiceDXX.{h,cpp} | 请求鉴权、消息/ACK事务、历史/sync及相关推送 |
| RequestDispatcherDXX.{h,cpp} | 8种chat命令、原会话与mutation集成 |
| server/network/TcpServerDXX.cpp | 有界结果队列、慢连接回收、响应编码失败保留关联ID |
| client/ui/ChatPageDXX.{h,cpp} | 异步聊天控制/展示、草稿/重试/历史/未读 |
| client/ui/MessageModelDXX.{h,cpp} | 消息ID/UUID合并、排序与真实状态 |
| client/network/TcpClientDXX.{h,cpp} | 有上限退避、内存重新认证/服务恢复、发送队列限制 |
| Workspace/FriendPage/GroupPage/SubgroupPage/LoginWindow | 追加消息第六页、入口信号、微博隐藏、sessionRestored；原0–4页索引保留 |
| client/main.cpp | 正常入口保留；可选--chat-smoke异步驱动实际IMClient，供独立进程验收 |
| tests/mvp4_database.py | populated v3无损迁移、同秒epochs、父子级联、解散 |
| tests/mvp4_acceptance.py | 真实TCP20项、SQL故障/丢响应/长正文/并发/慢连接/迁移 |
| tests/mvp4_receipts.py | 私聊完整/群部分送达，重启仍真实 |
| tests/mvp4_ui_main.cpp | 三独立IMClient PID/Socket，多平台/群/子群，Qt故障/草稿/去重/恢复 |
| 原acceptance/mvp2_acceptance/mvp3_acceptance | 仅当前schema断言3→4，旧备份版本及业务断言保留 |
| CMakeLists.txt | 新资源/源码与4组新测试，原目标保留 |
| demo/seed_mvp4.py | 专用新库、三人好友/群/子群和真实消息，已有库拒绝覆盖 |
| docs与README | 审计/合同/任务书、协议、DB/ER、UML/线程、实施/逐项测试及原始证据 |

所有新增自定义C++类DXX后缀、独立h/cpp。建议的ConversationDXX/MessageDXX/ChatControllerDXX等没有为凑类数机械拆空壳；消息值沿用JsonDXX，控制器职责由ChatPage承担，网络连接/DBworker继续复用现有结构。

## 通信与线程

Qt mainloop→QTcpSocket frame→POSIX网络线程→mutex任务队列→唯一DBworker/Dispatcher→事务/SQLite→有界result/event队列→网络线程write buffer→各客户端Socket信号/模型。worker不直接send，也不等待慢客户端读取。partial recv由原ProtocolDXX积累，partial send保留未发送尾部，响应由request_id匹配，不依赖到达顺序。

历史SQL先取limit+1，同时按实际JSON编码预算56KiB裁剪，再返回不超过64KiB envelope；转义字符导致扩大的正文也不会超帧。降序查最近页先裁留最近一段，再反转升序；游标只指向实际返回边界，后页不丢/重。

## 验证与修复

实际结果与命令见[MVP4_TEST_REPORT.md](MVP4_TEST_REPORT.md)，原始日志/截图一并交付。服务端20项首轮完整通过，原生Qt三个独立进程通信通过；最终全套13/13组通过、0失败，154.31秒；具体逐项范围与Not Run见测试报告和原始日志。

实施中审查并修复了长正文分页超帧、跨服务迟到open/sync响应、ACK与恢复超时状态、持久同步游标与不同测试库ID碰撞、回执历史恢复等问题。保留实际RED/FAIL证据，不将修复前结果写为通过。原MVP2 UI回归的实际诊断/结果在测试报告单列。

## 明确边界与遗留

1. delivered是实际ACK；群按具体接收者或x/y计数，没有“已读”。消息历史的stored表示持久保存，送达计数独立存在。
2. 游标/草稿/待确认正文为当前客户端进程内状态；进程重启从服务器会话与最近历史恢复。未将密码/正文写客户端SQLite；重新连接所需登录资料只保留内存，退出登录清除。完全退出时未知消息原UUID不持久保存，重新发送前应查历史。
3. 原属微信子群即使切QQ管理策略也不开放聊天，符合MVP4对QQ子群原服务限制；MVP3的管理数据不删除。
4. 默认最近50，实际长文页可少于50；历史全量保留于Server，按需要分页查询。未提供全量一次加载/导出，未新增聊天删除或媒体功能。
5. 自动原生三进程是实际程序/PID/连接，非单窗口模拟；Qt Creator菜单完整人工验收若未执行仍标Not Run，不能用CMake或自动化替代。
6. 本地教学回环明文TCP，单DBworker也做密码KDF；面向一台Mac、小规模并发。没有公网部署能力。

## 下一轮接口

可复用chat会话/历史/sync/ack与消息ID模型，group/member窗口已经持久化。后续视觉优化应保留异步UUID关联、成员区间和服务端鉴权，不能把UI可点击状态当授权。若未来要已读、消息撤回、媒体，需要分别设计真实read回执/权限/持久状态，不得据delivered自动显示已读。本轮达到MVP4停止，不预做MVP5或答辩报告。

## 代码提交

工作保存在feature/mvp4-chat，基于上述MVP3 commit；不合并main、不强推或覆盖其他分支。构建产物与本机数据库不加入Git；源码、脚本、文档及证据一起提交。实际commit/GitHub发布核验在最终交付说明及Git记录中给出，不将仅本机commit混称push。
