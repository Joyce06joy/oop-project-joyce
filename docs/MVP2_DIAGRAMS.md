# MVP2 UML、ER 与申请状态

以下图对应本轮已实现代码；根目录旧空窗口模板MainWindowDXX仍保留，但不是本业务页面入口。没有群、策略或聊天模块。

## 实际组件与类关系

```mermaid
classDiagram
    class LoginWindowDXX
    class RegisterWindowDXX
    class ServiceSelectWindowDXX
    class WorkspaceDXX
    class RequestPanelDXX
    class FriendPageDXX
    class FriendRequestPageDXX
    class RecommendationPageDXX
    class AccountPageDXX
    class TcpClientDXX
    class TcpServerDXX
    class RequestDispatcherDXX
    class AuthServiceDXX
    class AccountServiceDXX
    class FriendServiceDXX
    class RecommendationServiceDXX
    class SocialRepositoryDXX
    class MutationRepositoryDXX
    class DatabaseDXX
    class StatementDXX
    class ProtocolDXX
    class JsonDXX
    LoginWindowDXX *-- TcpClientDXX
    LoginWindowDXX *-- RegisterWindowDXX
    LoginWindowDXX *-- ServiceSelectWindowDXX
    ServiceSelectWindowDXX *-- WorkspaceDXX
    RequestPanelDXX <|-- WorkspaceDXX
    RequestPanelDXX <|-- FriendPageDXX
    RequestPanelDXX <|-- FriendRequestPageDXX
    RequestPanelDXX <|-- RecommendationPageDXX
    RequestPanelDXX <|-- AccountPageDXX
    WorkspaceDXX *-- FriendPageDXX
    WorkspaceDXX *-- FriendRequestPageDXX
    WorkspaceDXX *-- RecommendationPageDXX
    WorkspaceDXX *-- AccountPageDXX
    RequestPanelDXX --> TcpClientDXX : async requests/events
    TcpClientDXX --> ProtocolDXX
    ProtocolDXX --> JsonDXX
    TcpServerDXX --> ProtocolDXX
    TcpServerDXX --> RequestDispatcherDXX : worker queue
    RequestDispatcherDXX *-- AuthServiceDXX
    RequestDispatcherDXX *-- AccountServiceDXX
    RequestDispatcherDXX *-- FriendServiceDXX
    RequestDispatcherDXX *-- RecommendationServiceDXX
    RequestDispatcherDXX *-- MutationRepositoryDXX
    AccountServiceDXX *-- SocialRepositoryDXX
    FriendServiceDXX *-- SocialRepositoryDXX
    RecommendationServiceDXX *-- SocialRepositoryDXX
    SocialRepositoryDXX --> DatabaseDXX
    MutationRepositoryDXX --> DatabaseDXX
    AuthServiceDXX --> DatabaseDXX
    AccountServiceDXX --> DatabaseDXX
    FriendServiceDXX --> DatabaseDXX
    DatabaseDXX --> StatementDXX
```

组合使用同一TcpClient与数据库连接，没有复制UI中的业务权限。RequestPanelDXX基类负责异步关联、加载、错误与未确认mutation重试；页面子类只组织输入/展示。服务端各类由工作线程创建使用；没有让网络线程执行SQL。

## 数据关系

```mermaid
erDiagram
    users ||--o{ accounts : owns
    users ||--o{ registration_requests : registers
    users ||--o{ mutation_requests : mutates
    accounts ||--o| account_bindings : qq_side
    accounts ||--o| account_bindings : wechat_side
    accounts ||--o{ friend_requests : sender
    accounts ||--o{ friend_requests : target
    accounts ||--o{ friendships : lower_account
    accounts ||--o{ friendships : higher_account
    accounts ||--o{ friend_remarks : owner
    accounts ||--o{ friend_remarks : friend
    users {
        INTEGER user_id PK
        TEXT login_name UK
        TEXT shared_social_id UK
        TEXT password_hash
    }
    accounts {
        INTEGER account_pk PK
        INTEGER user_id FK
        TEXT service_type
        TEXT display_id
        TEXT status
    }
    account_bindings {
        INTEGER binding_id PK
        INTEGER qq_account_pk FK,UK
        INTEGER wechat_account_pk FK,UK
        TEXT created_at
    }
    friend_requests {
        TEXT request_id PK
        TEXT service_type
        INTEGER sender_account_pk FK
        INTEGER target_account_pk FK
        TEXT verification_message
        TEXT status
        TEXT created_at
        TEXT resolved_at
    }
    friendships {
        INTEGER account_a PK,FK
        INTEGER account_b PK,FK
        TEXT created_at
    }
    friend_remarks {
        INTEGER owner_account_pk PK,FK
        INTEGER friend_account_pk PK,FK
        TEXT remark
        TEXT updated_at
    }
    registration_requests {
        TEXT request_id PK
        INTEGER user_id FK
        TEXT request_fingerprint
    }
    mutation_requests {
        TEXT request_id PK
        INTEGER user_id FK
        TEXT request_type
        TEXT request_fingerprint
        TEXT result_json
    }
    id_counters {
        TEXT namespace PK
        INTEGER next_value
    }
```

accounts复合唯一与friend_requests部分唯一索引见[DB_SCHEMA_V2.md](DB_SCHEMA_V2.md)。friend_edges为friendships派生的只读双向视图，不是第二份关系事实。图省略未变化的基础资料字段，完整DDL以原schema与002迁移为准。

## 好友申请状态与事务

```mermaid
stateDiagram-v2
    [*] --> PENDING: 同服务有效账号、非自己、无好友及无待决
    PENDING --> ACCEPTED: 目标主体且服务已激活 / 同事务建唯一无向关系
    PENDING --> REJECTED: 目标主体且服务已激活 / 不建关系
    ACCEPTED --> [*]
    REJECTED --> [*]
```

CANCELLED是数据库保留合法终态，本轮不提供撤回请求或界面。已处理申请不能再次推进；同correlation ID重试返回原结果。双向申请竞争由无序对PENDING唯一索引及串行事务确定为一个成功、另一个FRIEND_REQUEST_PENDING。

```mermaid
sequenceDiagram
    participant A as Alice Qt
    participant N as POSIX Network
    participant W as DB Worker / Services
    participant D as SQLite
    participant B as Bob Qt
    A->>N: friend.request.send(UUID)
    N->>W: connection_id + validated frame
    W->>D: BEGIN / actor authorization / PENDING + mutation result
    D-->>W: COMMIT success
    W-->>N: response + targeted event envelopes
    N-->>A: success(PENDING)
    N-->>B: friend.request.new(event_id)
    B->>N: friend.request.pending
    N->>W: same connected session
    W->>D: authorized paginated query
    D-->>W: committed requests
    W-->>N: response
    N-->>B: pending items
```

事件丢失不丢申请；离线后再次查询恢复。所有通知仅在提交后生成，幂等重放不重复产生事实或事件。WebSocket、Qt服务端替代和群聊天均不在本实现中。
