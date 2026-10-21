"""Disposable real-socket fixtures; events never masquerade as responses."""
import json, socket, struct, subprocess, time, uuid
from pathlib import Path

PASSWORD = 'Mvp2-Pass-2026!'
SERVICES = ['QQ', 'WECHAT', 'WEIBO']

def registration(name, services=None):
    return dict(login_name=name, nickname=name, password=PASSWORD, birthday=None,
                location='长春', services=services or SERVICES)

class Client:
    def __init__(self, port):
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=5)
        self.buffer = b''
        self.events = []
        self.responses = {}
    def close(self):
        self.socket.close()
    def receive(self, timeout=5):
        deadline = time.monotonic() + timeout
        while len(self.buffer) < 4 or len(self.buffer) < 4 + struct.unpack('!I', self.buffer[:4])[0]:
            self.socket.settimeout(max(.001, deadline-time.monotonic()))
            block = self.socket.recv(65536)
            if not block: raise EOFError('server closed socket')
            self.buffer += block
        length = struct.unpack('!I', self.buffer[:4])[0]
        assert 0 < length <= 65536, length
        message = json.loads(self.buffer[4:4+length])
        self.buffer = self.buffer[4+length:]
        if message.get('kind') == 'event':
            assert message.get('event_id') and message.get('type'), message
            self.events.append(message)
        else:
            self.responses[message['request_id']] = message
        return message
    def call(self, kind, data=None, rid=None):
        rid = rid or str(uuid.uuid4())
        raw = json.dumps(dict(kind='request', type=kind, request_id=rid, data=data or {})).encode()
        self.socket.sendall(struct.pack('!I', len(raw)) + raw)
        deadline = time.monotonic() + 8
        while rid not in self.responses:
            self.receive(max(.001, deadline-time.monotonic()))
        return self.responses.pop(rid)
    def event(self, kind, after=0, timeout=3):
        deadline = time.monotonic() + timeout
        while True:
            for event in self.events[after:]:
                if event['type'] == kind: return event
            self.receive(max(.001, deadline-time.monotonic()))

class Server:
    def __init__(self, exe, db):
        self.exe, self.db = str(Path(exe).resolve()), Path(db)
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0)); self.port = probe.getsockname()[1]
        self.process = None
        self.clients = []
        self.log = open(str(db)+'.server.log', 'a')
    def start(self):
        self.process = subprocess.Popen([self.exe, '--db', str(self.db), '--port', str(self.port)], stdout=self.log, stderr=self.log)
        for _ in range(100):
            if self.process.poll() is not None: raise RuntimeError('server exited: '+Path(self.log.name).read_text())
            try:
                probe = Client(self.port); probe.close(); return
            except OSError: time.sleep(.05)
        raise RuntimeError('server startup timed out')
    def stop(self):
        for client in self.clients: client.close()
        self.clients.clear()
        if self.process and self.process.poll() is None:
            self.process.send_signal(2)
            try: self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.wait(); raise
            assert self.process.returncode == 0, self.process.returncode
    def close(self):
        try: self.stop()
        finally: self.log.close()
    def client(self):
        client = Client(self.port); self.clients.append(client); return client
    def user(self, name, services=None, activate=True):
        client = self.client()
        response = ok(client.call('auth.register', registration(name, services)))
        ids = {a['service_type']:a['display_id'] for a in response['accounts']}
        ok(client.call('auth.login', dict(login_name=name, password=PASSWORD)))
        if activate:
            for service in ids: ok(client.call('account.service.activate', dict(service_type=service)))
        return client, ids
    def login(self, name, services=SERVICES):
        client = self.client()
        ok(client.call('auth.login', dict(login_name=name, password=PASSWORD)))
        for service in services: ok(client.call('account.service.activate', dict(service_type=service)))
        return client

def ok(response):
    assert response.get('success') is True, response
    return response['data']

def error(response, code):
    assert response.get('success') is False and response.get('error', {}).get('code') == code, response
    return response

def rows(client, kind, **data):
    result = ok(client.call(kind, data))
    assert isinstance(result['items'], list) and isinstance(result['total'], int), result
    assert 'next_cursor' in result, result
    return result

def no_private(value):
    if isinstance(value, dict):
        assert not ({'user_id','account_pk','sender_account_pk','target_account_pk','password_hash','session_token','account_a','account_b','owner_account_pk','friend_account_pk','qq_account_pk','wechat_account_pk'} & value.keys()), value
        for item in value.values(): no_private(item)
    elif isinstance(value, list):
        for item in value: no_private(item)

def send(a, target, service='QQ', rid=None):
    return ok(a.call('friend.request.send', dict(service_type=service,target_display_id=target,verification_message='hello'), rid))['friend_request_id']

def befriend(a, b, target, service='QQ'):
    request = send(a, target, service)
    ok(b.call('friend.request.respond', dict(friend_request_id=request, decision='ACCEPT')))
    return request
