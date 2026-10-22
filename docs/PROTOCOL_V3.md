# 群组协议 v3

扩展MVP1/2 envelope；4字节大端UTF-8 JSON长度，最大64KiB，继续使用同一异步连接和原认证。`system.ping.protocol_version=1`表示兼容的帧/envelope版本；本文v3表示新增业务命令集，不要求修改旧客户端。

```json
{"kind":"request","request_id":"550e8400-e29b-41d4-a716-446655440000","type":"group.policy.switch","data":{"group_pk":1,"target_policy":"WECHAT","expected_policy_version":0}}
```

响应保持原 `kind:response, request_id, type, success, data/error`；event为 `kind:event,event_id,type,data`。以下标识必须是JSON正整数：group_pk、request_pk、subgroup_pk。group_number为展示字符串，按平台唯一，不能作为内部寻址键。target_display_id等展示号是字符串。

所有请求先验证连接身份及群原属服务已开通且已激活。WEIBO不支持群。群平台归属不因策略切换改变；任何actor/target使用该原服务账号。客户端的actor/role/permissions字段不参与授权。

| type | data必需/可选字段 | 成功data |
|---|---|---|
| group.create | service_type,name | 群摘要，创建者OWNER |
| group.list | service_type；query,mine_only,limit,cursor | items摘要,total,next_cursor |
| group.detail | group_pk；limit,cursor（成员页） | group,permissions,members,total,next_cursor,my_request |
| group.join.apply | group_pk | request_pk,status=PENDING |
| group.invite | group_pk,target_display_id | request_pk,status=PENDING |
| group.request.list | service_type；group_pk,limit,cursor | items,total,next_cursor |
| group.request.respond | request_pk,decision=ACCEPT/REJECT | request_pk,status=ACCEPTED/REJECTED |
| group.admin.set | group_pk,target_display_id,enabled（布尔） | updated=true |
| group.member.kick | group_pk,target_display_id | removed=true |
| group.member.leave | group_pk | left=true |
| group.announcement.update | group_pk,announcement | updated=true |
| group.name.update | group_pk,name | updated=true |
| group.disband | group_pk | disbanded=true |
| group.subgroup.create | group_pk,name；member_display_ids | subgroup_pk,parent_group_pk,name |
| group.subgroup.list | group_pk；subgroup_pk,limit,cursor | items,total,next_cursor,disabled；指定子群时含subgroup |
| group.subgroup.members.update | subgroup_pk；add_display_ids,remove_display_ids | updated=true |
| group.policy.switch | group_pk,target_policy=QQ/WECHAT,expected_policy_version | group_pk,policy_type,policy_version,cancelled_requests |

name为1–96 UTF-8字节且不可全空白；announcement最多1000字节可空。成员数组最多100项并去重；同一成员不能同时add和remove，所有目标必须是当前父群成员。query最多80字节，绑定参数并转义LIKE通配符。分页复用v2：cursor为非负十进制偏移字符串，limit 1–20（默认20）；集合变化时重新查首页。

群摘要含 `group_pk,service_type,group_number,name,policy_type,policy_version,status,member_count,is_member,stored_role,effective_role`。详情另含owner_display_id、announcement；members只对当前群成员返回，条目含display_id/nickname/stored_role/effective_role/can_kick/can_set_admin，不暴露account_pk或用户凭据。非成员可读取群公开元信息及人数，不能读取成员/子群敏感内容。

permissions字段：can_apply、can_invite、can_review、can_manage_announcement、can_rename、can_set_admin、can_create_subgroup、can_switch_policy、can_disband、can_leave。它们是UI显示提示，服务器每次操作重新鉴权。my_request为自己的最新请求request_pk/kind/status或null。

request.list只返回有效群PENDING：本人发起/目标的请求，或当前QQ审核者可审批的APPLY。条目含request_pk/group_pk/group_number/group_name/kind/status/initiator_display_id/target_display_id/created_at/direction（REVIEW/INCOMING/OUTGOING）/can_respond。微信邀请只能目标接受/拒绝；若邀请者已退群，接受拒绝，但目标可拒绝旧记录后由当前成员重新邀请。

subgroup.list不指定subgroup_pk返回子群摘要和can_manage；指定时返回子群成员display_id/nickname及subgroup元数据。微信策略返回disabled=true、items=[]，不泄漏旧子群名称/成员；切回QQ恢复。解散群拒绝后续读取和变更，列表不再包含。

## 版本、幂等与事务

切换必须提供expected_policy_version；旧版本返回GROUP_VERSION_CONFLICT。其他变更可选同字段（Qt详情操作发送它），若提供同样校验。相同目标策略为no-op，不递增版本、不发event。实际切换递增版本并将所有PENDING标记CANCELLED_POLICY_CHANGED；切回不复活。

所有变更复用mutation_requests，request_id全局唯一，指纹绑定当前user_id、type及规范化data；同ID同内容原样返回既有成功结果，不重复SQL或event；换用户/命令/内容报REQUEST_ID_CONFLICT。成功响应与业务同事务持久化，失败回滚且不缓存成功。重试前仍验证当前连接身份及原服务激活；已成功退出/解散可重放既有结果。客户端RequestPanel在未知结果时锁定原输入/UUID，重连同用户并激活原服务后重试；不跨应用退出保存待确认内容。

唯一DB worker串行执行，BEGIN IMMEDIATE内读取角色/策略，避免切换响应后旧权限继续执行；若竞态操作先于切换提交，可合法完成。COMMIT成功后才替换同一GroupDXX策略和发布事件。

## Socket事件

| type | 相关接收者 | 用途 |
|---|---|---|
| group.request.new | QQ申请者+当前审核者；微信邀请者+目标 | 待决列表刷新 |
| group.members.changed | 当次相关成员，含被移除者；审批时也通知请求参与者 | 成员/角色/公告/群名/请求状态刷新 |
| group.policy.changed | 当前成员+本次被取消请求的参与者 | 权限和停用子群刷新 |
| group.subgroup.changed | 当前父群成员 | 子群重新查询 |
| group.disbanded | 原成员+仍待决申请/邀请参与者 | 移除群/待决列表 |

data仅含group_pk、service_type、change，按场景附request_pk/status/policy_type/policy_version/subgroup_pk，不含完整成员或凭据。仅发送给相关用户且该原服务已激活的在线会话；同一用户多个会话均可接收。无关用户不收到。事件在COMMIT后发出，失败与幂等重放不发出。

members.changed的REQUEST_REJECTED表示请求视图变化，不能据此把目标加为成员。所有event都是查询提示；离线事件不存储，重连/页面刷新通过权威SQL查询恢复。Qt按event_id去重并合并刷新。

## 错误码

GROUP_NOT_FOUND、GROUP_NOT_MEMBER、GROUP_PERMISSION_DENIED、GROUP_ALREADY_MEMBER、GROUP_REQUEST_PENDING、GROUP_INVALID_TARGET、GROUP_POLICY_DISABLED、GROUP_VERSION_CONFLICT、GROUP_DISBANDED、GROUP_REQUEST_ALREADY_RESOLVED；继续使用AUTH_REQUIRED、SERVICE_NOT_OPEN、SERVICE_NOT_ACTIVE、VALIDATION_ERROR、PROTOCOL_ERROR、REQUEST_ID_CONFLICT、DATABASE_ERROR。错误不会发送成功event。不存在的请求/子群返回GROUP_INVALID_TARGET；已处理/取消的请求不能重新批准，权限不足时可先返回权限错误。
