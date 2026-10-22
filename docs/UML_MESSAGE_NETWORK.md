# 消息、网络与线程模型

```mermaid
classDiagram
  RequestDispatcherDXX --> MessageServiceDXX : authenticated user + active services
  RequestDispatcherDXX --> MutationRepositoryDXX : BEGIN/COMMIT + dedup
  MessageServiceDXX --> MessageRepositoryDXX : conversation/message/receipt
  MessageRepositoryDXX --> DatabaseDXX : bound SQL
  MessageServiceDXX --> SocialRepositoryDXX : owned/friends
  ChatPageDXX --> TcpClientDXX : async requests
  ChatPageDXX --> MessageModelDXX : message ID merge
  WorkspaceDXX --> ChatPageDXX : QQ/WECHAT only
```

类名以实际源码为准；已有类承担的任务不再机械拆空壳ConnectionManager/DatabaseWorker。客户端不链接SQLite，服务端不依赖Qt Widgets。群组管理策略与聊天成员资格是不同职责，不能以管理员/群主身份绕过聊天子群成员检查。

```mermaid
sequenceDiagram
  participant A as Alice Qt process/socket A
  participant N as POSIX network thread
  participant W as DB worker + Dispatcher
  participant D as SQLite
  participant B as Bob Qt process/socket B
  A->>N: framed chat.private.send(UUID,text)
  N->>W: mutex task queue
  W->>D: BEGIN IMMEDIATE
  W->>D: auth/friends + pair + message + receipt + dedup
  W->>D: COMMIT
  W-->>N: response stored + authorized events queue
  N-->>A: response(message_id,stored)
  N-->>B: chat.message.new(message_id)
  B->>B: merge message ID into model
  B->>N: chat.message.ack(message_id)
  N->>W: ACK task
  W->>D: verify recipient + receipt + COMMIT
  W-->>N: delivered response + sender event
  N-->>A: chat.message.delivered(actual recipient)
```

TCP send返回只说明本地缓冲已接受，不能视为送达。delivered由Bob独立ACK且服务器落库证明；没有read ACK及已读图标。Alice响应丢失重试原UUID得到原message_id，不重新推送，Bob也可由离线sync补齐。

```mermaid
flowchart LR
  U[Qt main event loop: Widgets/model] <-->|signals/async| Q[QTcpSocket + frame decoder]
  Q <-->|loopback TCP| P[POSIX poll: FD/read/write buffers]
  P -->|bounded tasks under mutex| W[Single DB worker: sessions/services]
  W -->|bound queries/transactions| S[(SQLite WAL)]
  W -->|bounded result/event queue| P
```

网络线程独占FD及发送缓冲，partial sends保留未发送尾部；worker独占Database与会话，没有跨线程无锁访问socket或SQLite。结果投递不等待客户端读取；慢连接受输出上限断开，线程间队列亦需限额。退出先关闭监听与客户端、通知worker停止、处理已入队任务并join，数据库正常关闭。

```mermaid
stateDiagram-v2
  [*] --> pending: 用户发送，固定UUID
  pending --> stored: SQLite提交响应
  pending --> unknown: 超时/断线
  pending --> failed: 明确业务拒绝
  unknown --> stored: 原UUID重试/权威消息合并
  unknown --> failed: 明确拒绝且无保存证据
  stored --> delivered: 实际接收者ACK已持久化
```

UI模型按message_id稳定排序，响应和event按request_id与message_id更新同一草稿。会话切换保留未发送草稿，历史分页合并而非重复追加；回调带QObject生命周期与会话标识，重连不重复安装信号连接。自动重连与重新认证/激活的实际边界见实现报告。
