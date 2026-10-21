# MVP2 实测报告

基线：e22f0fc。环境：本机 Apple Silicon macOS、Clang21、Qt6.12.0、CMake/Ninja、json-c0.19、OpenSSL3.6.5、SDK SQLite3.51.0。测试只使用新建临时库或本轮专用mvp2-demo；没有重置原用户库。

## 最终结果与证据

- 修改前MVP1：3/3组通过（19.37秒）。
- 最终清洁构建：成功，52步，无编译warning/error；[构建日志](mvp2-build-run.txt)。
- 最终CTest：**5/5组通过，58.06秒**；[完整输出](mvp2-ctest-run.txt)。其中protocol、acceptance、client_ui为MVP1回归；mvp2_acceptance为17项真实服务验收；mvp2_ui为真实Qt页面集成。
- 最终Qt代码在macOS Cocoa平台另行完整执行，退出码0；[原生日志](mvp2-native-ui-run.txt)。包括真实15秒无响应、断线原请求重试、推送去重及事件循环心跳。
- 五用户场景实际经TCP创建并查询校验，共同好友/跨平台推荐符合预期；[演示输出](mvp2-demo-run.txt)。

**M2-T01–T17：Pass。M2-T18：完整Qt Creator菜单+三个独立进程交互场景未完成（Not Run/部分已有证据），不能报告18项全通过。** 当前没有未修复的自动功能测试失败；界面工具限制列在下面。

## MVP1 T01–T14 回归

原tests/client_ui_main.cpp未改写；tests/acceptance.py仅把schema版本预期从1更新为2，旧业务断言保留。protocol测试新增规范JSON对象键顺序和整数类型检查；原拆帧/UTF8/超长异常响应/值语义断言保留。

| 编号 | 状态 | 实测证据 |
|---|---|---|
| T01 | Pass | Alice(QQ)真实注册、唯一ID与SQLite保存；Qt表单真实操作。 |
| T02 | Pass | Bob三服务，QQ/微博同号，微信独立。 |
| T03 | Pass | Carol只有微信，不生成QQ/微博。 |
| T04 | Pass | 正确密码查询服务与激活；Qt点击真实激活。 |
| T05 | Pass | 错误密码拒绝，Qt保留正确登录名。 |
| T06 | Pass | SIGINT后重启原临时库，仍可登录。 |
| T07 | Pass | SQL写入失败回滚原注册用户/服务/幂等/编号计数器，无部分保存。 |
| T08 | Pass（自动回归范围） | 原Qt测试三个真实窗口各登录不同用户，另启动三个独立IMClient进程检查存活；10ms心跳持续。本轮独立三进程分别交互登录的额外桌面演示未完成，不与此自动测试证据混同。 |
| T09 | Pass | 原注册同ID等价重放/冲突/并发仍生效。 |
| T10 | Pass | 服务器未启动清楚提示，启动后点击重连成功。 |
| T11 | Pass | 碎片/合帧/非法长度/UTF8/JSON/最大合法异常帧，其他连接仍可ping。 |
| T12 | Pass | schema2保持原外键、账号与登录名唯一约束。 |
| T13 | Pass | 捕获服务日志不含测试密码/auth完整JSON。 |
| T14 | Pass | 普通启动不清空；重启持久化、同库独占继续有效。 |

## MVP2 逐项验收

| 编号 | 状态 | 真实操作、断言与覆盖 |
|---|---|---|
| M2-T01 | Pass | 微信-only补QQ，再补微博复用共享号；重复开通无副本，新服务active=false；Qt补开也再次查询当前激活状态。 |
| M2-T02 | Pass | 同主体绑定/幂等绑定/get/解绑；传他人主键或主体参数拒绝，账号/好友/备注保留；Qt真实确认框执行。 |
| M2-T03 | Pass | QQ/微博同号码的搜索与好友严格分服务。 |
| M2-T04 | Pass | 展示号精确搜索、未开QQ者不出现；22条受限分页、非法参数拒绝；Qt21条分页、原搜索词保留。 |
| M2-T05 | Pass | 发送写PENDING，在线Bob通过socket receive取得kind=event/event_id；Qt申请页自动刷新。 |
| M2-T06 | Pass | 接受后双方一条无向关系，SQL规范化配对唯一；Qt两端刷新。 |
| M2-T07 | Pass | 拒绝不成为好友，同ID重试稳定重放，另ID再推进终态拒绝；Qt拒绝等待响应后移除。 |
| M2-T08 | Pass | 自身/重复/跨服务号码/重复好友拒绝；发送者不能审核；匿名及未激活请求拒绝；伪造actor不改变服务器身份；响应不泄漏内部主键。 |
| M2-T09 | Pass | 单向备注、清空备注、反向备注隔离；删除双向解除并清备注但保留微博关系；Qt危险操作真实确认。 |
| M2-T10 | Pass | 构造三角与二度关系，A/B共同好友准确；二度推荐排除自己/已有/待决，计算共同好友数与排序。 |
| M2-T11 | Pass | 两侧有效绑定才推荐Bob微信；目标已好友则不推荐，交集单独返回；候选解绑即排除，本人解绑返回BINDING_REQUIRED。 |
| M2-T12 | Pass | 未绑定、缺目标、未激活均不错误映射；微信↔微博也通过QQ绑定桥接；QQ↔微博共享主体号映射；无自动开通/自动加好友。 |
| M2-T13 | Pass | 在线新申请/处理/删除事件两端真实收到；目标离线再次登录能查待决并处理；Qt重复event_id只分派一次，不重复执行业务。 |
| M2-T14 | Pass | 真实触发器注入补开/接受写失败：计数器、申请状态、关系与mutation审计均回滚；可同ID重试。Qt静默peer真实15秒超时与断线，相同字节重试、心跳持续；正确输入保留。 |
| M2-T15 | Pass | 重启后绑定/备注/好友/申请/补开持久化；相同open UUID完整成功快照精确重放；普通启动不清理。 |
| M2-T16 | Pass | 两连接双向申请仅一成功；同申请双重审核仅一推进；所有新增mutation同ID重放不重复事件/关系，改payload/type/user冲突；未激活连接不能借旧ID绕过鉴权。 |
| M2-T17 | Pass | 用原v1 schema与真实旧KDF/注册幂等构造独立库；升级后原四表逐行一致、FK检查空、旧登录与注册重放可用；实际v1备份完整且重复启动不覆盖。 |
| M2-T18 | Not Run（完整场景；部分通过） | 清洁构建/服务端启动/三个Qt窗口会话/独立三个IMClient进程启动/原生Cocoa页面全流程均有通过证据。但Qt Creator AX读取报AXError.cannotComplete，未完成Creator菜单构建运行；额外独立三进程桌面交互因执行器结束后台进程、界面工具重启丢临时端口参数，未完整完成。README提供可复现手动步骤，不将这些未完成项标Pass。 |

## 重跑

```sh
cmake --build build/mvp2 --parallel 4
ctest --test-dir build/mvp2 --output-on-failure
python3 tests/mvp2_acceptance.py --server ./build/mvp2/IMServer
QT_QPA_PLATFORM=cocoa ./build/mvp2/Mvp2UiTestsDXX ./build/mvp2/IMServer
```

Qt自动测试是真实QWidget操作、QTcpSocket/POSIX网络、独立SQLite，不是模拟返回业务结果；只有超时/重复event边界用可控静默TCP peer。原生测试属于自动化Qt操作，不等同人工视觉穷举。offscreen字体/raise/sizeHint提示保留在日志，非编译警告，不影响退出码0。

## 失败历史及修复

1. 原MVP1二进制跑M2服务测试：0/17，均为未支持请求/schema1，预期红灯；保留原构建对照，没有伪报新增通过。
2. 首轮M2：16/17；T15发现open重放重算active导致data变化。将完整成功快照放入同一mutation事务，UI另读当前服务；最终17/17。
3. Qt首屏cursor空字符串被服务器正确拒绝；客户端改首屏"0"。测试确认框改为真实点击Yes按钮，避免done(Yes)没有clickedButton。
4. 慢网读回调可能相互覆盖；按查询族保留一个在途与一个最新后继，服务列表/绑定独立、好友查询合并；event刷新合并100ms，复跑Qt及原生通过。
5. 演示脚本误把helper返回的业务UUID字符串当对象，首次脚本退出失败；修正后显式重置本轮专用库并补推荐/共同好友校验，最终退出0。

## 其他未运行

ASan/TSan、长时间压力测试、其他Qt/macOS版本、跨机器/公网部署：Not Run。旧课设全功能、群组、多态策略、聊天：不属于本轮，未实现/未验收。GitHub远端CI未配置/未运行；本地测试通过不等于远端CI通过。
