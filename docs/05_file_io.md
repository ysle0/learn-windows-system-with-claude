# 5장: 파일 시스템과 I/O

## 5.1 파일 핸들과 기본 I/O

### 파일 열기/생성
```c
HANDLE CreateFileW(
    LPCWSTR               lpFileName,           // 파일 경로
    DWORD                 dwDesiredAccess,      // 접근 권한
    DWORD                 dwShareMode,          // 공유 모드
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, // 보안 속성
    DWORD                 dwCreationDisposition,// 생성 방식
    DWORD                 dwFlagsAndAttributes, // 플래그 및 속성
    HANDLE                hTemplateFile         // 템플릿 파일
);
```

### 접근 권한 (dwDesiredAccess)
| 플래그 | 값 | 설명 |
|--------|-----|------|
| GENERIC_READ | 0x80000000 | 읽기 |
| GENERIC_WRITE | 0x40000000 | 쓰기 |
| GENERIC_EXECUTE | 0x20000000 | 실행 |
| GENERIC_ALL | 0x10000000 | 모든 권한 |

### 공유 모드 (dwShareMode)
| 플래그 | 설명 |
|--------|------|
| 0 | 독점 (다른 프로세스 접근 불가) |
| FILE_SHARE_READ | 다른 프로세스 읽기 허용 |
| FILE_SHARE_WRITE | 다른 프로세스 쓰기 허용 |
| FILE_SHARE_DELETE | 다른 프로세스 삭제 허용 |

### 생성 방식 (dwCreationDisposition)
| 플래그 | 파일 존재 시 | 파일 없을 시 |
|--------|-------------|-------------|
| CREATE_NEW | 실패 | 생성 |
| CREATE_ALWAYS | 덮어쓰기 | 생성 |
| OPEN_EXISTING | 열기 | 실패 |
| OPEN_ALWAYS | 열기 | 생성 |
| TRUNCATE_EXISTING | 비우고 열기 | 실패 |

### 기본 파일 I/O
```c
// 파일 열기
HANDLE hFile = CreateFileW(
    L"test.txt",
    GENERIC_READ | GENERIC_WRITE,
    0,
    NULL,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL
);

// 쓰기
const char* data = "Hello, World!";
DWORD written;
WriteFile(hFile, data, strlen(data), &written, NULL);

// 파일 포인터 이동
SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

// 읽기
char buffer[256];
DWORD read;
ReadFile(hFile, buffer, sizeof(buffer), &read, NULL);

// 닫기
CloseHandle(hFile);
```

---

## 5.2 동기 vs 비동기 I/O

### 동기 I/O
```
애플리케이션              커널                  디스크
     │                     │                     │
     │ ReadFile() ─────────>│                     │
     │                     │ ─────────────────────>│
     │   블로킹 대기        │                     │ 디스크 읽기
     │                     │ <─────────────────────│
     │ <─────── 완료 ───────│                     │
     │                     │                     │
     ▼ 다음 코드 실행      │                     │
```

### 비동기 I/O (Overlapped)
```
애플리케이션              커널                  디스크
     │                     │                     │
     │ ReadFile() ─────────>│                     │
     │ <─ 즉시 반환 (PENDING)│                     │
     │                     │ ─────────────────────>│
     │ 다른 작업 수행       │                     │ 디스크 읽기
     │                     │ <─────────────────────│
     │                     │                     │
     │ 완료 확인 ─────────>│                     │
     │ <─── 데이터 ─────────│                     │
```

### 왜 비동기 I/O가 중요한가?
```
게임서버 시나리오 (1000명 동시 접속):

동기 I/O:
┌───────────────────────────────────────────────────────────────┐
│  클라이언트 1  │  클라이언트 2  │  ...  │  클라이언트 1000   │
│    recv()      │    대기...     │       │      대기...       │
│    처리        │    대기...     │       │      대기...       │
│    send()      │    recv()      │       │      대기...       │
│                │    처리        │       │      대기...       │
│                │    send()      │       │                    │
└───────────────────────────────────────────────────────────────┘
→ 순차 처리, 매우 느림

비동기 I/O + IOCP:
┌───────────────────────────────────────────────────────────────┐
│  Worker 1      │  Worker 2      │  Worker 3    │  Worker 4   │
│  클라 7 처리   │  클라 23 처리  │  클라 156 처리│ 클라 891 처리│
│  클라 45 처리  │  클라 8 처리   │  클라 77 처리 │ 클라 12 처리 │
└───────────────────────────────────────────────────────────────┘
→ 동시 처리, 매우 빠름
```

---

## 5.3 Overlapped I/O

### OVERLAPPED 구조체
```c
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;         // 내부 상태 (OS 사용)
    ULONG_PTR InternalHigh;     // 전송된 바이트 수
    union {
        struct {
            DWORD Offset;       // 파일 오프셋 (하위 32비트)
            DWORD OffsetHigh;   // 파일 오프셋 (상위 32비트)
        };
        PVOID Pointer;
    };
    HANDLE hEvent;              // 완료 이벤트 (선택)
} OVERLAPPED;
```

### 비동기 파일 열기
```c
// FILE_FLAG_OVERLAPPED 필수!
HANDLE hFile = CreateFileW(
    L"data.bin",
    GENERIC_READ,
    FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,  // 비동기 I/O 활성화
    NULL
);
```

### 비동기 읽기
```c
char buffer[4096];
OVERLAPPED ov = { 0 };
ov.Offset = 0;  // 파일 시작부터

BOOL result = ReadFile(
    hFile,
    buffer,
    sizeof(buffer),
    NULL,           // 비동기 시 무시됨
    &ov             // OVERLAPPED 구조체
);

if (!result && GetLastError() == ERROR_IO_PENDING) {
    // I/O 진행 중 - 정상!

    // 방법 1: GetOverlappedResult로 대기
    DWORD bytesRead;
    GetOverlappedResult(hFile, &ov, &bytesRead, TRUE);  // TRUE = 대기
    printf("읽은 바이트: %lu\n", bytesRead);

    // 방법 2: 이벤트로 대기
    // ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    // WaitForSingleObject(ov.hEvent, INFINITE);
}
```

### 완료 확인 방법
```
┌─────────────────────────────────────────────────────────────────┐
│                   비동기 I/O 완료 확인 방법                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. GetOverlappedResult (대기 가능)                             │
│     - 단순하지만 블로킹 가능                                    │
│     - 단일 I/O 완료 확인에 적합                                 │
│                                                                 │
│  2. OVERLAPPED.hEvent                                           │
│     - WaitForSingleObject/Multiple로 대기                       │
│     - 여러 I/O 중 하나 완료 대기 가능                           │
│                                                                 │
│  3. I/O Completion Port (IOCP) ★★★                            │
│     - 가장 확장성 좋음                                          │
│     - 다수의 핸들, 다수의 I/O 처리에 최적                       │
│     - 게임서버 표준                                             │
│                                                                 │
│  4. Alertable I/O (APC)                                         │
│     - 콜백 기반                                                 │
│     - 사용 복잡                                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5.4 I/O Completion Port (IOCP)

### IOCP란?
IOCP는 **Windows에서 가장 효율적인 비동기 I/O 처리 메커니즘**입니다. 수천 개의 동시 연결을 적은 스레드로 처리할 수 있습니다.

### IOCP 동작 원리
```
┌─────────────────────────────────────────────────────────────────┐
│                        IOCP 아키텍처                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                      I/O 요청들                          │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │   │
│  │  │ recv()  │ │ send()  │ │ recv()  │ │ recv()  │       │   │
│  │  │ 소켓 1  │ │ 소켓 2  │ │ 소켓 3  │ │ 파일    │       │   │
│  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘       │   │
│  │       │          │          │          │               │   │
│  └───────┼──────────┼──────────┼──────────┼───────────────┘   │
│          │          │          │          │                    │
│          ▼          ▼          ▼          ▼                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              I/O Completion Port (커널)                  │   │
│  │  ┌─────────────────────────────────────────────────┐    │   │
│  │  │            완료 큐 (Completion Queue)            │    │   │
│  │  │  [완료1] [완료2] [완료3] [완료4] ...            │    │   │
│  │  └─────────────────────────────────────────────────┘    │   │
│  └──────────────────────────┬──────────────────────────────┘   │
│                             │                                   │
│          ┌──────────────────┼──────────────────┐               │
│          ▼                  ▼                  ▼               │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐        │
│  │  Worker 1   │    │  Worker 2   │    │  Worker 3   │        │
│  │ GetQueued.. │    │ GetQueued.. │    │ GetQueued.. │        │
│  │  → 처리     │    │  → 처리     │    │  → 처리     │        │
│  └─────────────┘    └─────────────┘    └─────────────┘        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### IOCP API

#### 1. IOCP 생성
```c
HANDLE CreateIoCompletionPort(
    HANDLE    FileHandle,          // 연결할 핸들 (또는 INVALID_HANDLE_VALUE)
    HANDLE    ExistingCompletionPort, // 기존 IOCP (또는 NULL)
    ULONG_PTR CompletionKey,       // 완료 키 (사용자 정의 데이터)
    DWORD     NumberOfConcurrentThreads // 동시 실행 스레드 수 (0=CPU 수)
);

// 새 IOCP 생성
HANDLE hIOCP = CreateIoCompletionPort(
    INVALID_HANDLE_VALUE,
    NULL,
    0,
    0  // CPU 코어 수만큼 동시 실행
);

// 소켓/파일을 IOCP에 연결
CreateIoCompletionPort(
    (HANDLE)socket,
    hIOCP,
    (ULONG_PTR)pClientContext,  // 완료 시 이 값 반환됨
    0
);
```

#### 2. 완료 대기
```c
BOOL GetQueuedCompletionStatus(
    HANDLE       CompletionPort,     // IOCP 핸들
    LPDWORD      lpNumberOfBytes,    // [OUT] 전송 바이트
    PULONG_PTR   lpCompletionKey,    // [OUT] 완료 키
    LPOVERLAPPED *lpOverlapped,      // [OUT] OVERLAPPED 포인터
    DWORD        dwMilliseconds      // 타임아웃
);

// 워커 스레드에서
while (TRUE) {
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED pOv;

    BOOL success = GetQueuedCompletionStatus(
        hIOCP,
        &bytesTransferred,
        &completionKey,
        &pOv,
        INFINITE
    );

    if (success) {
        // I/O 완료 처리
        ClientContext* pClient = (ClientContext*)completionKey;
        ProcessIO(pClient, pOv, bytesTransferred);
    } else {
        if (pOv != NULL) {
            // I/O 실패
            HandleError(completionKey, pOv);
        } else {
            // GetQueuedCompletionStatus 자체 실패
            break;
        }
    }
}
```

#### 3. 수동으로 완료 포스트
```c
// 커스텀 이벤트를 IOCP에 전달 (종료 신호 등)
PostQueuedCompletionStatus(
    hIOCP,
    0,                  // 바이트 수
    SHUTDOWN_KEY,       // 특별한 완료 키
    NULL                // OVERLAPPED
);
```

### IOCP 스레드 관리
```
┌─────────────────────────────────────────────────────────────────┐
│                IOCP 스레드 스케줄링                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  NumberOfConcurrentThreads = 4 (예: 4코어 CPU)                  │
│                                                                 │
│  Worker 1: 처리 중  ████████░░░░░░░░░░░░░░░░░░░░░░             │
│  Worker 2: 처리 중  ████████░░░░░░░░░░░░░░░░░░░░░░             │
│  Worker 3: 처리 중  ████████░░░░░░░░░░░░░░░░░░░░░░             │
│  Worker 4: 처리 중  ████████░░░░░░░░░░░░░░░░░░░░░░             │
│  Worker 5: 대기     ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ (4개 초과)   │
│  Worker 6: 대기     ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░             │
│                                                                 │
│  Worker 1이 WaitForSingleObject()로 블로킹되면:                 │
│  → Worker 5가 깨어나서 처리 시작                                │
│                                                                 │
│  권장: Worker 수 = CPU 코어 수 * 2                              │
│        (블로킹 I/O 허용 시)                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5.5 디렉토리 변경 감시

### ReadDirectoryChangesW
```c
BOOL ReadDirectoryChangesW(
    HANDLE                          hDirectory,
    LPVOID                          lpBuffer,
    DWORD                           nBufferLength,
    BOOL                            bWatchSubtree,      // 하위 디렉토리 포함
    DWORD                           dwNotifyFilter,     // 감시 대상
    LPDWORD                         lpBytesReturned,
    LPOVERLAPPED                    lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

// 감시 필터
FILE_NOTIFY_CHANGE_FILE_NAME   // 파일 이름 변경
FILE_NOTIFY_CHANGE_DIR_NAME    // 디렉토리 이름 변경
FILE_NOTIFY_CHANGE_SIZE        // 파일 크기 변경
FILE_NOTIFY_CHANGE_LAST_WRITE  // 파일 수정
FILE_NOTIFY_CHANGE_CREATION    // 파일 생성
```

### 비동기 디렉토리 감시 (IOCP)
```c
// 디렉토리 열기
HANDLE hDir = CreateFileW(
    L"C:\\WatchFolder",
    FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
    NULL
);

// IOCP에 연결
CreateIoCompletionPort(hDir, hIOCP, DIR_WATCH_KEY, 0);

// 감시 시작
char buffer[4096];
OVERLAPPED ov = { 0 };

ReadDirectoryChangesW(
    hDir,
    buffer,
    sizeof(buffer),
    TRUE,   // 하위 디렉토리 포함
    FILE_NOTIFY_CHANGE_FILE_NAME |
    FILE_NOTIFY_CHANGE_DIR_NAME |
    FILE_NOTIFY_CHANGE_LAST_WRITE,
    NULL,
    &ov,
    NULL
);

// 결과 처리 (워커 스레드에서)
FILE_NOTIFY_INFORMATION* pInfo = (FILE_NOTIFY_INFORMATION*)buffer;
while (TRUE) {
    wchar_t fileName[MAX_PATH];
    memcpy(fileName, pInfo->FileName, pInfo->FileNameLength);
    fileName[pInfo->FileNameLength / sizeof(WCHAR)] = L'\0';

    switch (pInfo->Action) {
    case FILE_ACTION_ADDED:
        printf("추가: %ls\n", fileName);
        break;
    case FILE_ACTION_REMOVED:
        printf("삭제: %ls\n", fileName);
        break;
    case FILE_ACTION_MODIFIED:
        printf("수정: %ls\n", fileName);
        break;
    case FILE_ACTION_RENAMED_OLD_NAME:
        printf("이름 변경 (이전): %ls\n", fileName);
        break;
    case FILE_ACTION_RENAMED_NEW_NAME:
        printf("이름 변경 (새로운): %ls\n", fileName);
        break;
    }

    if (pInfo->NextEntryOffset == 0) break;
    pInfo = (FILE_NOTIFY_INFORMATION*)((char*)pInfo + pInfo->NextEntryOffset);
}
```

---

## 게임서버 I/O 패턴

### 1. Per-Connection OVERLAPPED
```c
// 연결별 Overlapped 구조체 확장
struct IOContext : OVERLAPPED {
    enum Operation { OP_RECV, OP_SEND };

    Operation operation;
    WSABUF wsaBuf;
    char buffer[8192];

    IOContext() {
        memset(this, 0, sizeof(OVERLAPPED));
        wsaBuf.buf = buffer;
        wsaBuf.len = sizeof(buffer);
    }
};

struct ClientContext {
    SOCKET socket;
    IOContext recvContext;
    IOContext sendContext;
    // 클라이언트별 데이터...
};
```

### 2. 전형적인 IOCP 서버 루프
```c
DWORD WINAPI WorkerThread(LPVOID param) {
    HANDLE hIOCP = (HANDLE)param;

    while (TRUE) {
        DWORD bytesTransferred;
        ClientContext* pClient;
        IOContext* pIO;

        BOOL success = GetQueuedCompletionStatus(
            hIOCP,
            &bytesTransferred,
            (PULONG_PTR)&pClient,
            (LPOVERLAPPED*)&pIO,
            INFINITE
        );

        // 종료 신호 확인
        if (pClient == nullptr) {
            break;
        }

        // 연결 종료 또는 에러
        if (!success || bytesTransferred == 0) {
            CloseClient(pClient);
            continue;
        }

        // I/O 종류에 따라 처리
        switch (pIO->operation) {
        case IOContext::OP_RECV:
            OnRecv(pClient, pIO->buffer, bytesTransferred);
            // 다음 수신 시작
            PostRecv(pClient);
            break;

        case IOContext::OP_SEND:
            OnSendComplete(pClient);
            break;
        }
    }

    return 0;
}
```

### 3. 소켓 I/O 시작
```c
void PostRecv(ClientContext* pClient) {
    pClient->recvContext.operation = IOContext::OP_RECV;

    DWORD flags = 0;
    WSARecv(
        pClient->socket,
        &pClient->recvContext.wsaBuf,
        1,
        NULL,
        &flags,
        &pClient->recvContext,
        NULL
    );
}

void PostSend(ClientContext* pClient, const void* data, int len) {
    memcpy(pClient->sendContext.buffer, data, len);
    pClient->sendContext.wsaBuf.len = len;
    pClient->sendContext.operation = IOContext::OP_SEND;

    WSASend(
        pClient->socket,
        &pClient->sendContext.wsaBuf,
        1,
        NULL,
        0,
        &pClient->sendContext,
        NULL
    );
}
```

---

## 정리

| 개념 | 핵심 내용 |
|------|-----------|
| CreateFile | 파일/디바이스 핸들 생성, 플래그로 동작 제어 |
| 동기 I/O | 완료까지 블로킹, 단순하지만 비효율 |
| 비동기 I/O | FILE_FLAG_OVERLAPPED, 즉시 반환 |
| Overlapped | 비동기 I/O 상태 관리 구조체 |
| IOCP | 완료 큐 + 스레드 풀, 고성능 서버의 핵심 |
| GetQueuedCompletionStatus | IOCP 완료 이벤트 대기 |

다음 장에서는 프로세스 간 통신(IPC)에 대해 알아봅니다.
