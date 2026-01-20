# 4장: 동기화 객체

## 4.1 커널 객체의 개념

### 커널 객체란?
커널 객체는 **운영체제가 관리하는 리소스**를 나타내는 데이터 구조입니다. 유저 모드에서는 **핸들**을 통해 간접적으로 접근합니다.

### 커널 객체의 공통 특성
```
┌─────────────────────────────────────────────────────────────────┐
│                      커널 객체 구조                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   공통 헤더                                │  │
│  │  • 참조 카운트 (Reference Count)                          │  │
│  │  • 보안 기술자 (Security Descriptor)                      │  │
│  │  • 신호 상태 (Signaled State) ← 동기화에 핵심!            │  │
│  │  • 타입 정보                                              │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   객체별 데이터                            │  │
│  │  • Event: 수동/자동 리셋, 신호 상태                       │  │
│  │  • Mutex: 소유 스레드, 재귀 카운트                        │  │
│  │  • Semaphore: 현재 카운트, 최대 카운트                    │  │
│  │  • ...                                                    │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 신호 상태 (Signaled State)
모든 동기화 객체의 핵심 개념입니다.

| 상태 | 의미 | WaitForXxx 결과 |
|------|------|-----------------|
| Signaled (신호) | "준비됨", "사용 가능" | 즉시 반환 |
| Non-signaled (비신호) | "대기 필요" | 블로킹 |

### 동기화 객체 종류
| 객체 | 용도 | 프로세스 간 | 소유권 |
|------|------|-------------|--------|
| Critical Section | 상호 배제 | ✗ | ✓ |
| Mutex | 상호 배제 | ✓ | ✓ |
| Semaphore | 리소스 카운팅 | ✓ | ✗ |
| Event | 신호 전달 | ✓ | ✗ |
| Waitable Timer | 시간 기반 신호 | ✓ | ✗ |
| SRWLock | 읽기/쓰기 락 | ✗ | ✗ |
| Condition Variable | 조건 대기 | ✗ | ✗ |

---

## 4.2 이벤트 (Event)

### 이벤트란?
이벤트는 **스레드 간 신호를 전달**하는 가장 단순한 동기화 객체입니다.

### 수동 리셋 vs 자동 리셋
```
수동 리셋 이벤트 (Manual-Reset):
────────────────────────────────────────────────────────
                SetEvent()              ResetEvent()
                    │                        │
   비신호 ──────────┼── 신호 ───────────────┼── 비신호
                    │         ↑              │
                    │     모든 대기          │
                    │     스레드 깨움        │
────────────────────────────────────────────────────────

자동 리셋 이벤트 (Auto-Reset):
────────────────────────────────────────────────────────
                SetEvent()        (자동으로 리셋)
                    │                   │
   비신호 ──────────┼── 신호 ──────────┼── 비신호
                    │      ↑            │
                    │   하나의 대기      │
                    │   스레드만 깨움    │
────────────────────────────────────────────────────────
```

### CreateEvent 함수
```c
HANDLE CreateEventW(
    LPSECURITY_ATTRIBUTES lpEventAttributes,  // 보안 속성
    BOOL                  bManualReset,       // TRUE=수동, FALSE=자동
    BOOL                  bInitialState,      // TRUE=신호, FALSE=비신호
    LPCWSTR               lpName              // 이름 (NULL=익명)
);

// 이벤트 제어
SetEvent(hEvent);    // 신호 상태로 설정
ResetEvent(hEvent);  // 비신호 상태로 설정
PulseEvent(hEvent);  // 신호 후 즉시 리셋 (사용 비권장)
```

### 사용 패턴
```c
// 패턴 1: 작업 완료 통보 (자동 리셋)
HANDLE g_hWorkDone = CreateEvent(NULL, FALSE, FALSE, NULL);

// 워커 스레드
DWORD WINAPI Worker(LPVOID) {
    // 작업 수행...
    SetEvent(g_hWorkDone);  // 완료 통보
    return 0;
}

// 메인 스레드
WaitForSingleObject(g_hWorkDone, INFINITE);  // 완료 대기

// 패턴 2: 브로드캐스트 (수동 리셋)
HANDLE g_hStartSignal = CreateEvent(NULL, TRUE, FALSE, NULL);

// 여러 워커가 동시에 시작 대기
DWORD WINAPI Worker(LPVOID) {
    WaitForSingleObject(g_hStartSignal, INFINITE);  // 시작 대기
    // 작업 수행...
    return 0;
}

// 메인에서 모든 워커 시작
SetEvent(g_hStartSignal);  // 모든 워커 깨움
```

### 게임서버에서의 이벤트 사용
```c
// IOCP 워커 스레드 종료 신호
class IOCPServer {
    HANDLE m_hShutdownEvent;  // 수동 리셋

public:
    IOCPServer() {
        m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    void WorkerThread() {
        HANDLE waitHandles[2] = { m_hShutdownEvent, m_hIOCP };

        while (true) {
            // 종료 신호 또는 IOCP 완료 대기
            DWORD result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

            if (result == WAIT_OBJECT_0) {
                // 종료 신호
                break;
            }

            // IOCP 처리...
        }
    }

    void Shutdown() {
        SetEvent(m_hShutdownEvent);  // 모든 워커에 종료 신호
    }
};
```

---

## 4.3 세마포어 (Semaphore)

### 세마포어란?
세마포어는 **카운터 기반** 동기화 객체입니다. 리소스 풀의 동시 접근을 제한할 때 사용합니다.

### 동작 원리
```
세마포어 (초기값 3, 최대값 3):

┌────────────────────────────────────────────────────────────┐
│                                                            │
│  카운터: 3  (신호 상태)                                    │
│                                                            │
│  Wait() → 카운터: 2 (신호)    ← 스레드 1 획득             │
│  Wait() → 카운터: 1 (신호)    ← 스레드 2 획득             │
│  Wait() → 카운터: 0 (비신호)  ← 스레드 3 획득             │
│  Wait() → 블로킹!             ← 스레드 4 대기             │
│                                                            │
│  Release() → 카운터: 1 (신호) ← 스레드 1 해제, 4 깨어남   │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### CreateSemaphore 함수
```c
HANDLE CreateSemaphoreW(
    LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,  // 보안 속성
    LONG                  lInitialCount,          // 초기 카운트
    LONG                  lMaximumCount,          // 최대 카운트
    LPCWSTR               lpName                  // 이름
);

// 세마포어 해제 (카운트 증가)
BOOL ReleaseSemaphore(
    HANDLE hSemaphore,
    LONG   lReleaseCount,      // 증가량 (보통 1)
    LPLONG lpPreviousCount     // [OUT] 이전 카운트
);
```

### 사용 패턴: 연결 풀
```c
// 데이터베이스 연결 풀 (최대 10개 연결)
class ConnectionPool {
    HANDLE m_hSemaphore;
    Connection* m_connections[10];

public:
    ConnectionPool() {
        m_hSemaphore = CreateSemaphore(NULL, 10, 10, NULL);
        for (int i = 0; i < 10; i++) {
            m_connections[i] = new Connection();
        }
    }

    Connection* Acquire(DWORD timeout = INFINITE) {
        // 사용 가능한 연결 대기
        DWORD result = WaitForSingleObject(m_hSemaphore, timeout);

        if (result == WAIT_OBJECT_0) {
            // 연결 찾기 (Critical Section으로 보호 필요)
            return FindFreeConnection();
        }

        return nullptr;  // 타임아웃
    }

    void Release(Connection* conn) {
        // 연결 반환
        ReturnConnection(conn);

        // 세마포어 카운트 증가
        ReleaseSemaphore(m_hSemaphore, 1, NULL);
    }
};
```

### 뮤텍스와 세마포어 비교
| 특성 | Mutex | Semaphore |
|------|-------|-----------|
| 카운트 | 1 (바이너리) | N (카운팅) |
| 소유권 | 있음 (스레드) | 없음 |
| 재귀 잠금 | 가능 | 불가 |
| 용도 | 상호 배제 | 리소스 제한 |

---

## 4.4 뮤텍스 (Mutex)

### 뮤텍스란?
뮤텍스는 **상호 배제(Mutual Exclusion)**를 위한 동기화 객체입니다. 한 번에 하나의 스레드만 소유할 수 있습니다.

### 특징
1. **소유권**: 획득한 스레드만 해제 가능
2. **재귀 잠금**: 같은 스레드가 여러 번 획득 가능
3. **프로세스 간 공유**: 이름으로 공유 가능
4. **WAIT_ABANDONED**: 소유 스레드 종료 시 감지

### CreateMutex 함수
```c
HANDLE CreateMutexW(
    LPSECURITY_ATTRIBUTES lpMutexAttributes,  // 보안 속성
    BOOL                  bInitialOwner,      // TRUE=생성자가 소유
    LPCWSTR               lpName              // 이름
);

// 뮤텍스 해제
BOOL ReleaseMutex(HANDLE hMutex);
```

### WAIT_ABANDONED 처리
```c
DWORD result = WaitForSingleObject(hMutex, INFINITE);

switch (result) {
case WAIT_OBJECT_0:
    // 정상 획득
    break;

case WAIT_ABANDONED:
    // 이전 소유자가 ReleaseMutex 없이 종료됨!
    // 뮤텍스는 획득했지만, 보호하던 데이터가 불안정할 수 있음
    // → 데이터 정합성 검사 필요
    break;

case WAIT_TIMEOUT:
    // 타임아웃
    break;

case WAIT_FAILED:
    // 에러
    break;
}
```

### 단일 인스턴스 보장
```c
// 프로그램이 하나만 실행되도록 보장
int main() {
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"MyUniqueGameServer");

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("이미 실행 중입니다!\n");
        CloseHandle(hMutex);
        return 1;
    }

    // 프로그램 실행...

    CloseHandle(hMutex);
    return 0;
}
```

---

## 4.5 대기 함수들

### WaitForSingleObject
```c
DWORD WaitForSingleObject(
    HANDLE hHandle,      // 대기할 객체
    DWORD  dwMilliseconds// 타임아웃 (INFINITE = 무한)
);

// 반환값
WAIT_OBJECT_0   // 객체가 신호 상태가 됨
WAIT_TIMEOUT    // 타임아웃
WAIT_ABANDONED  // 뮤텍스 소유자가 종료됨
WAIT_FAILED     // 에러 (GetLastError() 확인)
```

### WaitForMultipleObjects
```c
DWORD WaitForMultipleObjects(
    DWORD        nCount,         // 객체 개수 (최대 64)
    const HANDLE *lpHandles,     // 핸들 배열
    BOOL         bWaitAll,       // TRUE=모두 대기, FALSE=하나라도
    DWORD        dwMilliseconds  // 타임아웃
);

// 반환값 (bWaitAll = FALSE 일 때)
WAIT_OBJECT_0 + n  // n번째 객체가 신호
WAIT_ABANDONED_0 + n // n번째 뮤텍스가 abandoned
```

### 여러 객체 대기 패턴
```c
// 패턴 1: 아무거나 하나
HANDLE events[3] = { hEvent1, hEvent2, hEvent3 };
DWORD result = WaitForMultipleObjects(3, events, FALSE, INFINITE);

if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + 3) {
    int signaled = result - WAIT_OBJECT_0;
    printf("Event %d signaled\n", signaled);
}

// 패턴 2: 모두 대기
WaitForMultipleObjects(3, events, TRUE, INFINITE);
printf("All events signaled\n");

// 패턴 3: 종료 이벤트 포함
HANDLE waitHandles[2] = { hShutdownEvent, hWorkEvent };

while (TRUE) {
    DWORD result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

    if (result == WAIT_OBJECT_0) {
        // 종료 이벤트
        break;
    } else if (result == WAIT_OBJECT_0 + 1) {
        // 작업 이벤트
        ProcessWork();
    }
}
```

### WaitForMultipleObjects 64개 제한 극복
```c
// 64개 이상 대기할 때
DWORD WaitForMoreThan64(HANDLE* handles, DWORD count, DWORD timeout) {
    if (count <= 64) {
        return WaitForMultipleObjects(count, handles, FALSE, timeout);
    }

    // 64개씩 그룹으로 나눠서 스레드에서 대기
    // 각 그룹의 결과를 상위 이벤트로 전파
    // ... 복잡한 구현 필요

    // 또는 RegisterWaitForSingleObject 사용
    return WAIT_FAILED;
}
```

### SignalObjectAndWait
```c
// 원자적으로: 객체 신호 + 다른 객체 대기
DWORD SignalObjectAndWait(
    HANDLE hObjectToSignal,  // 신호할 객체 (Event/Mutex/Semaphore)
    HANDLE hObjectToWaitOn,  // 대기할 객체
    DWORD  dwMilliseconds,   // 타임아웃
    BOOL   bAlertable        // APC 대기 여부
);

// Producer-Consumer 패턴에서 유용
// 1. 데이터 준비 완료 신호
// 2. 소비자 완료 대기
// → 두 작업 사이에 틈이 없음
```

### Alertable Wait와 APC
```c
// Alertable 대기: APC(Asynchronous Procedure Call) 처리 가능
DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable);
DWORD WaitForSingleObjectEx(HANDLE h, DWORD ms, BOOL bAlertable);
DWORD WaitForMultipleObjectsEx(DWORD n, HANDLE* h, BOOL all, DWORD ms, BOOL alertable);

// APC 큐에 콜백 추가
QueueUserAPC(APCProc, hThread, dwData);

// Alertable 대기 중인 스레드에서 APCProc 실행됨
VOID CALLBACK APCProc(ULONG_PTR dwParam) {
    printf("APC called with %llu\n", dwParam);
}
```

---

## 4.6 고급 동기화 객체

### SRWLock (Slim Reader/Writer Lock)
```c
SRWLOCK g_lock = SRWLOCK_INIT;

// 읽기 잠금 (여러 스레드 동시 가능)
AcquireSRWLockShared(&g_lock);
// 읽기 작업...
ReleaseSRWLockShared(&g_lock);

// 쓰기 잠금 (단독)
AcquireSRWLockExclusive(&g_lock);
// 쓰기 작업...
ReleaseSRWLockExclusive(&g_lock);

// TryAcquire 버전도 있음
if (TryAcquireSRWLockExclusive(&g_lock)) {
    // 쓰기 작업...
    ReleaseSRWLockExclusive(&g_lock);
}
```

### Condition Variable
```c
SRWLOCK g_lock = SRWLOCK_INIT;
CONDITION_VARIABLE g_cv = CONDITION_VARIABLE_INIT;
BOOL g_ready = FALSE;

// 생산자
AcquireSRWLockExclusive(&g_lock);
g_ready = TRUE;
ReleaseSRWLockExclusive(&g_lock);
WakeConditionVariable(&g_cv);  // 하나 깨움
// WakeAllConditionVariable(&g_cv);  // 모두 깨움

// 소비자
AcquireSRWLockExclusive(&g_lock);
while (!g_ready) {
    // 락 해제하고 대기, 깨어나면 락 다시 획득
    SleepConditionVariableSRW(&g_cv, &g_lock, INFINITE, 0);
}
// g_ready == TRUE
ReleaseSRWLockExclusive(&g_lock);
```

### Interlocked 함수들
```c
// 원자적 연산 (락 없이 빠름)
LONG value = 0;

InterlockedIncrement(&value);     // ++value
InterlockedDecrement(&value);     // --value
InterlockedAdd(&value, 10);       // value += 10
InterlockedExchange(&value, 100); // value = 100, 이전 값 반환

// Compare and Swap (CAS)
LONG old = InterlockedCompareExchange(
    &value,     // 대상
    newValue,   // 새 값
    expected    // 예상 값
);
// value == expected 이면 newValue로 교체, 아니면 변경 없음
// 반환: 원래 값

// 64비트 버전
InterlockedIncrement64(&value64);

// 포인터 버전
InterlockedExchangePointer(&ptr, newPtr);
InterlockedCompareExchangePointer(&ptr, newPtr, expectedPtr);
```

### Lock-Free 큐 예시
```c
// 단순 Lock-Free 스택 (Interlocked 사용)
struct Node {
    Node* next;
    int data;
};

Node* g_head = nullptr;

void Push(int data) {
    Node* newNode = new Node{ nullptr, data };

    do {
        newNode->next = g_head;
    } while (InterlockedCompareExchangePointer(
        (PVOID*)&g_head,
        newNode,
        newNode->next
    ) != newNode->next);
}

Node* Pop() {
    Node* head;
    do {
        head = g_head;
        if (head == nullptr) return nullptr;
    } while (InterlockedCompareExchangePointer(
        (PVOID*)&g_head,
        head->next,
        head
    ) != head);

    return head;
}
```

---

## 게임서버 동기화 패턴

### 1. Double-Checked Locking
```c
class Singleton {
    static Singleton* s_instance;
    static SRWLOCK s_lock;

public:
    static Singleton* GetInstance() {
        if (s_instance == nullptr) {  // 첫 번째 검사 (락 없이)
            AcquireSRWLockExclusive(&s_lock);

            if (s_instance == nullptr) {  // 두 번째 검사 (락 안에서)
                s_instance = new Singleton();
            }

            ReleaseSRWLockExclusive(&s_lock);
        }
        return s_instance;
    }
};
```

### 2. 읽기 많은 데이터
```c
// 플레이어 목록 (읽기 많음, 쓰기 적음)
class PlayerManager {
    SRWLOCK m_lock = SRWLOCK_INIT;
    std::map<UINT64, Player*> m_players;

public:
    // 읽기 (여러 스레드 동시 가능)
    Player* GetPlayer(UINT64 id) {
        AcquireSRWLockShared(&m_lock);
        auto it = m_players.find(id);
        Player* result = (it != m_players.end()) ? it->second : nullptr;
        ReleaseSRWLockShared(&m_lock);
        return result;
    }

    // 쓰기 (단독)
    void AddPlayer(UINT64 id, Player* player) {
        AcquireSRWLockExclusive(&m_lock);
        m_players[id] = player;
        ReleaseSRWLockExclusive(&m_lock);
    }
};
```

### 3. 작업 큐 (Producer-Consumer)
```c
class WorkQueue {
    CRITICAL_SECTION m_cs;
    CONDITION_VARIABLE m_cv;
    std::queue<Work*> m_queue;
    bool m_shutdown = false;

public:
    WorkQueue() {
        InitializeCriticalSection(&m_cs);
        InitializeConditionVariable(&m_cv);
    }

    void Push(Work* work) {
        EnterCriticalSection(&m_cs);
        m_queue.push(work);
        LeaveCriticalSection(&m_cs);
        WakeConditionVariable(&m_cv);
    }

    Work* Pop() {
        EnterCriticalSection(&m_cs);

        while (m_queue.empty() && !m_shutdown) {
            SleepConditionVariableCS(&m_cv, &m_cs, INFINITE);
        }

        Work* work = nullptr;
        if (!m_queue.empty()) {
            work = m_queue.front();
            m_queue.pop();
        }

        LeaveCriticalSection(&m_cs);
        return work;
    }

    void Shutdown() {
        EnterCriticalSection(&m_cs);
        m_shutdown = true;
        LeaveCriticalSection(&m_cs);
        WakeAllConditionVariable(&m_cv);
    }
};
```

---

## 정리

| 객체 | 핵심 용도 | 특징 |
|------|-----------|------|
| Event | 신호 전달 | 수동/자동 리셋 |
| Mutex | 상호 배제 | 소유권, 재귀 가능 |
| Semaphore | 리소스 카운팅 | N개 동시 접근 |
| Critical Section | 빠른 상호 배제 | 유저모드, 단일 프로세스 |
| SRWLock | 읽기/쓰기 분리 | 빠름, 재귀 불가 |
| Condition Variable | 조건 대기 | SRW/CS와 함께 사용 |
| Interlocked | 원자적 연산 | 락 없이 매우 빠름 |

다음 장에서는 파일 시스템과 I/O에 대해 알아봅니다.
