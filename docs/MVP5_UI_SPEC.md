# MVP5 实际界面规范

基于现有LoginWindowDXX这一QMainWindow，保留注册辅助窗口，不增加第二个已登录主窗口。默认1100×720，最小900×620；主窗口顶部品牌/连接状态/手动重试，用户条与退出登录，横向服务激活按钮；左侧QQ/WECHAT/WEIBO为当前空间导航，激活与切换不是同一动作。

六业务页索引保持0好友/1申请/2推荐/3账号/4群组/5消息。微博隐藏群组/消息，QQ/微信支持原有功能；未开通与已开通未激活显示不同提示，可在账号页开通、再确认服务激活。当前空间/展示号码在serviceGate和聊天标题显示，QQ/微博同号不代表共享数据。切换空间不重连用户Socket。

主题集中client/resources/theme.qss，经ui.qrc内嵌、ThemeDXX一次初始化，资源不依赖开发目录。系统字体13pt，标题22px、章节16px、辅助12px；间距8/12/16/24；画布#F3F6FA、白色表面、侧栏#E9EFF7、文字#172B46、辅助#52657C、强调#2264B0、危险#B53738；控件圆角6，消息10。保留原生键盘/焦点、标准确认对话框，危险操作不绕过服务器。

登录和注册有显示/隐藏密码，标准Tab顺序、错误字段焦点，保留正确输入；注册成功的ID可选择/复制，只在服务器确认保存后展示。辅助窗口与主窗口都有父子ownership，非后台悬空回调。

管理页复用RequestPanelDXX，body放入可滚动容器，状态和重试保留；小屏可以滚动抵达底部操作，不强迫窗口扩张。搜索/好友、申请、推荐、账号、群/子群给出空状态或下一步解释；推荐空集合仍显示真实“共0项”。QQ规则显示管理员操作，微信规则隐藏无效管理员入口；角色、原属服务和当前策略仍由服务器数据展示。

异步状态：Idle/Loading/Success/Error/Unknown与全局Offline/Reconnecting分别呈现；写入中的按钮防重复；Unknown保留原UUID/内容供安全重试。错误不清整张表单，失败不假更新，权限仍由Server判定。重新登录其他账号同时清除好友搜索模式、旧查询结果、聊天模型/草稿和原用户请求上下文。

聊天：QSplitter可调会话/消息分栏；MessageBubbleDelegateDXX使用QTextLayout纯文本绘制，绝不解析用户HTML；本人右侧蓝色、对方左侧浅色，时间/账号与状态可读。长中文、emoji和无空格内容按实时viewport宽度测量/换行；焦点与选择有可见边框，无横向消息溢出。模型保留UserRole=request UUID、UserRole+1=messageID，附加Content/Sender/Timestamp/State/OwnMessage/Delivery角色。AccessibleTextRole提供完整文本。

pending=发送中、stored=已保存、unknown=待确认、failed=未发送；送达x/y只来自真实持久ACK，不出现已读。会话区分单聊/主群/子群，显示当前服务与本人账号，真实权限不可发送时给解释；最近会话/未读、空消息、分页最早状态可见。输入只接受纯文本，原发送/ACK队列/历史/同步/重连算法不重写。

Mac应用图标是本项目原创矢量聊天符号，SVG/PNG及7尺寸ICNS可通过scripts/generate_app_icon.py复现；CFBundle版本0.5.0、标识edu.joyce.implatform.client。包路径继续IMClient.app，保证测试与启动兼容。包装主题/图标均相对资源加载。
