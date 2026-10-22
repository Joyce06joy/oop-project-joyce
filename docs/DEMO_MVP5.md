# 单机三客户端演示

1. 在Qt Creator打开根CMakeLists，使用Qt6 Desktop/macOS Kit构建IMServer和IMClient。终端方法见README，独立build/mvp5。
2. 执行python3 demo/seed_mvp5.py：只创建data/mvp5-demo.sqlite3；存在时拒绝覆盖。需要重置时先停止使用该库的Server，再明确执行--reset，不操作其他库。
3. 启动build/mvp5/IMServer --db ./data/mvp5-demo.sqlite3 --port 8895。演示端口选8895避免干扰用户已有8888服务。
4. 三个终端分别启动build/mvp5/IMClient.app/Contents/MacOS/IMClient --port 8895，或使用已部署包内同一可执行文件。每个进程独立登录，不能以单窗口切换账号冒充三客户端。

专用测试账号Alice/Bob/Carol，演示密码Mvp5-Demo-2026!，不是个人/生产凭据。QQ10001/10002/10003，微信WX8001/WX8002/WX8003。Alice与Bob、Alice与Carol在QQ和微信均为好友；QQ主群1、微信主群2；QQ子群1仅Alice/Bob，Bob是QQ管理员。种子已通过TCP保存9条示例消息。

建议窗口摆放：Alice左、Bob中、Carol右，各可900×620，需要时最小化其他窗口；Retina绘制以实际Qt截图为证。激活QQ后好友选择发消息，Alice→Bob，Bob看气泡/已保存/送达。群组选择QQ主群进入群聊，三人接收；子群只有Alice/Bob。切微信空间再验证单聊及微信群。

Alice群主在群详情双确认切微信管理规则，原QQ成员不变、Bob旧ADMIN管理特权暂停、子群停用；切QQ恢复。不是把QQ账号转换为微信账号。Carol的普通成员权限不能用本地按钮绕过。

关闭Bob后Alice继续发消息；Bob重开登录/激活，最近会话打开，历史/离线内容按分页恢复。停止并重启专用Server，可观察连接横幅与自动恢复；不要停止用户其他Server。超时/断线unknown选择原消息重试，不能另造UUID重复发送。未知操作正文在进程内保留，完全退出后先查历史。

UI01–28的真实自动化、原生三进程和打包结果见MVP5_TEST_REPORT.md。自动化截图不是人工录像；Qt Creator完整菜单流程未执行的部分保留Not Run。Finder启动默认8888；如使用8895，终端向包内可执行文件传--port8895。当前8888已有用户服务时不抢占、不删除.lock、不清库。

成功证据清单：小窗/常规尺寸气泡、好友与群管理、三个独立PID/真实messageID、非子群成员未收到、Server重启后的消息恢复、打包依赖/签名/资源核验及Finder启动。普通客户端日志不输出密码或token；已提交测试日志隐藏机器路径及测试消息正文。
