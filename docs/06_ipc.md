# 6장: 프로세스 간 통신 (IPC)

## 6.1 파이프 (Pipe)

### Anonymous Pipe (익명 파이프)
부모-자식 프로세스 간 단방향 통신에 사용됩니다.

```c
BOOL CreatePipe(
    PHANDLE               hReadPipe,         // [OUT] 읽기 핸들
    PHANDLE               hWritePipe,        // [OUT] 쓰기 핸들
    LPSECURITY_ATTRIBUTES lpPipeAttributes,  // 보안 속성 (상속 설정)
    DWORD                 nSize              // 버퍼 크기 (0=기본값)
);
```

```
┌─────────────────┐                    ┌─────────────────┐
│   부모 프로세스  │                    │   자식 프로세스  │
│                 │                    │                 │
│  hWritePipe ────┼────── 파이프 ──────┼──> hReadPipe   │
│                 │      (단방향)       │                 │
└─────────────────┘                    └─────────────────┘
```

### Named Pipe (명명된 파이프)
무관한 프로세스 간 양방향 통신이 가능합니다.

```c
// 서버: 파이프 생성
HANDLE CreateNamedPipeW(
    LPCWSTR               lpName,             // 이름: \\.\pipe\PipeName
    DWORD                 dwOpenMode,         // PIPE_ACCESS_DUPLEX 등
    DWORD                 dwPipeMode,         // 메시지/바이트 모드
    DWORD                 nMaxInstances,      // 최대 인스턴스 수
    DWORD                 nOutBufferSize,     // 출력 버퍼 크기
    DWORD                 nInBufferSize,      // 입력 버퍼 크기
    DWORD                 nDefaultTimeOut,    // 기본 타임아웃
    LPSECURITY_ATTRIBUTES lpSecurityAttributes
);

// 클라이언트: 파이프 연결
HANDLE hPipe = CreateFileW(
    L"\\\\.\\pipe\\MyPipe",
    GENERIC_READ | GENERIC_WRITE,
    0, NULL, OPEN_EXISTING, 0, NULL
);
```

### Named Pipe 서버-클라이언트 패턴
```c
// 서버
HANDLE hPipe = CreateNamedPipeW(
    L"\\\\.\\pipe\\GameServerPipe",
    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES,
    4096, 4096,
    0, NULL
);

// 클라이언트 연결 대기
ConnectNamedPipe(hPipe, NULL);

// 통신
char buffer[256];
DWORD bytesRead;
ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL);

// 연결 종료
DisconnectNamedPipe(hPipe);
CloseHandle(hPipe);
```

---

## 6.2 메일슬롯 (Mailslot)

브로드캐스트 가능한 단방향 IPC입니다.

```c
// 서버 (수신측)
HANDLE hSlot = CreateMailslotW(
    L"\\\\.\\mailslot\\MyMailslot",
    0,              // 최대 메시지 크기 (0=무제한)
    MAILSLOT_WAIT_FOREVER,
    NULL
);

// 클라이언트 (송신측)
HANDLE hFile = CreateFileW(
    L"\\\\.\\mailslot\\MyMailslot",
    GENERIC_WRITE,
    FILE_SHARE_READ,
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
);

// 브로드캐스트 (도메인 내 모든 메일슬롯)
HANDLE hBroadcast = CreateFileW(
    L"\\\\*\\mailslot\\MyMailslot",  // * = 브로드캐스트
    GENERIC_WRITE, ...
);
```

---

## 6.3 공유 메모리

가장 빠른 IPC 방식입니다. (3장에서 다룸)

```c
// 프로세스 A: 생성
HANDLE hMapping = CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    NULL, PAGE_READWRITE,
    0, 4096,
    L"MySharedMemory"
);
LPVOID pBuf = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

// 프로세스 B: 열기
HANDLE hMapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MySharedMemory");
LPVOID pBuf = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
```

---

## 6.4 소켓 (Winsock)

네트워크 및 로컬 IPC 모두 가능합니다.

### Winsock 초기화
```c
WSADATA wsaData;
WSAStartup(MAKEWORD(2, 2), &wsaData);
// ... 소켓 사용 ...
WSACleanup();
```

### TCP 서버 기본 흐름
```c
// 1. 소켓 생성
SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

// 2. 바인딩
sockaddr_in addr = { 0 };
addr.sin_family = AF_INET;
addr.sin_port = htons(9000);
addr.sin_addr.s_addr = INADDR_ANY;
bind(listenSock, (sockaddr*)&addr, sizeof(addr));

// 3. 리슨
listen(listenSock, SOMAXCONN);

// 4. Accept
SOCKET clientSock = accept(listenSock, NULL, NULL);

// 5. 송수신
recv(clientSock, buffer, size, 0);
send(clientSock, data, len, 0);

// 6. 종료
closesocket(clientSock);
closesocket(listenSock);
```

### 비동기 소켓 + IOCP
```c
// 소켓을 IOCP에 연결
CreateIoCompletionPort((HANDLE)socket, hIOCP, (ULONG_PTR)pContext, 0);

// 비동기 수신
WSABUF wsaBuf = { len, buffer };
DWORD flags = 0;
WSARecv(socket, &wsaBuf, 1, NULL, &flags, &overlapped, NULL);

// 비동기 송신
WSASend(socket, &wsaBuf, 1, NULL, 0, &overlapped, NULL);
```

---

## 6.5 RPC (Remote Procedure Call)

원격 함수 호출을 로컬 함수처럼 사용할 수 있게 합니다.

```
┌─────────────────────────────────────────────────────────────────┐
│  클라이언트                           서버                       │
│  ┌─────────────┐                    ┌─────────────┐             │
│  │ 클라이언트   │                    │ 서버 함수   │             │
│  │ 코드        │                    │ 구현        │             │
│  └──────┬──────┘                    └──────▲──────┘             │
│         │ 함수 호출                        │                    │
│         ▼                                  │                    │
│  ┌─────────────┐     네트워크       ┌──────┴──────┐             │
│  │ 클라이언트   │ ─────────────────> │ 서버        │             │
│  │ 스텁        │ <───────────────── │ 스텁        │             │
│  └─────────────┘                    └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 게임서버에서의 IPC 선택

| 방식 | 사용 시나리오 | 특징 |
|------|--------------|------|
| Named Pipe | 로컬 서버 간 통신 | 간단, Windows 최적화 |
| 공유 메모리 | 고속 데이터 공유 | 가장 빠름, 동기화 필요 |
| 소켓 (TCP) | 클라이언트-서버 | 네트워크 통신 표준 |
| 소켓 (UDP) | 실시간 게임 데이터 | 빠름, 신뢰성 없음 |

게임서버에서는 **소켓 + IOCP** 조합이 가장 일반적입니다.
