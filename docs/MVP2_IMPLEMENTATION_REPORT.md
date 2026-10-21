# IMPlatform MVP2 实施报告

基线commit：e22f0fc9c40ebb98f4e75168de66b0ca36b0217d。开发分支：`feature/mvp2-friends`。用户本轮任务书已保留为[MVP2_WORK_SPEC.md](MVP2_WORK_SPEC.md)，增量合同与顺序见[计划](plans/MVP2_CONTRACT.md)，最初实际审计见[MVP2_AUDIT.md](MVP2_AUDIT.md)。最终Git提交/远端分支或PR以本次交付链接及Git历史为准；不合并默认main、不强推。

## 完成范围

在原MVP1注册/统一登录/服务激活上增量增加：补开服务与共享号码复用；本人QQ/微信显式一对一绑定/解绑；同服务账号搜索分页、好友申请/接受/拒绝、无向好友关系、删除、单向备注与资料；共同好友、同平台二度推荐、受显式绑定约束的跨平台迁移推荐与跨服务好友交集；同一Qt窗口中的三个服务空间与四个管理页面；真实Socket定向事件推送及离线重新查询恢复。

未开发群、子群、动态群策略、单聊、群聊、消息历史或账号注销。QQ/微博好友独立，不因同展示号共享关系；无昵称身份猜测或隐藏user_id映射绕过解绑。没有未实现模块的假按钮。

## 文件变更与可复用边界

完整清单见[MVP2_CHANGED_FILES.txt](MVP2_CHANGED_FILES.txt)。主要变化：

| 层 | 新增/增量文件及职责 |
|---|---|
| 构建 | CMakeLists.txt增加迁移内嵌、新服务/页面和两组测试，保留原目标/入口。 |
| 迁移 | database/migrations/002_friends.sql、MigrationDXX.h.in；原schema.sql不改，原四表不重建。 |
| 存储 | DatabaseDXX增加一致性检查、sqlite3_backup与事务迁移；MutationRepositoryDXX统一成功结果持久幂等与回滚；原StatementDXX参数绑定复用。 |
| 业务 | AccountServiceDXX补开/绑定，FriendServiceDXX申请状态机/关系，RecommendationServiceDXX交集与推荐，SocialRepositoryDXX统一账号授权/安全公开资料/SQL分页；BusinessErrorDXX携稳定错误码。 |
| 分派与网络 | 原RequestDispatcherDXX扩展白名单与认证路由，收集提交后通知；原TcpServerDXX复用结果队列发event，事件不扣请求计数；原POSIX/poll/单工作线程不变。 |
| 协议 | JsonDXX新增isInteger和规范键顺序canonicalDump供指纹使用；原64KiB帧/错误信封保持。 |
| Qt | 原TcpClientDXX增加eventReceived及有界去重；原ServiceSelectWindowDXX嵌WorkspaceDXX；LoginWindowDXX标题/尺寸适配。原注册/登录控件和激活逻辑保留。 |
| Qt页面 | RequestPanelDXX异步读/写加载与相同ID重试；FriendPageDXX搜索/列表/资料/备注/删除确认；FriendRequestPageDXX审核；RecommendationPageDXX四类查询/发申请；AccountPageDXX开通/绑定；WorkspaceDXX侧栏和页面联动。 |
| 测试演示 | tests/mvp2_acceptance.py、mvp2_helpers.py、mvp2_ui_main.cpp；原acceptance仅schema断言适配为2，protocol增加规范JSON测试。demo/seed_mvp2.py五用户真实TCP场景；旧seed只更新默认构建路径。 |
| 文档 | README、DB_SCHEMA_V2、PROTOCOL_V2、UML/ER/状态机、逐项测试、构建/测试/原生/演示输出。MVP1历史报告保留。 |

全部自定义C++类以DXX结尾、有独立同名h/cpp（新增及原业务共24类），纯Qt与json-c第三方前置声明不在此列。根旧MainWindowDXX模板完整保留，没有复制第二个同名类。UI组合复用一个TcpClient；业务权限位于服务端，不从UI直接SQL。

## 关键保证与实现选择

- **数据库保留**：实施前只读备份现有两个本地开发库；升级已有v1库时服务器再次自动、不覆盖地备份。v2迁移只建新表/索引/触发器，BEGIN/COMMIT或rollback。未知库/版本拒绝。实际迁移测试逐行比较原数据并验证旧登录/注册重放。
- **会话权限**：沿用连接→user_id/active_services，好友操作验证当前服务，跨推荐验证两侧；不能借payload身份或旧UUID绕过。补开不等于激活。
- **绑定映射**：同主体QQ/微信一对一；本人及候选都须有效绑定才允许涉及微信的映射，微信↔微博也通过QQ绑定证明；QQ↔微博用同主体共享号。解绑不删好友，但推荐可见性即时改变。
- **关系事务**：好友申请无序对PENDING部分唯一，接受同事务更新申请并插入一条a<b关系；终态不可逆。删除清双方备注，保留申请审计。单向备注不改昵称/他方备注。
- **幂等**：新增写操作的全局UUID、认证主体/type/规范data指纹和完整结果同事务保存。相同ID精确重放，不重复推送；不同载荷冲突。open成功services为原快照，客户端重查实时会话状态。
- **网络真实**：单工作线程拥有数据库/会话，提交后按用户和已激活服务定向生成event，通过原网络队列POSIX send。Qt同一QTcpSocket异步分派event，1024条ID去重，100ms刷新合并；断线后查询申请恢复，不承诺事件离线历史/ACK。
- **错误恢复**：客户端mutation不确定结果保留原ID/输入，重新登录激活后同请求重试；查询按族一个在途/一个最新后继，避免旧搜索覆盖新输入或绑定查询覆盖服务列表。拒绝时不先假装更新关系。

推荐目录中的FriendshipDXX/FriendRequestDXX是建议而非强制实体，本实现没有为SQL行重复建立空DTO；状态机和关系不变量集中于FriendServiceDXX及DDL约束，推荐SQL复用SocialRepositoryDXX。与实际代码一致的图见[MVP2_DIAGRAMS.md](MVP2_DIAGRAMS.md)。

## 实测与遗留

最终清洁构建通过，无编译警告；全部5组CTest通过，MVP1回归和M2-T01–T17均有真实证据。最终Qt完整流程另在Cocoa平台通过。独立静态审查未发现可确认P1/P2；该审查不是测试通过依据。

**M2-T18未完整通过**：同Kit命令行完整构建、三个Qt窗口真实网络会话、独立三个IMClient进程启动以及原生自动流程已验证；但Qt Creator桌面AX读取失败，额外独立进程人工交互演示受执行器生命周期/重启参数限制，不能声称完成该完整菜单场景。详情和原始输出见[MVP2_TEST_REPORT.md](MVP2_TEST_REPORT.md)。

遗留限制：仅本机loopback明文；串行KDF/DB工作线程可能排队，GUI异步保持响应；64连接、每连接8在途是教学边界；offset分页不冻结跨页并发快照；mutation审计未做过期清理；UI未确认操作仅内存，进程退出/换主体后不自动恢复；事件是可丢失刷新提示，事实通过查询恢复；没有签名分发包、压力/ASan/TSan或其他系统验收。没有已知未修的自动功能测试失败，以上限制不冒称已验证。

## 下一轮接口

MVP3应另行确认需求，本轮停止于MVP2。可复用：

- 认证连接及已激活服务上下文；后续业务禁止信任客户端actor身份。
- SocialRepositoryDXX::owned/target/profile/page以及同服务账号主键边界；好友判断和绑定可见性不要被群/聊天绕过。
- MutationRepositoryDXX::execute作为原子业务写入与幂等边界；后续副作用也在成功提交后发布通知。
- RequestDispatcherDXX::takeEvents + TcpClientDXX::eventReceived用于轻通知；若加入聊天，持久消息/历史/ACK须另设计，不能把当前event当可靠离线消息。
- 数据库user_version=2；新功能用独立003迁移，保留账号/好友/备注/绑定数据；申请业务ID与网络UUID保持区分。
- WorkspaceDXX和RequestPanelDXX可接新页面，但没有预先创建群/聊天入口或空壳策略。
