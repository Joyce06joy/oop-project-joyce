# IMPlatform — MVP 3

一台 Mac 上运行一个 POSIX TCP 服务端和多个 Qt 6 Widgets 客户端。在保留 MVP1 注册、密码登录、自动编号及持久化基础上，增加服务补开通、QQ/微信绑定、好友管理、共同好友与推荐、真实 Socket 事件推送。MVP3 增加 QQ/微信群、审批/邀请、角色权限、子群和运行时策略切换；本轮不包含聊天。

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
cmake -S . -B build/mvp3 -G Ninja \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/macos" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mvp3 --parallel 4
ctest --test-dir build/mvp3 --output-on-failure
```

没有 Ninja 时可去掉 `-G Ninja`，并使用新的构建目录。只构建程序可加 `-DBUILD_TESTING=OFF`。在限制回环网络的执行沙箱中，集成测试需要允许本机 TCP；不能把 PermissionError 当作产品测试通过。

## Qt Creator 打开、构建、运行

1. 打开本仓库根目录 `CMakeLists.txt`，选择 **Qt 6.12.0 for macOS**（或已安装的 Qt 6 Desktop Kit），Debug 构建。
2. 若提示找不到 json-c/OpenSSL，在 Projects → Build → CMake 中添加 `json-c_DIR=/opt/homebrew/opt/json-c/lib/cmake/json-c`、`OPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3`。Intel Mac 相应改成 `/usr/local/opt`。
3. 点击 Configure/Build。此工程保留原 `oop-project-Joyce` 空窗口目标，正式程序要选择 **IMServer** 和 **IMClient**。
4. 在 Projects → Run 选择 **IMServer**。建议参数 `--db /Users/joyce/oop-project-Joyce/data/manual.sqlite3`，先启动服务端，输出 `IMServer listening 127.0.0.1:8888` 后再启动客户端。也可使用默认数据路径。
5. 切换运行目标 **IMClient** 并运行。启动三个独立实例最稳定的方式是在三个终端分别执行下列客户端命令；关闭其中一个不会停止服务器。Qt Creator 的“重复运行”设置可能会终止上次运行，因此不依赖该行为。
6. 服务端可在终端按 Ctrl+C 正常退出。停止服务器后客户端显示连接丢失；重启后点击“重试连接”，重新登录。

命令行运行：

```sh
./build/mvp3/IMServer --db ./data/manual.sqlite3
```

另开三个终端，每个执行一次：

```sh
./build/mvp3/IMClient.app/Contents/MacOS/IMClient
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

`--db` 支持新路径并创建父目录。数据库旁的 `.lock` 文件用于进程独占；不要删除正在使用的锁文件。普通启动不清空任何用户。版本 0 的空数据库先从 `database/schema.sql` 初始化 v1，再依序应用 `002_friends.sql` 和 `003_groups.sql` 升级 v3；已有 v1/v2 库先做 SQLite 在线备份并校验，再事务升级。备份名为原路径加 `.v1-backup.sqlite3` 或 `.v2-backup.sqlite3`，已存在时追加序号，绝不覆盖。原 users、accounts、计数器和注册幂等记录不重建。未知非空库或不支持的版本仍拒绝启动；旧课设目录数据不会自动读取。

## MVP2 五用户演示与显式重置

先构建，然后在根目录执行：

```sh
python3 demo/seed_mvp2.py
./build/mvp3/IMServer --db ./data/mvp2-demo.sqlite3
```

Alice、Bob、David 已有三服务，Carol 只有微信，Emily 只有 QQ；演示密码均为 `Mvp2-Pass-2026!`，仅用于本机演示。脚本使用独立临时端口通过真实 TCP 生成场景后正常停止，随后请用上面的命令启动演示服务器，再启动三个 IMClient 分别登录。

1. Alice 与 Bob 先分别点击上方 QQ 服务确认激活。左侧选择 QQ，“好友”页查看真实账号及 Alice 对 Bob 的单向备注。
2. Alice 的 QQ 好友有 Bob、David；Bob 的 QQ 好友有 Alice、David、Emily。“推荐”选同平台推荐可看到 Emily；共同好友填 Bob 的 QQ 展示 ID 可看到 David。
3. Alice、Bob 已绑定自己的 QQ/微信，David 刻意未绑定。两人在各自窗口手动激活微信后，Alice 在推荐页选择 QQ→WECHAT，可以推荐 Bob 的微信；David 的微信不会因同 user_id 自动映射。
4. 在推荐结果点击申请，Bob 选择微信空间、“申请”页可实时收到，再同意/拒绝。重复推送只刷新，不重复执行业务。接受后双方好友列表实时变化。
5. 微博空间独立：Bob 给 Alice 有一条尚未处理的微博申请，激活微博后可查询；QQ/微博同号不代表共享好友关系。
6. Carol 在账号页补开 QQ，开通后需再次点击上方 QQ 进行激活；再补微博会复用同一展示号码。绑定/解绑只作用于本人账号；解绑后推荐映射立即改变，平台好友保留。
7. 删除好友需确认，删除双向关系和双方备注；其他平台关系不受影响。

只有明确执行下列命令才会重置专用 MVP2 演示数据，执行前先关闭使用该库的服务器：

```sh
python3 demo/seed_mvp2.py --reset
```

重置只处理 `data/mvp2-demo.sqlite3` 及 WAL/SHM；不会清理默认数据库、manual.sqlite3、mvp1-demo.sqlite3 或旧课设库。已有库不传 `--reset` 会拒绝覆盖。原三用户 `demo/seed.py` 仍保留，使用不同的专用 mvp1-demo 库。

## MVP2 界面与失败恢复

登录后保留原服务激活按钮，在同一窗口嵌入 QQ/微信/微博空间和“好友、申请、推荐、账号”四页。所有操作复用同一异步 QTcpSocket；资料只读展示，不显示密码哈希、会话 token 或内部 account_pk。

- 搜索可用展示号码精确查找或昵称文字匹配，每页最多20条，提供下一页；昵称不是身份依据。
- “申请”同时显示收到/发出的 PENDING，只有收到的申请可接受/拒绝；双向申请竞争时先处理原待决申请。
- 推荐需先手动激活来源和目标服务；凡涉及微信，本人和被映射候选均须有有效 QQ/微信绑定。推荐按钮只发申请，不能直接复制关系。
- 写操作等待服务器确认；超时、断线或临时数据库错误时保留原 UUID/内容并显示“重试原操作”。先重新连接、登录同一主体并激活相应服务，再重试；完全退出客户端或切换其他主体不会持久保存待确认内容。
- 服务器推送事件更新页面并合并刷新；通知不是唯一事实来源，重登/切换申请页重新查询可恢复离线申请。

## 验证与文档

新增 `tests/mvp2_acceptance.py` 覆盖17项服务/迁移/推送测试，`tests/mvp2_ui_main.cpp` 覆盖真实 Qt 好友、账号与推荐流程。MVP2 阶段 `ctest` 为5组（MVP3 当前为9组）：旧协议、旧服务、旧Qt UI、新服务、新Qt UI。新增 UI 原生运行：

```sh
QT_QPA_PLATFORM=cocoa ./build/mvp3/Mvp2UiTestsDXX ./build/mvp3/IMServer
```


- `tests/acceptance.py`：真实 TCP/SQLite 验收，独立临时数据库；包括异常写入回滚、并发幂等、重启、非法报文及服务器存活。
- `tests/protocol_main.cpp`：分帧/合帧、严格 UTF-8/JSON、JSON 深拷贝、异常响应大小。
- `tests/client_ui_main.cpp`：实际 Qt Widgets 操作，注册/错误密码保留字段/服务激活、三窗口、独立进程、断线、15 秒超时及原请求重试、退出恢复。CTest 默认 offscreen；可直接运行 `QT_QPA_PLATFORM=cocoa ./build/mvp3/ClientUiTestsDXX ./build/mvp3/IMServer ./build/mvp3/IMClient.app/Contents/MacOS/IMClient` 检查 macOS 原生平台。
- [MVP2 逐项测试报告（含 MVP1 回归）](docs/MVP2_TEST_REPORT.md)
- [MVP1 历史报告](docs/TEST_REPORT.md)
- [MVP2 实施报告与文件变化](docs/MVP2_IMPLEMENTATION_REPORT.md)
- [MVP2 UML / ER / 状态机](docs/MVP2_DIAGRAMS.md)
- [MVP2 数据库迁移说明](docs/DB_SCHEMA_V2.md)
- [MVP2 协议与事件](docs/PROTOCOL_V2.md)（保留[MVP1协议](docs/PROTOCOL.md)兼容）

密码使用随机 16 字节盐的 PBKDF2-HMAC-SHA256（600000 轮、32 字节输出），保存算法、参数、盐及派生值。会话随机且绑定连接；不接受通过另一个连接提交 session_id 冒用登录。日志仅记录连接事件、批准的请求类型和不含字段值的错误。

本地教学流量经回环 TCP 明文传递；本工程不提供公网安全部署能力。服务端串行数据库工作线程也执行密码 KDF，小规模演示请求可能排队；网络与 Qt GUI 事件循环独立，避免阻塞界面。


## MVP3 群组演示

```sh
python3 demo/seed_mvp3.py
./build/mvp3/IMServer --db ./data/mvp3-demo.sqlite3
# 在另外三个终端分别启动同一客户端命令
./build/mvp3/IMClient.app/Contents/MacOS/IMClient
```

演示账号 Alice、Bob、Carol、David；密码均为 `Mvp3-Pass-2026!`，只用于本机演示。QQ展示号10001–10004，微信WX8001–WX8004。两个平台各预置群号1001–1006，新建群继续分配1007起，不限制为固定六群。

1. 三个客户端分别登录Alice/Bob/Carol，点击上方QQ激活，左侧QQ空间选择“群组”。查询1001并选择结果，再进入“详情与成员”。Alice群主、Bob管理员、Carol普通成员；临时子群包括Alice与Carol。
2. Alice/Bob在“申请与邀请”查看David待处理申请，可接受或拒绝。第四个客户端David也可在另一群申请。审批后成员与请求通过Socket刷新。
3. Bob可维护公告、审批、踢普通成员，不能设管理员或踢群主/其他管理员；Alice可设/撤管理员、改群名。Carol无管理特权。踢人/退出均清理该成员所属子群关系。群主不能直接退出，可二次确认解散。
4. Alice选择“切换群管理模式…”，阅读成员/原属服务不变、管理员与子群暂停、待决请求取消的影响，确定后再次确认。QQ原群切微信规则后Bob管理按钮禁用，旧stored ADMIN保留；子群不可见/操作；成员仍为原QQ账号。切回QQ后角色权限与原子群恢复，已取消请求不复活。
5. 激活微信、切换微信空间，查询1004。Alice群主、Bob普通成员；Carol有Bob发出的待确认邀请。Carol本人接受/拒绝；邀请者不能代确认。Bob能邀请，却不能维护公告/踢人/设管理员/建子群；直接申请被服务器拒绝。
6. 群组页支持搜索、仅我加入、分页、详情、成员、请求、子群成员增删。微博空间隐藏群组入口。断线未知结果保留原操作，重连登录同一用户并激活原服务后重试；失败不认定成功。数据从服务器查询，重启仍保留策略/角色/子群。

仅明确需要重置专用演示库且已停止其服务器时使用 `python3 demo/seed_mvp3.py --reset`；不会覆盖其他MVP库或默认用户库。初次运行发现已有库会拒绝覆盖。已有真实库由服务器自动备份迁移；恢复说明见数据库文档。

## MVP3 验收资料

```sh
ctest --test-dir build/mvp3 --output-on-failure
QT_QPA_PLATFORM=cocoa ./build/mvp3/Mvp3UiTestsDXX ./build/mvp3/IMServer
```

当前共9组CTest，包含MVP1/MVP2原5组、纯C++策略、数据库约束、MVP3真实TCP和Qt群界面。Qt Creator的实际人工菜单验收状态以测试报告为准，CMake构建通过不能替代该验收。

- [实现报告与文件清单](docs/MVP3_IMPLEMENTATION_REPORT.md)
- [逐项测试报告及未运行项](docs/MVP3_TEST_REPORT.md)
- [群组协议与推送](docs/PROTOCOL_V3.md)
- [数据库迁移、ER及恢复](docs/DB_SCHEMA_V3.md)
- [动态多态UML及事务时序](docs/UML_GROUP_POLICY.md)
- [基线审计](docs/MVP3_AUDIT.md)、[原始任务书](docs/MVP3_WORK_SPEC.md)
