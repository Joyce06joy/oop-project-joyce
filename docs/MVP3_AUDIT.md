# MVP3 实际基线审计

基线commit：53955f3ad9b86ad33f8f0f76b5c3161b49b15fca，原分支feature/mvp2-friends，开始时工作区干净；GitHub API核对远端同SHA。本轮分支feature/mvp3-groups，不合并main。

源码真实结构：client/network/TcpClientDXX异步请求与event去重，client/ui/Login/Register/ServiceSelect+Workspace和四个MVP2页面；server/network/TcpServerDXX为POSIX poll；RequestDispatcherDXX在唯一DB工作线程拥有会话、应用服务和MutationRepositoryDXX。DatabaseDXX原支持v1→v2备份迁移，database/schema.sql仍为v1、002_friends.sql为增量。根MainWindowDXX是保留空模板，实际业务主窗口为LoginWindowDXX内嵌ServiceSelect/Workspace，不新增第二个同名窗口。

实际运行修改前build/mvp2全部5组CTest：5/5通过，56.55秒；MVP2报告仍明确T18完整Creator菜单/独立进程人工场景未完成，本轮不把它当作已通过。已完成的MVP2自动功能足以作为群系统基线，没有发现阻塞MVP3的旧业务失败。

已安装工具：Apple Silicon macOS，Qt6.12.0 Widgets/Network/Test、Clang21、Qt自带CMake/Ninja，json-c0.19、OpenSSL3.6.5、SDK SQLite3.51.0。构建使用根CMake、独立build/mvp3目录；原build/mvp2保留用于新增请求预期失败对照。命令详见README。

原协议：4字节大端长度+UTF8 JSON、64KiB，kind=request/response/event；UUID相关响应，成功mutation结果与SQL同提交，重放不重复event。连接序号映射user_id及active_services，不能拿payload角色或session_id冒充；已激活服务由连接会话决定。网络线程和DB线程分离，Qt不读SQL；本轮继续复用这些实际接口。

开发库实际版本：mvp1-demo.sqlite3=v1(3用户)、manual.sqlite3=v1(0用户)、mvp2-demo.sqlite3=v2(5用户)。实施前通过SQLite只读backup到data/backups/mvp3-preflight并quick_check，原库不升级、不清空、不停止用户进程。新服务器真正打开已有v1/v2库时会再次自动生成版本对应的迁移前快照；本轮验收及演示都用独立库。

计划文件：core/groups纯C++策略组合/成员/子群；GroupServiceDXX与GroupRepositoryDXX；003_groups迁移与内嵌资源；RequestDispatcher扩展群请求及原事件队列；GroupPage/PolicyDialog/SubgroupPage与Workspace增量；纯策略、DDL、真实TCP、Qt测试；协议/ER/UML/演示/报告。旧用户、服务、好友、绑定、幂等表不重建。详细接口冻结与授权/分页/事件语义见plans/MVP3_CONTRACT.md。
