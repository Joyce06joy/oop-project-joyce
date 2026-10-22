# 聊天协议 v4

沿用v1–v3：4字节大端长度 + UTF8 JSON、64KiB上限，同一TCP连接；kind=request/response/event，request_id关联响应，event_id通知去重。system.ping的protocol_version仍表示兼容envelope版本；v4表示新增业务集合，不改变旧请求。

所有聊天请求先认证并检查原属QQ/WECHAT服务已开通且本会话激活。微博无聊天。sender由连接身份推导，客户端不能指定他人身份；文本最多4096 UTF8字节，拒绝空白正文和NUL。只支持TEXT，不实现图片/语音/文件或已读。

| 请求 | data | 成功结果 |
|---|---|---|
| chat.private.open | service_type,target_display_id | conversation摘要 |
| chat.private.send | service_type,target_display_id,content | 已COMMIT的message |
| chat.group.open | group_pk；subgroup_pk可选 | conversation摘要 |
| chat.group.send | group_pk；subgroup_pk可选；content | 已COMMIT的message |
| chat.conversation.list | service_type；limit,cursor可选 | items,total,next_cursor |
| chat.history.get | conversation_id；before_message_id或after_message_id；limit可选 | items,has_more,before_cursor,after_cursor |
| chat.sync | service_type,after_message_id；limit可选 | items,next_cursor,has_more |
| chat.message.ack | message_id | message_id,status=delivered |

ID是JSON正整数；after_message_id允许0。limit 1–50，默认50。history的before/after互斥且排除边界，items始终按message_id升序；无游标先查最近页，往前分页由before_cursor驱动。conversation.list游标是非负十进制offset字符串；sync游标是全局message ID整数，只返回当前账号允许看到的消息，不能用offset冒充ID。

会话摘要含conversation_id/service_type/kind/title/can_send/latest_message_id和适用group_pk/subgroup_pk；PRIVATE按同服务canonical账号对唯一，GROUP按group_pk唯一，SUBGROUP按subgroup_pk唯一。展示ID只是服务内标识，不作全局主键。

message对象含message_id/conversation_id/service_type/sender_display_id/content/content_type=TEXT/created_at/request_id/status=stored，另含recipient_count/delivered_count真实持久回执计数。时间是服务器UTC，顺序以message_id稳定排序。响应stored只表示服务器SQLite COMMIT完成，不表示对方已接收或阅读。

```json
{"kind":"request","type":"chat.private.send","request_id":"550e8400-e29b-41d4-a716-446655440000","data":{"service_type":"QQ","target_display_id":"10002","content":"你好 Bob 👋"}}
```

## 权限与成员窗口

私聊打开/新发均检查当前同服务好友关系；删除好友后保留既有双方历史，但不可再发送。主群发送要求当前有效成员且未解散。子群聊天要求原属QQ、父群当前QQ策略、有效父/子群成员；原属微信的MVP3子群即使采用QQ管理策略也不提供聊天。切微信策略停用QQ子群，旧消息/成员不删除，切回恢复。

群历史按conversation_members多个epoch授权：消息ID严格大于加入边界，且不大于已关闭的退出边界。边界在同一成员变更事务内基于全局已保存最大消息ID记录，因此同秒离开/重入也不会泄漏空档消息。退群者能查询过去有权限的区间，不可读未来；从未加入者拒绝；重入创建新epoch而非覆盖原窗口。历史可读与现在可发送分开判断。

## 保存、重试与事件

消息保存、初始recipient回执和幂等成功结果同一事务。异常ROLLBACK，不能返回stored或发布new事件。重复相同主体/UUID/操作/内容返回原message_id和完整成功结果，不重新保存、不再发event；换内容/主体冲突。重放仍检查当前认证及原服务激活；已提交但响应丢失时可安全重试原UUID。

- chat.message.new：COMMIT后向相关当前成员/私聊参与者的已激活在线会话推送message。发送者其他会话也可合并同一消息；UI按message_id与原request_id合并本地草稿和响应，不重复展示。
- chat.message.delivered：接收者本人明确chat.message.ack，服务器核实其指定recipient回执并持久化后，向发送方推送message_id/conversation_id/service_type/recipient_display_id/status=delivered。sender自己、无关账号及无回执者不可代ACK。重复ACK不重复宣称新送达。
- 没有chat.message.read、没有已读展示。群消息的单个recipient ACK只表示此人送达，不能推断全群送达；事件及历史带recipient_count/delivered_count，重启可重新查询已持久化的计数。

客户端状态pending→stored；超时/断线进入unknown，原UUID重试；明确拒绝为failed并保留输入。离线消息仍保存，重连认证/激活后查询会话与增量sync；event不是唯一事实来源。历史和event可能重叠，客户端必须按message_id合并排序。切页面保留草稿，退出应用的未确认发送不保证持久保存。

## 错误与边界

CHAT_NOT_FRIENDS、CHAT_NOT_MEMBER、CHAT_CONVERSATION_NOT_FOUND、CHAT_POLICY_DISABLED、CHAT_MESSAGE_TOO_LARGE、CHAT_INVALID_CONTENT、CHAT_SEND_NOT_ALLOWED、CHAT_CURSOR_INVALID；沿用AUTH_REQUIRED、SERVICE_NOT_OPEN、SERVICE_NOT_ACTIVE、VALIDATION_ERROR、REQUEST_ID_CONFLICT、DATABASE_ERROR。无效UTF8/JSON/超帧按原协议断开连接，不造成不完整消息。

服务端网络线程维护partial read/write buffer，DBworker仅投递已提交响应/event。慢客户端必须受有限发送队列约束；具体限额、关闭策略及实测证据见实现与测试报告。没有第二条聊天专用Socket或客户端数据库直读路径。
