# MVP4 implementation contract and plan

Goal: increment MVP3 into real TCP text messaging with commit-backed states, membership-window history, delivery ACK, reconnect recovery, and native independent-client evidence. User's attached MVP4 specification is authoritative scope; no media/read receipts/visual redesign.

Baseline: 3bc6b017cf3bfab8c6653815a925649d26a8b845; feature/mvp4-chat. Preserve original schemas and data. C++17 Qt6 Widgets/POSIX poll/SQLite DB-worker only, DXX class files. Native implementation proceeds under user's request; independent backend, Qt, and TCP acceptance scopes follow this frozen protocol.

## Frozen protocol

Use original envelope/framing, positive integer IDs and service_type strings QQ/WECHAT, original active connection account inferred server-side. All outputs omit account_pk/password. Pagination limit 1..50, default50. Every mutation UUID idempotent with persisted results; group history windows use message IDs (not second-resolution time).

- chat.private.open {service_type,target_display_id} -> {conversation_id,service_type,kind:PRIVATE,title,can_send,...}
- chat.private.send {service_type,target_display_id,content} -> message object below.
- chat.group.open {group_pk,subgroup_pk?} -> conversation summary. SUBGROUP only original QQ service and QQ current strategy, active parent+sub membership.
- chat.group.send {group_pk,subgroup_pk?,content} -> message object.
- chat.conversation.list {service_type,limit?,cursor? decimal offset string} -> {items:[summary],next_cursor,total}. Summary conversation_id,service_type,kind,title,group_pk/subgroup_pk if applicable,latest_message_id,can_send.
- chat.history.get {conversation_id,before_message_id?,after_message_id?,limit?} -> {items:[messages ascending ID],has_more,before_cursor,after_cursor}. Before and after mutually exclusive nonnegative integers; before exclusive descending SQL then return ascending, after exclusive ascending. Only authorized own private or membership-window group messages. Former members may read old authorized intervals but no future; nonmembers with no epoch rejected. New joins cannot read earlier messages or gaps after rejoin.
- chat.sync {service_type,after_message_id integer>=0,limit?} -> {items ascending visible messages,next_cursor integer,has_more}. Cursor progresses by returned message IDs, only visible membership windows. Client re-log loads recent conversations and syncs from known per-service cursor (in-memory, process restart can reload history), merge message IDs.
- chat.message.ack {message_id} -> {message_id,status:delivered}. Only actual designated recipient receipt row, own original service active, no fake ACK by sender/unrelated. ACK persist before sender event. No chat.message.read endpoint/read labels.
- message object: {message_id,conversation_id,service_type,sender_display_id,content,content_type:TEXT,created_at,status:stored,request_id}. max content4096 UTF8 bytes; reject empty/whitespace-only, NUL; invalid UTF8/JSON original protocol rejects. Sender fields provided by client must not impersonate: reject explicit sender_id/from_account/sender_account_pk fields (validation error) rather than silently claim forged sender.
- event chat.message.new {message fields} sent postcommit to authorized current recipients and sender's other active sessions (same sender session may receive too, dedup bymessageID/requestID). Recipient only current valid private friends or group/sub members; sender ACK not required. Event chat.message.delivered {message_id,conversation_id,service_type,recipient_display_id,status:delivered} after persisted actual ACK, idempotent/no false read.
- errors CHAT_NOT_FRIENDS,CHAT_NOT_MEMBER,CHAT_CONVERSATION_NOT_FOUND,CHAT_POLICY_DISABLED,CHAT_MESSAGE_TOO_LARGE,CHAT_INVALID_CONTENT,CHAT_SEND_NOT_ALLOWED,CHAT_CURSOR_INVALID; standard AUTH_REQUIRED/SERVICE_NOT_ACTIVE/SERVICE_NOT_OPEN/VALIDATION_ERROR/REQUEST_ID_CONFLICT/DATABASE_ERROR. Tests may accept exact documented equivalent where appropriate.

## Persistent authorization windows

Migration004 adds conversations,private_pairs,conversation_members epochs,messages,message_receipts. Existing/current group memberships seeded; future group/subgroup membership changes tracked by SQL triggers in same MVP3 transaction (do not patch every old service call). Epoch joined_after_message_id=current global MAX(message_pk), left_at_message_id=current global MAX on exit; authorized messages >joined_after and <=left_at if closed. Rejoin new row, unique one open epoch. Deleting parent membership cascades subgroup deletion and closes both epochs. Disband closes membership windows but preserves history; sending checks live status. New group/subgroup creates conversation and membership windows via triggers (original WECHAT subgroup never chat-enabled). Policy switches keep windows/old messages; QQ subgroup inaccessible until QQ restored. PRIVATE canonical same-service pair unique; deleted friends keep history but no sending.

## Ownership and execution tasks

1. Root: audit baseline/re-run 9 tests; back up original libraries read-only; branch; integrate CMake/dispatcher if backend requests; document constraints/ER/UML/README/reports, run final full suite and native process checks, commit and feature-only publication.
2. Backend implementer: migration004+embedded resource, DatabaseDXX version4/backup, MessageRepositoryDXX/MessageServiceDXX independent files, RequestDispatcher integration, network bounded buffers/tasks if needed, core message/domain classes if useful. Freeze protocol above, mutation authorize-before-replay checking identity/service but allow successful replay after permission changes. SQL failure no stored/event. First actual RED old server, then tests. Do not edit client/tests/CMake/docs beyond dedicated backend notes.
3. Qt implementer: client/** plus tests/mvp4_ui_main.cpp. ChatPageDXX/MessageModelDXX (independent class files); append message tab after old indices0..4, hide WEIBO; friend/group/subgroup entry signals. Async sender pending/stored/unknown/failed; retry exactUUID; preserve per-conversation drafts, merge event/historyID, ACK actual receipt only, unread indicator. Bounded nonblocking reconnect+reauth/activation; avoid repeated signal connections. Tests independent processes can launch IMClient --smoke-chat options in client/main.cpp if needed, no shared UI or DB. No client SQLite. Do not edit CMake/server/SQL or TCP acceptance.
4. TCP acceptance implementer: tests/mvp4_acceptance.py and optional own helper, real temporary server/db, T02–T20/T22 edges/SQL triggers/epochs/sync/ACK/partial framing, old server RED. No product edits. T01/T21 native independent-process evidence separate, don't pretend.
5. Root final gate: complete previous 9 suites plus new DB/TCP/Qt tests; actual failure logs and fixes, independent review where available; honest M4-T01–T22, Creator manual not run if absent.

## Review focus

- membership join/exit/rejoin within same second never exposes gaps or future messages (message-ID epoch tests)
- current auth vs successful retry after friend removal or group exit (one stored ID, no duplicate push)
- response lost aftercommit + duplicate event/history (one UI row/oneSQLrow)
- invalid cursors/cross-service/forged sender/ACK identities bounded and rejected
- slow receiver/fanout buffers and clean shutdown bounded; DBworker never waits onpeer

Each owner writes failing test/run evidence before implementation, then runs relevant green tests; integrate against frozen interfaces before final complete suite. Dedicated demo script only writes new mvp4-demo and refuses existing unless explicit --reset. No real user data cleared.
