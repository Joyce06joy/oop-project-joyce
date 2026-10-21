# MVP 1 测试报告

日期：2026-10-21（Asia/Shanghai）。实际环境：Apple Silicon macOS、Apple Clang 21.0.0、Qt 6.12.0、OpenSSL 3.6.5、json-c 0.19、SDK SQLite 3.51.0。构建：C++17 Debug，CMake/Ninja。所有验收数据库均为独立临时库或本轮专用演示库；没有使用/重置旧课程项目数据库。

## 最终结果

T01–T14 均 **Pass**，由真实服务/协议测试、Qt Widgets 自动操作和三个独立 Mac 客户端原生操作共同覆盖。最终 CTest 三组测试均通过（protocol、acceptance、client_ui），Cocoa 原生 UI 测试另行通过。完整证据：[ctest-run.txt](ctest-run.txt)、[native-ui-run.txt](native-ui-run.txt)、[T08 原生实测记录](T08_NATIVE_OBSERVATIONS.md)、[演示生成输出](demo-run.txt)。这些日志保留实际输出中的平台提示，不隐藏警告。

| 编号 | 状态 | 实际操作与证据 |
|---|---|---|
| T01 | Pass | 真实 TCP 注册 Alice(QQ)，检查成功 ID；SQL 查询确认已保存；Qt 表单另以 UiUser0 覆盖相同流程。专用演示 Alice 为 user_id 1 / QQ 10001。 |
| T02 | Pass | Bob 三服务注册，自动检查 QQ/WEIBO 同号及 WX 前缀；原生 Bob 窗口实际显示 QQ 10002、WEIBO 10002、WECHAT WX8001。 |
| T03 | Pass | Carol 仅微信，自动断言响应只有 WECHAT；原生窗口仅有 WX8002，不存在 QQ 按钮。 |
| T04 | Pass | 正确密码登录，取得服务列表，激活已开通服务后查询 active=true；未开通服务拒绝。Qt 自动及三个独立原生进程均真实点击激活成功。 |
| T05 | Pass | 错误密码与不存在用户返回同一 AUTH_INVALID_CREDENTIALS；Qt 实际输入错误密码，等待错误提示并检查原登录名仍存在。 |
| T06 | Pass | SIGINT 停止服务端后用同一库重启，原 Bob 正确密码仍能登录，服务信息保留。 |
| T07 | Pass | SQLite BEFORE INSERT accounts 触发器注入真实写入失败：返回 DATABASE_ERROR；用户、账号、幂等记录无部分写入，两编号计数器未变化。Qt 另用可控测试 peer 返回 DATABASE_ERROR，检查保留原载荷与 ID、可重试，未显示成功。 |
| T08 | Pass | Qt 自动创建三个真实窗口，各注册/登录不同用户并激活服务，10 ms 心跳确认事件循环响应；另外启动三个独立 IMClient 进程验证存活。最终在 Mac 直接打开同二进制的三个独立 .app 副本，分别登录 Alice/Bob/Carol 并激活，三个窗口再次观测均保持登录；详见原生记录。 |
| T09 | Pass | 同 ID 重放规范化后等价的注册（服务顺序及登录名大小写变化）返回同一结果；改昵称冲突。三连接同时提交同 ID，仅一个用户/一组编号/一条幂等记录。重启后重放仍成功。 |
| T10 | Pass | Qt 窗口在未启动服务器的端口打开，等待明确连接失败提示；启动服务端后点击重试并连上。 |
| T11 | Pass | C++ 协议单测及真实 TCP：逐字节碎片、连续两帧、0/65537 长度、非法 UTF-8/JSON，错误连接安全拒绝，其他连接 ping 仍成功；最大合法异常请求也不能终止服务端。 |
| T12 | Pass | 启动验证服务器 foreign_keys=ON；自动 SQL 检查 schema v1，外键孤儿插入、重复服务账号及大小写重复登录名均触发约束。 |
| T13 | Pass | 捕获测试服务端 stdout/stderr，断言没有演示密码或完整 password JSON；代码仅记录白名单请求类型、连接序号及不含数据的异常。数据库保存带随机盐和参数的 PBKDF2；审计 fingerprint 无明文密码。 |
| T14 | Pass | 两轮正常关闭/重启后原库继续可用，普通启动不清空数据；第二服务端即使换端口也不能占用同一数据库。演示库只由显式 seed/reset 生成，普通 IMServer 启动不会调用。 |

## 复跑命令

在根目录构建后：

```sh
"$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/ctest" \
  --test-dir build/mvp1 --output-on-failure
QT_QPA_PLATFORM=cocoa ./build/mvp1/ClientUiTestsDXX \
  ./build/mvp1/IMServer ./build/mvp1/IMClient.app/Contents/MacOS/IMClient
python3 tests/acceptance.py --server ./build/mvp1/IMServer
```

自动测试选择临时端口，不要求停掉默认 8888 上的演示服务器；每个测试的临时数据退出时删除。测试用可控静默 peer 验证真实 15 秒注册超时；主线程心跳持续跳动，点击重试发出的字节（包括原 UUID 与载荷）完全相同。模拟已失效会话的 AUTH_REQUIRED 后实际检查返回登录页。客户端正常生产代码没有测试账号或自动登录开关。

## 原生操作步骤与结果

通过 Mac UI 自动化直接操作三个原生应用（同一 IMClient 二进制，副本只改应用标识方便定位）：输入演示账户→点击登录→点击对应服务→重新读取三个窗口。实际 Alice QQ 已激活、Bob 微信已激活、Carol 微信已激活，连接状态均为已连接。另查看 Bob 原生窗口截图，资料和服务号码可读，布局完整。Qt 自动化在 Cocoa 平台也重复表单、错误字段保留和超时检查；这与人工视觉检查是不同证据。

## 过程中失败及处理

以下是实际遇到并修复/解除的历史结果，不计作最终通过依据：

- **Fail（预期红灯）**：首个验收在 IMServer 未实现时提示可执行文件不存在。
- **Not Run（环境）**：初次集成执行被沙箱禁止回环 bind；允许本机 TCP 后才真正运行。
- **Fail（构建配置）**：Homebrew json-c 0.19 的 CMake 包未提供版本元数据，版本型 find_package 被拒；改为配置包发现并在编译时检查 JSON_C_VERSION_NUM ≥0.16。
- **Fail（测试自身）**：等待 helper 对会消费 stdout 的条件求值两次，造成“server ready”假失败；改为只保存并检查最后一次结果。
- **Fail（产品缺陷）**：Qt 连接析构期间的 Socket 信号访问已销毁解析器，macOS 崩溃堆栈确认；显式析构先断开回调。最终测试包括仍连接状态窗口析构并通过。
- **Fail（产品回归）**：数据库错误后重试原请求 ID 的测试先失败，再修复注册暂态错误处理。
- **Fail（独立审查回归）**：合法 64 KiB 异常请求导致错误响应越界，其他连接 EOF；修复有限回显及网络编码异常隔离。
- **Fail（独立审查回归）**：退出重试 AUTH_REQUIRED 后服务页未恢复；修复失效会话回到登录页。
- **Fail（值语义回归）**：复制 JSON 后 set 改动原对象；改为深拷贝，避免工作线程与网络结果共享可变 JSON。

最终上述回归与原测试一起通过。offscreen 的 `raise/propagateSizeHints` 不支持及字体别名提示、Cocoa 输入法的 mach-port 提示属于本机平台运行提示；原生日志保留，测试退出码为 0。未把这些提示声称为编译警告，也未用隐藏日志处理。

## 未运行项目

| 项目 | 状态 | 原因/边界 |
|---|---|---|
| GitHub 远端拉取、推送及远端 CI | Not Run | CLI DNS 失败，GitHub 连接器返回 404，当前私库授权不可确认。 |
| 通过 Qt Creator GUI 点选所有构建/运行菜单 | Not Run | 实际使用同一 Qt Kit 的 CMake/Ninja 编译运行；已提供 Creator 逐步说明，未冒充 GUI 菜单验收。 |
| 其他 Mac/Qt 版本、Windows、跨机器/公网 | Not Run | 本轮仅本机 Mac；无对外监听。 |
| ASan/TSan、压力/长时浸泡、所有控件的人工视觉穷举 | Not Run | 本轮功能/异常/并发规模验收以外，未据此宣称生产级稳定性。 |
| 原旧工程完整课程功能 | Not Run | 不属于 MVP1，也未修改旧工程。 |
