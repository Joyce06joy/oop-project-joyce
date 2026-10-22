# MVP3_IMPLEMENTATION_REPORT

日期：2026-10-22（Asia/Shanghai）。范围：群组管理、QQ/微信规则、子群与运行时动态多态，无消息单聊/群聊。

## 基线与实际交付

基于MVP2 commit `53955f3ad9b86ad33f8f0f76b5c3161b49b15fca`，分支 `feature/mvp3-groups`。开始时工作区干净，远端MVP2同SHA。只读审计及开发库快照见 [MVP3_AUDIT.md](MVP3_AUDIT.md)；详细冻结接口见 [MVP3_CONTRACT.md](plans/MVP3_CONTRACT.md)。本轮未合并main、未强推、未清空已有用户库。最终提交情况见本文末节和实际Git记录。

功能通过 Qt→异步QTcpSocket→POSIX TCP→唯一DB worker→Service/虚策略→SQLite 完整链路实现。保留MVP1注册/登录/自动号码和MVP2好友/绑定/推荐；原四页索引不变，群组追加为第五页，微博隐藏。新增类均DXX结尾、独立h/cpp。纯业务域无Qt/SQL依赖。

| 需求 | 实现与关键约束 |
|---|---|
| 建群/查群/资料 | QQ/WECHAT原服务成员，独立群号事务分配，搜索/分页/成员/公告/改群名 |
| QQ申请/审批 | PENDING唯一；群主/管理员接受拒绝，同事务加入/终态 |
| 微信邀请 | 当前成员可邀，目标本人确认；无直接申请，普通成员无管理权 |
| 管理员/踢人 | 群主设撤非自身合法成员；管理员仅踢普通成员，群主不可被踢 |
| 退出/解散 | 群主禁直接退出；成员退出级联子群；群/子群软删除、所有后续操作拒绝 |
| QQ临时子群 | 仅群主创建，自动包含创建者；独立成员集合，父群成员子集复合FK |
| 策略切换 | 同一GroupDXX替换抽象策略，原服务/成员/角色/子群不丢；微信规则停用ADMIN与子群，切回恢复 |
| 版本与失败 | expected_policy_version，单worker+BEGIN IMMEDIATE；SQL提交后更新策略和发event，失败回滚 |
| 推送/恢复 | 真实Socket，相关在线且已激活服务会话；离线重新查询权威状态；无重放event |
| 迁移 | 003增量，旧库先验证备份，不重建旧表，foreign_key_check |
| Qt交互 | 列表/详情/申请邀请/成员/子群、loading/错误、输入保留、同UUID重试、切换双确认 |

## 文件修改清单

- `core/groups/`：GroupContextDXX、GroupPolicyDXX、QQGroupPolicyDXX、WeChatGroupPolicyDXX、GroupPolicyFactoryDXX、GroupMemberDXX、SubgroupDXX、GroupDXX，各独立h/cpp，共16文件。
- `server/application/GroupServiceDXX.{h,cpp}`：授权、虚策略调用、请求状态机、成员/子群操作、后提交策略切换与推送目标。
- `server/persistence/GroupRepositoryDXX.{h,cpp}`：群查询、成员/子群重建、摘要与分页；DatabaseDXX新增v3备份迁移。
- RequestDispatcherDXX扩展17种群请求，复用MutationRepository事务和事件队列；原网络帧/认证/好友业务未重写。
- `database/migrations/003_groups.sql`、`MigrationV3DXX.h.in`；根CMake增加域库、源码、迁移嵌入和4组测试。
- `client/ui/GroupPageDXX`、GroupPolicyDialogDXX、SubgroupPageDXX六文件；Workspace追加群组入口，复用RequestPanel失败恢复。
- `tests/group_policy_main.cpp`、`group_database.py`、`mvp3_acceptance.py`、`mvp3_ui_main.cpp`；旧acceptance和mvp2_acceptance只将当前schema断言2升级3，旧数据/备份断言保留。
- `demo/seed_mvp3.py`：真实API生成独立库，显式--reset，Alice/Bob/Carol/David与两平台1001–1006。
- README、审计、合同、任务书存档、DB_SCHEMA_V3、PROTOCOL_V3、UML_GROUP_POLICY、本报告、MVP3_TEST_REPORT及原始日志/截图。

## 构建与验收

Mac Apple Silicon，Clang21、Qt6.12.0、C++17、CMake/Ninja、json-c0.19、OpenSSL3.6.5、SQLite SDK3.51.0。`build/mvp3`实际生成IMServer、IMClient及测试目标。详见 [MVP3_TEST_REPORT.md](MVP3_TEST_REPORT.md)，其中明确区分Pass/Fail/Not Run。最终CTest9/9组通过、0失败、85.08秒；M3-T01–T21全部通过，并发T17/T20另各重复5次通过。原生cocoa Qt三窗口真实TCP联调已通过，并保存截图；没有将Qt Creator菜单操作视为已执行。

审查曾发现解散只通知现成员，未通知群外待决参与者。新增T10复现超时，再扩充postcommit通知至相关申请者/受邀者，验证修复。Qt测试初次点击未等待异步写响应导致按钮暂禁用；测试改为等待按钮可用后操作，未削弱断言。SQL触发器注入验证Qt不误报成功、保留原输入、移除故障后相同UUID只建一次群。

## 设计适配与限制

1. 建议性GroupRequestDXX未单独抽取；请求状态机沿用MVP2的JSON仓储/服务与SQL约束模式，避免重复层。权限核心均为真实虚方法，域测试证明同一对象行为切换。
2. 原属微信群切QQ规则可用申请/管理员/子群，账号始终限定WECHAT；service_type不可更改。规则切换取消所有待决请求，不自动复活。
3. 解散保留PENDING历史审计但从有效请求列表移除且不可处理；通过group.disbanded让待决参与者刷新。未添加任务书之外的终态枚举。
4. 群组页基于现有Widgets风格、滚动布局；支持缩放和原生Retina截图。M3-T22的Qt Creator菜单构建与三个独立IMClient进程人工完整场景未运行，报告列Not Run；已有三窗口自动原生联调不能替代该项。
5. 事件不持久化，离线靠查询恢复；待确认mutation不跨客户端退出存储。服务器串行DB worker含密码KDF，面向单机教学规模。不存在互联网部署、聊天、媒体或复杂群层级。
6. 停用子群期间成员退出会正常级联清理；恢复QQ策略不复活已离群成员。群主不可转让，本轮仅可解散。

## MVP4可复用接口

- `group.detail`：有效群状态/原服务/策略版本/成员及权限；公开详情不能作为发送消息资格，必须服务端重新查询成员。
- `GroupRepositoryDXX::group/members/subgroups`、`GroupDXX::context`、虚策略提供原服务、有效成员、角色及当前策略。
- `group.subgroup.list`：有效父群成员且QQ策略，停用时不返回敏感成员；将来发送子群消息也须检查父/子群成员和当前策略。
- `group.members.changed/policy.changed/disbanded/subgroup.changed`：客户端刷新入口，不能替代权威查询或持久消息。
- `MutationRepositoryDXX`：提交结果幂等与提交后event；继续复用连接会话鉴权、原服务激活、DB worker事务。
- 目前没有conversation/message表、聊天协议或聊天UI；MVP4需要独立设计持久消息与消息权限虚方法，不能把canInvite当作发言授权。

## 提交与交付

本轮代码在 `feature/mvp3-groups`；未改写MVP2历史或main。源码及文档与测试证据一起提交。远端可用性以实际发布核验为准；不将本机commit混称为已push。README包含Qt Creator选择IMServer/IMClient、命令行构建、三客户端、演示密码及恢复步骤。
