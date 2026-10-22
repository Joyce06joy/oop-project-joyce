# MVP3 增量合同与实施顺序

以用户附件MVP3任务书为规格，基线53955f3，独立feature/mvp3-groups，不开发聊天、不重建已有表。复用MutationRepositoryDXX成功事务幂等、SocialRepositoryDXX认证账号及SQL分页、RequestPanelDXX异步错误/原ID重试与现有TCP事件队列。

顺序：实际基线回归/备份→纯C++策略及同对象动态派发单测→003迁移/仓储/服务→真实TCP全部M3用例→Qt增量页/原生联调→全回归/文档/独立审查/分支提交。Qt、策略域与网络验收按下述固定接口独立开发，主代理实现服务/迁移。

## 域接口（core/groups，独立.h/.cpp）
- GroupContextDXX public字段 active=true,member=false,owner=false,creator=false,targetMember=false；string role="",targetRole=""。默认构造。
- GroupPolicyDXX virtual destructor，纯虚 type()->string，以及bool canRequestJoin(ctx),canInvite(ctx),canApproveJoin(ctx),canKick(ctx),canManageAnnouncement(ctx),canCreateSubgroup(ctx),canManageAdmins(ctx),canManageSubgroup(ctx)。均const。
- QQGroupPolicyDXX/WeChatGroupPolicyDXX override。QQ申请非成员，审批/公告owner或ADMIN；踢：owner可踢非owner、ADMIN仅可踢MEMBER；子群创建owner，管理owner/creator且仍主群成员。微信成员可邀请，管理仅owner；不批准APPLY、不设管理员、不允许子群。
- GroupPolicyFactoryDXX::create(string)->unique_ptr<GroupPolicyDXX>。
- GroupMemberDXX(int64 account,string role,string joinedAt)，getters accountId(),storedRole(),joinedAt()。
- SubgroupDXX(int64 id,int64 parent,int64 creator,string name,vector<int64> members)，getters id(),parentId(),creatorId(),name(),members()。
- GroupDXX(int64 id,string service,int64 owner,string policy,int64 version)；getters id/service/ownerId/policyVersion（方法名id(),service(),ownerId(),policyVersion()），policy()->const GroupPolicyDXX&，setActive(bool)，setMembers(vector<GroupMemberDXX>)，setSubgroups(vector<SubgroupDXX>)，members()/subgroups()const引用，context(actor,target=0,creator=0)const，switchPolicy(string,int64)。组合unique_ptr，不改service或成员集合。服务端缓存同一对象，commit后再switchPolicy；加载/重启从DB工厂重建。

## 协议（保留M1/M2 envelope/64KiB）
group_pk/subgroup_pk/request_pk为JSON正整数（不使用裸展示群号跨平台定位）。所有群请求必须原服务账号已开通且连接已激活，不信任actor/role字段。

- group.create {service_type,name} -> {group_pk,service_type,group_number,name,policy_type,policy_version}；初始policy=原服务，QQ/WECHAT分开编号空间，从1001起。正常启动不预置；demo显式各建1001–1006。
- group.list {service_type,query?:最多80字节,mine_only?:bool,limit?,cursor?} -> {items:[群摘要],total,next_cursor}；匹配群号精确/名称文字，ACTIVE，原分页1..20、cursor"0"起。摘要有group_pk,service_type,group_number,name,policy_type,policy_version,member_count,is_member,stored_role/effective_role。
- group.detail {group_pk,limit?,cursor?} -> {group:摘要+owner_display_id/announcement/status, permissions:{can_apply,can_invite,can_review,can_manage_announcement,can_rename,can_set_admin,can_create_subgroup,can_switch_policy,can_disband,can_leave},members:[{display_id,nickname,stored_role,effective_role,can_kick,can_set_admin}],total,next_cursor,my_request:null|{request_pk,kind,status}}。非成员仅公开摘要/公告、自己申请及can_apply，其余成员列表为空。
- group.join.apply {group_pk}；group.invite {group_pk,target_display_id} -> {request_pk,status:"PENDING"}。APPLY目标即当前申请人，INVITE由当前成员发起、目标自行决定。
- group.request.list {service_type,group_pk?:int,limit?,cursor?} -> {items:[{request_pk,group_pk,group_number,group_name,kind,status,initiator_display_id,target_display_id,created_at,direction:"REVIEW"|"INCOMING"|"OUTGOING",can_respond:bool}],total,next_cursor}。仅PENDING；QQ审批人见APPLY；INVITE仅目标/发起人，未激活不可读。可不传group查个人所有相关待决。
- group.request.respond {request_pk,decision:"ACCEPT"|"REJECT"} -> {request_pk,status}。对向申请审批按当前虚策略；INVITE仅受邀人。
- group.admin.set {group_pk,target_display_id,enabled:bool} -> {updated:true}；群主不能设自己，QQ当前策略且目标MEMBER/ADMIN，OWNER不变。
- group.member.kick {group_pk,target_display_id} -> {removed:true}；group.member.leave {group_pk} -> {left:true}。
- group.announcement.update {group_pk,announcement:最多1000字节可空} -> {updated:true}；group.name.update {group_pk,name:1..96字节} -> {updated:true}，改名仅owner。
- group.disband {group_pk} -> {disbanded:true}，软删除、子群同步失效。
- group.subgroup.create {group_pk,name,member_display_ids?:array最多100} -> {subgroup_pk,parent_group_pk,name}；仅群主，自动含创建者，必须主群成员。
- group.subgroup.list {group_pk,subgroup_pk?:int,limit?,cursor?} -> {items,total,next_cursor,disabled:bool}；未传subgroup_pk时items为子群元数据（subgroup_pk,parent_group_pk,name,creator_display_id,member_count,can_manage）；传选中subgroup_pk时items为分页成员{display_id,nickname}并有subgroup元数据。微信策略只返回空items/disabled:true，不泄漏内容。
- group.subgroup.members.update {subgroup_pk,add_display_ids?:array,remove_display_ids?:array} -> {updated:true}，creator/owner且仍父群成员，QQ策略；成员退出父群自动级联清理。
- group.policy.switch {group_pk,target_policy,expected_policy_version:int>=0} -> {group_pk,policy_type,policy_version,cancelled_requests:int}。仅owner；实际切换version+1，取消全部PENDING为CANCELLED_POLICY_CHANGED，不复活。相同policy不增版本/无事件。任何其他群mutation可选expected_policy_version，提供时校验；后续总按最新策略授权，不用UI布尔。

所有mutation复用原全局UUID幂等，先登录/原服务激活再重放（成功leave/disband也可同ID重放）；新请求按当前成员/规则检查。冲突REQUEST_ID_CONFLICT；群不存在GROUP_NOT_FOUND，非成员GROUP_NOT_MEMBER，越权GROUP_PERMISSION_DENIED，目标异常GROUP_INVALID_TARGET，重复成员GROUP_ALREADY_MEMBER，待决GROUP_REQUEST_PENDING，终态GROUP_REQUEST_ALREADY_RESOLVED，策略禁用GROUP_POLICY_DISABLED，过期版本GROUP_VERSION_CONFLICT，已解散GROUP_DISBANDED，SQL失败DATABASE_ERROR。

事件继续kind:event/event_id/type/data，类型group.request.new/group.members.changed/group.policy.changed/group.subgroup.changed/group.disbanded；data仅group_pk/service_type/change或request_pk/policy_version等最小刷新元数据。给有权的相关用户已激活会话：申请给申请人+当前审批者，邀请给目标+发起者，关系变化给变更前后成员；后续不再通知退出者。切换取消请求时通知其相关人，不泄漏其他群成员资料。离线查询恢复。

## DB与一致性
003只建groups/group_members/group_requests/subgroups/subgroup_members/group_counters；原表保留。groups的owner通过可延迟复合FK指向成员，创建同事务插群+OWNER，commit检查不留无主群；OWNER唯一且owner不可转让。subgroup_members存parent_group_pk，与parent成员复合FK级联，确保子群始终为子集。微信策略停用子群是查询/授权层禁用，不删行。解散软删group/subgroups；不删用户或好友。现有v1/v2升级前备份，防覆盖/校验。
