# IMPlatform — MVP 1

一台 Mac 上运行一个 POSIX TCP 服务端和多个 Qt 6 Widgets 客户端。本轮只提供统一注册、密码登录、自动编号、服务选择及 SQLite 持久化，不包含好友、群组或聊天。

## 本机依赖与构建

需要 Apple Command Line Tools、Qt 6（macOS 的 Widgets/Network；测试另需 Qt Test）、CMake ≥3.16、SQLite3、json-c ≥0.16、OpenSSL ≥3、Python 3（验收与演示脚本）。第三方 JSON 使用 json-c，测试版本 0.19；OpenSSL 使用 3.6.5；SQLite 使用 macOS SDK 的 3.51.0。依赖均在构建时链接，无运行时下载。json-c 为 MIT 许可证，OpenSSL 3 为 Apache-2.0；均由本机安装提供，未复制第三方源码。

Qt 可用官方 Qt 安装器安装 Desktop/macOS Kit；其余缺失依赖可通过 Homebrew 安装：

```sh
xcode-select --install
brew install json-c openssl@3
```

这些是按需安装命令，本轮没有自动更新你的 Qt 或 Homebrew 包。CMake 会搜索 Apple Silicon `/opt/homebrew/opt` 和 Intel `/usr/local/opt`；其他位置可配置 `json-c_DIR` 与 `OPENSSL_ROOT_DIR`。

本机已验证的构建方式，在仓库根目录运行（Qt 路径根据实际安装修改）：

```sh
export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake -S . -B build/mvp1 -G Ninja \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mvp1 --parallel 4
ctest --test-dir build/mvp1 --output-on-failure
```

没有 Ninja 时可去掉 `-G Ninja`，并使用新的构建目录。只构建程序可加 `-DBUILD_TESTING=OFF`。在限制回环网络的执行沙箱中，集成测试需要允许本机 TCP；不能把 PermissionError 当作产品测试通过。

## Qt Creator 打开、构建、运行

1. 打开本仓库根目录 `CMakeLists.txt`，选择 **Qt 6.12.0 for macOS**（或已安装的 Qt 6 Desktop Kit），Debug 构建。
2. 若提示找不到 json-c/OpenSSL，在 Projects → Build → CMake 中添加 `json-c_DIR=/opt/homebrew/opt/json-c/lib/cmake/json-c`、`OPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3`。Intel Mac 相应改成 `/usr/local/opt`。
3. 点击 Configure/Build。此工程保留原 `oop-project-Joyce` 空窗口目标，正式 MVP 1 要选择 **IMServer** 和 **IMClient**。
4. 在 Projects → Run 选择 **IMServer**。建议参数 `--db /Users/joyce/oop-project-Joyce/data/manual.sqlite3`，先启动服务端，输出 `IMServer listening 127.0.0.1:8888` 后再启动客户端。也可使用默认数据路径。
5. 切换运行目标 **IMClient** 并运行。启动三个独立实例最稳定的方式是在三个终端分别执行下列客户端命令；关闭其中一个不会停止服务器。Qt Creator 的“重复运行”设置可能会终止上次运行，因此不依赖该行为。
6. 服务端可在终端按 Ctrl+C 正常退出。停止服务器后客户端显示连接丢失；重启后点击“重试连接”，重新登录。

命令行运行：

```sh
./build/mvp1/IMServer --db ./data/manual.sqlite3
```

另开三个终端，每个执行一次：

```sh
./build/mvp1/IMClient.app/Contents/MacOS/IMClient
```

仅监听 `127.0.0.1`；默认端口 8888，两个程序都支持 `--port 8890` 等自选端口，必须匹配。不提供对外绑定选项。

## 操作与数据规则

- 注册登录名为 3–32 位 ASCII 英文、数字或下划线，大小写不敏感；服务器统一小写保存，SQL 使用同样的 NOCASE 唯一约束。昵称 1–96 UTF-8 字节；密码 8–128 UTF-8 字节；所在地最多 160 字节；生日可留空，填写时限 1900 年至今天的有效日期。
- 勾选至少一种服务。QQ/微博共享号码（10001 起），同一用户同时开通二者时号码相同；微信号码 WX8001 起。仅开通微信不会生成 QQ/微博账号或消耗共享编号。
- 注册成功后页面显示用户 ID、UTC 注册时间和各服务号码，可复制。返回登录，用登录名和密码登录，再选择已开通服务。激活状态保存在当前连接会话内；退出或断线后失效。
- 表单错误保留输入；注册提交期间禁用重复提交。超时、断线或临时数据库错误时，不把未知结果认定为失败：保留并锁定原输入，恢复连接后重试原请求。已确认成功才显示成功界面。
- 未知注册请求保留在当前客户端内存中；完全退出客户端会丢失待重试的请求编号。如需避免重复，优先登录原登录名确认结果。本阶段不持久化未确认请求或密码。
- 窗口每次运行可完成一次注册；若要继续创建另一个账号，可再开一个客户端。

SQLite **仅服务端打开**。不指定 `--db` 时路径是：

```text
~/Library/Application Support/IMPlatformMVP1/implatform.sqlite3
```

`--db` 支持新路径并创建父目录。数据库旁的 `.lock` 文件用于进程独占；不要删除正在使用的锁文件。普通启动不清空任何用户。版本 0 的空数据库从 `database/schema.sql` 初始化为版本 1；未知非空数据库或不支持的版本拒绝启动，不迁移/覆盖旧课程项目的数据。

## 三用户演示与显式重置

先构建，然后在根目录执行：

```sh
python3 demo/seed.py
./build/mvp1/IMServer --db ./data/mvp1-demo.sqlite3
```

脚本通过真正的 TCP 注册生成 Alice（QQ）、Bob（QQ/微信/微博）、Carol（仅微信），登录名不区分大小写；演示密码均为 `Mvp1-Pass-2026!`。脚本使用临时服务器，完成后关闭；随后请按上述命令启动正式演示服务器。

**只有明确执行下面命令才重置专用演示数据**（先关闭使用该库的服务器）：

```sh
python3 demo/seed.py --reset
```

重置范围固定为本仓库 `data/mvp1-demo.sqlite3` 及 WAL/SHM，不影响默认数据库、manual.sqlite3 或旧课设目录。已有演示库未传 `--reset` 时脚本拒绝覆盖；正常程序启动不会调用脚本。`--server /path/to/IMServer` 可指定 Qt Creator 构建出的服务器。

## 验证与文档

- `tests/acceptance.py`：真实 TCP/SQLite 验收，独立临时数据库；包括异常写入回滚、并发幂等、重启、非法报文及服务器存活。
- `tests/protocol_main.cpp`：分帧/合帧、严格 UTF-8/JSON、JSON 深拷贝、异常响应大小。
- `tests/client_ui_main.cpp`：实际 Qt Widgets 操作，注册/错误密码保留字段/服务激活、三窗口、独立进程、断线、15 秒超时及原请求重试、退出恢复。CTest 默认 offscreen；可直接运行 `QT_QPA_PLATFORM=cocoa ./build/mvp1/ClientUiTestsDXX ./build/mvp1/IMServer ./build/mvp1/IMClient.app/Contents/MacOS/IMClient` 检查 macOS 原生平台。
- [逐项测试报告](docs/TEST_REPORT.md)
- [实施报告与简化图](docs/MVP1_IMPLEMENTATION_REPORT.md)
- [协议说明](docs/PROTOCOL.md)

密码使用随机 16 字节盐的 PBKDF2-HMAC-SHA256（600000 轮、32 字节输出），保存算法、参数、盐及派生值。会话随机且绑定连接；不接受通过另一个连接提交 session_id 冒用登录。日志仅记录连接事件、批准的请求类型和不含字段值的错误。

本地教学流量经回环 TCP 明文传递；本工程不提供公网安全部署能力。服务端串行数据库工作线程也执行密码 KDF，小规模演示请求可能排队；网络与 Qt GUI 事件循环独立，避免阻塞界面。
