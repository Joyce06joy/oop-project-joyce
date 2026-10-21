# MVP 1 协议与数据库

协议为任务书 v1：4 字节大端无符号 payload 长度 + UTF-8 JSON；长度 1–65536。半包缓存，多包逐一取出，严格拒绝非法 UTF-8、JSON、长度；帧级错误直接关闭连接，不强求发送无法关联的错误响应。信封错误返回 PROTOCOL_ERROR。异常信封中超 64 字节或非字符串的 type/request_id 不回显，响应字段设为 null，避免异常响应放大。

请求：`{"kind":"request","type":"system.ping","request_id":"UUID-or-128bit-random-id","data":{}}`。
响应：`{"kind":"response","type":"system.ping","request_id":"...","success":true,"data":{"protocol_version":1},"error":null}`。
失败：同一信封，`success:false,data:null,error:{code,message}`。

request_id 为 16–64 个 ASCII 字母、数字、下划线或连字符；客户端生成 UUID。注册采用数据库持久幂等；其他操作不持久记录 ID，登录/查询/激活可重试（重复激活不重复增加状态），重复退出在已退出后返回 AUTH_REQUIRED，客户端回到登录页。

| 请求 | data | 结果 |
|---|---|---|
| auth.register | login_name,nickname,password,birthday(null/ISO日期),location(null/文本),services(数组) | user_id,registered_at,accounts(service_type/display_id) |
| auth.login | login_name,password | user_id,nickname,services(service_type/display_id/active),session_id |
| auth.logout | {}，须登录 | logged_out:true |
| account.services.list | {}，须登录 | services |
| account.service.activate | service_type，须登录且已开通 | services（包括当前 active） |
| system.ping | {} | protocol_version:1 |

错误码：VALIDATION_ERROR、LOGIN_NAME_TAKEN、AUTH_INVALID_CREDENTIALS、AUTH_REQUIRED、SERVICE_NOT_OPEN、REQUEST_ID_CONFLICT、PROTOCOL_ERROR、DATABASE_ERROR、SERVER_ERROR。用户不存在/密码错误使用相同错误及相同 KDF 验证路径。

同注册 ID 的等价载荷：登录名大小写归一；服务排序去重；可选字段缺省与 null 等价；其他文本按原文比较，空 location 与 null 有区别。确认密码仅客户端比较，不传给服务端。新请求在事务外生成随机盐 KDF；事务内分配编号、插入用户/所选服务/幂等记录并 commit，之后才返回成功。失败 rollback 包括计数器；幂等记录的 fingerprint 为规范化字段与该用户已保存的带盐 KDF 记录的 SHA256，重放时还必须验证提交的密码。明文密码不进入审计、日志或指纹原文持久存储。

服务端只把连接作为会话权限来源；session_id 是该连接会话的临时标识，不是跨连接 bearer token。不读客户端传入的 user_id 或 session_id 来决定身份。重登创建新标识，清空本连接 active 服务；断线排入相同工作队列清理。

网络 poll 循环最多接受 64 个连接，每连接最多 8 个在途业务请求并施加读取背压，超限突发可断开。网络收发非阻塞并支持部分发送。独立数据库线程按序执行 SQL 和 KDF；任务/结果跨线程深拷贝 JSON，连接使用递增序号，避免文件描述符复用将旧响应发给新连接。断线前已经接收的注册可能仍提交，故客户端必须重试原 ID 取回结果。

数据库由 schema.sql 唯一维护并在 CMake 时生成内嵌字符串，启动不依赖当前工作目录寻找 SQL。所有 SQL 参数绑定；foreign_keys=ON，user_version=1，事务注册与 WAL；同数据库使用 POSIX flock 防止第二个服务端误用。此阶段仅初始化版本 1；不自动修改已有未知表或不支持的 schema。
