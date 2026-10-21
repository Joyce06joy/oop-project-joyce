# T08 原生独立客户端实测记录

日期：2026-10-21，Asia/Shanghai。通过 Mac 原生 UI 自动化直接操作，非静态截图或伪造网络结果。

1. 专用演示服务器以 `IMServer --db ./data/mvp1-demo.sqlite3` 启动，监听 127.0.0.1:8888。
2. 为让 Mac 的应用自动化明确区分三个进程，把同一 `IMClient.app` 复制为 build/mvp1 下的 AliceClient.app、BobClient.app、CarolClient.app；只修改各副本 Info.plist 的 BundleIdentifier/BundleName，C++ 可执行文件相同。副本仅用于验收，不是三个不同业务版本。
3. 分别打开三个应用，输入 Alice、Bob、Carol 与演示密码并点击登录。三者均真实连接同一个服务器。
4. Alice 点击 QQ，Bob 点击 WECHAT，Carol 点击 WECHAT；再次读取三个独立窗口的可访问性树确认同时保留登录及激活状态。

实际观测：

| 独立进程 | 当前资料 | 服务号码 | 激活结果 |
|---|---|---|---|
| AliceClient | Alice，用户 ID 1，已连接 | QQ 10001 | QQ 已激活 |
| BobClient | Bob，用户 ID 2，已连接 | QQ 10002，WECHAT WX8001，WEIBO 10002 | WECHAT 已激活 |
| CarolClient | Carol，用户 ID 3，已连接 | WECHAT WX8002 | WECHAT 已激活 |

Bob 原生窗口截图已通过 UI 工具实际查看：标题、连接状态、用户 ID、三个服务号码、激活说明及退出按钮布局可见，没有好友/群/聊天入口。Alice、Carol 通过实际可访问性树与点击结果验证。Qt UI 自动测试另用 10 ms 定时器验证主循环响应，并覆盖注册、错误密码保留输入、断线、重连和 15 秒无响应。

结果：**Pass**。这份记录是对本轮真实 UI 操作的观察摘要；机器可复跑的 Qt UI 证据另见 ctest-run.txt 与 native-ui-run.txt。未声称完整人工穷举所有控件布局或所有 macOS 版本。
