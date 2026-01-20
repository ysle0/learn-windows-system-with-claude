# 2장: 프로세스와 스레드

## 2.1 프로세스의 개념과 생명주기

### 프로세스란?
프로세스는 **실행 중인 프로그램의 인스턴스**입니다. 단순히 코드(EXE 파일)가 아니라, 실행에 필요한 모든 리소스를 포함합니다.

### 프로세스의 구성 요소
```
┌─────────────────────────────────────────────────────────────┐
│                    프로세스 (Process)                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              가상 주소 공간 (4GB / 128TB)             │   │
│  │  ┌─────────────────────────────────────────────┐    │   │
│  │  │ 코드 영역 (.text)  - 실행 코드               │    │   │
│  │  ├─────────────────────────────────────────────┤    │   │
│  │  │ 데이터 영역 (.data, .bss) - 전역/정적 변수   │    │   │
│  │  ├─────────────────────────────────────────────┤    │   │
│  │  │ 힙 (Heap) - 동적 할당 메모리                 │    │   │
│  │  ├─────────────────────────────────────────────┤    │   │
│  │  │ 스택 (Stack) - 스레드별 지역 변수, 호출 스택  │    │   │
│  │  └─────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────┐  ┌─────────────────────────────────┐  │
│  │   핸들 테이블    │  │      보안 토큰 (Access Token)    │  │
│  │  (파일,소켓 등)  │  │     (사용자 권한 정보)           │  │
│  └─────────────────┘  └─────────────────────────────────┘  │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    스레드들                          │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐             │   │
│  │  │ Main    │  │ Worker  │  │ Worker  │  ...        │   │
│  │  │ Thread  │  │ Thread  │  │ Thread  │             │   │
│  │  └─────────┘  └─────────┘  └─────────┘             │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 프로세스 생명주기
```
┌─────────┐    CreateProcess()    ┌─────────┐
│  생성   │ ───────────────────> │  실행   │
└─────────┘                       └────┬────┘
                                       │
                    ┌──────────────────┴──────────────────┐
                    │                                      │
                    ▼                                      ▼
              ┌─────────┐                           ┌─────────┐
              │  대기   │ <──────────────────────> │  준비   │
              │ (Wait)  │    WaitForSingleObject   │ (Ready) │
              └─────────┘        Signal            └─────────┘
                    │
                    │ ExitProcess() / TerminateProcess()
                    ▼
              ┌─────────┐
              │  종료   │
              │(Zombie) │
              └────┬────┘
                   │ CloseHandle()
                   ▼
              ┌─────────┐
              │  정리   │
              └─────────┘
```

### 프로세스 상태
| 상태 | 설명 |
|------|------|
| New (생성) | CreateProcess() 호출 직후 |
| Ready (준비) | CPU 할당 대기 중 |
| Running (실행) | CPU에서 실행 중 |
| Waiting (대기) | I/O나 동기화 객체 대기 중 |
| Terminated (종료) | 실행 완료, 리소스 정리 대기 |

---

## 2.2 프로세스 생성 (CreateProcess)

### CreateProcess 함수
```c
BOOL CreateProcessW(
    LPCWSTR               lpApplicationName,    // 실행 파일 경로 (선택)
    LPWSTR                lpCommandLine,        // 커맨드 라인 (수정 가능해야 함!)
    LPSECURITY_ATTRIBUTES lpProcessAttributes,  // 프로세스 보안 속성
    LPSECURITY_ATTRIBUTES lpThreadAttributes,   // 스레드 보안 속성
    BOOL                  bInheritHandles,      // 핸들 상속 여부
    DWORD                 dwCreationFlags,      // 생성 플래그
    LPVOID                lpEnvironment,        // 환경 변수 블록
    LPCWSTR               lpCurrentDirectory,   // 작업 디렉토리
    LPSTARTUPINFOW        lpStartupInfo,        // 시작 정보
    LPPROCESS_INFORMATION lpProcessInformation  // [OUT] 프로세스 정보
);
```

### 주요 생성 플래그 (dwCreationFlags)
| 플래그 | 값 | 설명 |
|--------|-----|------|
| CREATE_NEW_CONSOLE | 0x10 | 새 콘솔 창 생성 |
| CREATE_NO_WINDOW | 0x08000000 | 콘솔 창 없이 생성 |
| CREATE_SUSPENDED | 0x04 | 일시 정지 상태로 생성 |
| DETACHED_PROCESS | 0x08 | 부모 콘솔에서 분리 |
| CREATE_NEW_PROCESS_GROUP | 0x200 | 새 프로세스 그룹 |
| NORMAL_PRIORITY_CLASS | 0x20 | 일반 우선순위 |
| HIGH_PRIORITY_CLASS | 0x80 | 높은 우선순위 |

### 프로세스 생성 흐름
```
CreateProcess() 호출
       │
       ▼
┌─────────────────────────────────────┐
│  1. 실행 파일(PE) 로드              │
│     - 이미지 매핑                   │
│     - 의존 DLL 로드                 │
└─────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│  2. 커널 객체 생성                  │
│     - EPROCESS 구조체               │
│     - PEB (Process Environment Block)│
│     - 핸들 테이블                   │
└─────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│  3. 주소 공간 설정                  │
│     - 가상 메모리 공간 할당         │
│     - 코드/데이터 섹션 매핑         │
└─────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│  4. 메인 스레드 생성                │
│     - ETHREAD 구조체                │
│     - TEB (Thread Environment Block) │
│     - 스택 할당                     │
└─────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│  5. 실행 시작                       │
│     - ntdll!LdrInitializeThunk      │
│     - DLL 초기화 (DllMain)          │
│     - 메인 함수 호출                │
└─────────────────────────────────────┘
```

### STARTUPINFO 구조체
```c
typedef struct _STARTUPINFOW {
    DWORD   cb;              // 구조체 크기
    LPWSTR  lpReserved;
    LPWSTR  lpDesktop;       // 데스크탑 이름
    LPWSTR  lpTitle;         // 콘솔 타이틀
    DWORD   dwX, dwY;        // 창 위치
    DWORD   dwXSize, dwYSize;// 창 크기
    DWORD   dwXCountChars;   // 콘솔 버퍼 크기
    DWORD   dwYCountChars;
    DWORD   dwFillAttribute; // 콘솔 색상
    DWORD   dwFlags;         // 유효한 필드 플래그
    WORD    wShowWindow;     // SW_SHOW, SW_HIDE 등
    WORD    cbReserved2;
    LPBYTE  lpReserved2;
    HANDLE  hStdInput;       // 표준 입력 핸들
    HANDLE  hStdOutput;      // 표준 출력 핸들
    HANDLE  hStdError;       // 표준 에러 핸들
} STARTUPINFOW;
```

### PROCESS_INFORMATION 구조체
```c
typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;    // 새 프로세스 핸들
    HANDLE hThread;     // 메인 스레드 핸들
    DWORD  dwProcessId; // 프로세스 ID
    DWORD  dwThreadId;  // 스레드 ID
} PROCESS_INFORMATION;
```

---

## 2.3 스레드의 개념과 생성

### 스레드란?
스레드는 **프로세스 내에서 실행되는 실행 단위**입니다. 하나의 프로세스는 여러 스레드를 가질 수 있습니다.

### 프로세스 vs 스레드
```
┌─────────────────────────────────────────────────────────────────┐
│                          프로세스 A                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    공유 리소스                             │  │
│  │  • 가상 주소 공간 (코드, 데이터, 힙)                       │  │
│  │  • 핸들 테이블                                            │  │
│  │  • 전역 변수                                              │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  스레드 1   │  │  스레드 2   │  │  스레드 3   │             │
│  │             │  │             │  │             │             │
│  │ • 스택      │  │ • 스택      │  │ • 스택      │             │
│  │ • 레지스터  │  │ • 레지스터  │  │ • 레지스터  │             │
│  │ • TLS       │  │ • TLS       │  │ • TLS       │             │
│  │ • 우선순위  │  │ • 우선순위  │  │ • 우선순위  │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘

공유하는 것: 코드, 힙, 전역변수, 핸들
스레드별로 고유한 것: 스택, 레지스터 컨텍스트, TLS
```

### CreateThread 함수
```c
HANDLE CreateThread(
    LPSECURITY_ATTRIBUTES   lpThreadAttributes, // 보안 속성
    SIZE_T                  dwStackSize,        // 스택 크기 (0=기본값 1MB)
    LPTHREAD_START_ROUTINE  lpStartAddress,     // 스레드 함수
    LPVOID                  lpParameter,        // 함수 매개변수
    DWORD                   dwCreationFlags,    // 생성 플래그
    LPDWORD                 lpThreadId          // [OUT] 스레드 ID
);

// 스레드 함수 프로토타입
DWORD WINAPI ThreadProc(LPVOID lpParameter);
```

### 스레드 생성 플래그
| 플래그 | 값 | 설명 |
|--------|-----|------|
| 0 | 0 | 즉시 실행 |
| CREATE_SUSPENDED | 0x04 | 일시 정지 상태로 생성 |
| STACK_SIZE_PARAM_IS_A_RESERVATION | 0x10000 | 스택 크기 예약 |

### 스레드 상태 전이
```
                CreateThread()
                     │
                     ▼
               ┌───────────┐
               │ Initialized│
               └─────┬─────┘
                     │ ResumeThread() (if SUSPENDED)
                     ▼
         ┌─────────────────────┐
         │                     │
         ▼                     │
   ┌───────────┐         ┌───────────┐
   │   Ready   │ <────── │  Running  │
   │  (준비)   │ ──────> │  (실행)   │
   └───────────┘  스케줄  └─────┬─────┘
         ▲                     │
         │                     │ WaitForXxx()
         │    Signal           ▼
         │              ┌───────────┐
         └───────────── │  Waiting  │
                        │  (대기)   │
                        └───────────┘
                              │
         ExitThread() /       │
         TerminateThread()    │
                              ▼
                        ┌───────────┐
                        │Terminated │
                        │  (종료)   │
                        └───────────┘
```

### _beginthreadex vs CreateThread
```c
// C 런타임 라이브러리 사용 시 _beginthreadex 권장
#include <process.h>

unsigned __stdcall ThreadFunc(void* arg) {
    // C 런타임 함수 안전하게 사용 가능
    printf("Thread running\n");
    return 0;
}

int main() {
    unsigned threadId;
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,           // 보안 속성
        0,              // 스택 크기
        ThreadFunc,     // 스레드 함수
        NULL,           // 매개변수
        0,              // 플래그
        &threadId       // 스레드 ID
    );

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    return 0;
}
```

| 비교 항목 | CreateThread | _beginthreadex |
|-----------|--------------|----------------|
| C 런타임 지원 | 불완전 (메모리 누수 가능) | 완전 지원 |
| TLS 초기화 | 부분적 | 완전 |
| errno | 공유됨 (위험) | 스레드별 |
| 권장 상황 | 순수 Win32 API만 사용 | C/C++ 런타임 함수 사용 |

---

## 2.4 스레드 동기화 기초

### 동기화가 필요한 이유
```c
// 동기화 없이 공유 자원 접근 - 문제 발생!
int g_counter = 0;

DWORD WINAPI IncrementThread(LPVOID param) {
    for (int i = 0; i < 100000; i++) {
        g_counter++;  // 읽기 → 증가 → 쓰기 (3단계, 원자적이지 않음!)
    }
    return 0;
}

// 예상: g_counter = 200000
// 실제: g_counter < 200000 (경쟁 상태로 인한 손실)
```

### 경쟁 상태 (Race Condition)
```
스레드 A                          스레드 B
    │                                │
    │ g_counter 읽기 (값: 100)       │
    │                                │ g_counter 읽기 (값: 100)
    │                                │
    │ 100 + 1 = 101 계산             │ 100 + 1 = 101 계산
    │                                │
    │ g_counter에 101 저장           │
    │                                │ g_counter에 101 저장
    │                                │
    ▼                                ▼
        결과: g_counter = 101 (기대값 102)
```

### Critical Section (임계 영역)
가장 가볍고 빠른 동기화 방법입니다. **같은 프로세스 내의 스레드만** 동기화 가능합니다.

```c
CRITICAL_SECTION g_cs;
int g_counter = 0;

// 초기화 (프로그램 시작 시)
InitializeCriticalSection(&g_cs);

DWORD WINAPI SafeIncrementThread(LPVOID param) {
    for (int i = 0; i < 100000; i++) {
        EnterCriticalSection(&g_cs);  // 잠금
        g_counter++;                   // 안전한 접근
        LeaveCriticalSection(&g_cs);  // 잠금 해제
    }
    return 0;
}

// 정리 (프로그램 종료 시)
DeleteCriticalSection(&g_cs);
```

### Critical Section 동작 원리
```
┌─────────────────────────────────────────────────────────────┐
│                    CRITICAL_SECTION                          │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ LockCount: 스핀 카운트 및 잠금 상태                    │  │
│  │ OwningThread: 현재 소유 스레드 ID                     │  │
│  │ RecursionCount: 재귀 잠금 횟수                        │  │
│  │ LockSemaphore: 대기용 이벤트                          │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘

EnterCriticalSection():
    1. 스핀락으로 빠른 시도
    2. 실패하면 커널 모드 대기 (이벤트)

LeaveCriticalSection():
    1. 소유권 해제
    2. 대기 중인 스레드 깨움
```

### Mutex (뮤텍스)
**프로세스 간** 동기화가 가능한 커널 객체입니다.

```c
// 뮤텍스 생성
HANDLE hMutex = CreateMutexW(
    NULL,               // 보안 속성
    FALSE,              // 초기 소유권 (FALSE = 소유하지 않음)
    L"MyGameMutex"      // 이름 (NULL = 익명)
);

// 뮤텍스 획득 (대기)
DWORD result = WaitForSingleObject(hMutex, INFINITE);
if (result == WAIT_OBJECT_0) {
    // 임계 영역 - 안전하게 작업
    g_counter++;

    // 뮤텍스 해제
    ReleaseMutex(hMutex);
}

// 정리
CloseHandle(hMutex);
```

### Critical Section vs Mutex
| 특성 | Critical Section | Mutex |
|------|------------------|-------|
| 범위 | 단일 프로세스 | 프로세스 간 |
| 성능 | 빠름 (유저 모드 우선) | 느림 (항상 커널 전환) |
| 타임아웃 | 불가 (TryEnter만) | 가능 |
| 이름 | 불가 | 가능 |
| 소유권 | 스레드 | 스레드 |
| 재귀 잠금 | 가능 | 가능 |

---

## 2.5 스레드 풀 (Thread Pool)

### 스레드 풀이란?
미리 생성된 스레드 집합을 재사용하여 작업을 처리하는 메커니즘입니다.

### 스레드 풀의 장점
```
직접 스레드 관리:
┌─────────────────────────────────────────────────────────────┐
│  작업 1  →  CreateThread()  →  실행  →  종료  →  CloseHandle()  │
│  작업 2  →  CreateThread()  →  실행  →  종료  →  CloseHandle()  │
│  작업 3  →  CreateThread()  →  실행  →  종료  →  CloseHandle()  │
│                                                             │
│  문제: 매번 생성/종료 오버헤드, 스레드 수 무제한 증가 가능   │
└─────────────────────────────────────────────────────────────┘

스레드 풀 사용:
┌─────────────────────────────────────────────────────────────┐
│                      스레드 풀                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  스레드 1: 대기 → 작업1 실행 → 대기 → 작업4 실행 → 대기  │   │
│  │  스레드 2: 대기 → 작업2 실행 → 대기 → 작업5 실행 → 대기  │   │
│  │  스레드 3: 대기 → 작업3 실행 → 대기 → ...             │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  장점: 생성/종료 오버헤드 없음, 스레드 수 제어              │
└─────────────────────────────────────────────────────────────┘
```

### Windows 스레드 풀 API
```c
// 작업 콜백 함수
VOID CALLBACK WorkCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID                 Context,
    PTP_WORK              Work
);

// 사용 예
void UseThreadPool() {
    // 1. 작업 생성
    PTP_WORK pWork = CreateThreadpoolWork(
        WorkCallback,   // 콜백 함수
        NULL,           // 컨텍스트
        NULL            // 환경 (NULL = 기본 풀)
    );

    // 2. 작업 제출
    SubmitThreadpoolWork(pWork);

    // 3. 완료 대기
    WaitForThreadpoolWorkCallbacks(pWork, FALSE);

    // 4. 정리
    CloseThreadpoolWork(pWork);
}
```

### 커스텀 스레드 풀 설정
```c
// 풀 생성
PTP_POOL pPool = CreateThreadpool(NULL);

// 최소/최대 스레드 수 설정
SetThreadpoolThreadMinimum(pPool, 2);
SetThreadpoolThreadMaximum(pPool, 8);

// 콜백 환경 설정
TP_CALLBACK_ENVIRON callbackEnv;
InitializeThreadpoolEnvironment(&callbackEnv);
SetThreadpoolCallbackPool(&callbackEnv, pPool);

// 작업 생성 시 환경 지정
PTP_WORK pWork = CreateThreadpoolWork(WorkCallback, NULL, &callbackEnv);

// 정리
DestroyThreadpoolEnvironment(&callbackEnv);
CloseThreadpool(pPool);
```

### 게임서버에서의 스레드 풀 활용
```
┌─────────────────────────────────────────────────────────────┐
│                      게임 서버 구조                          │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                   Accept 스레드                      │   │
│  │              (클라이언트 연결 수락)                   │   │
│  └──────────────────────┬──────────────────────────────┘   │
│                         │ 새 연결                          │
│                         ▼                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              IOCP (완료 큐)                          │   │
│  │          (비동기 I/O 완료 이벤트)                    │   │
│  └──────────────────────┬──────────────────────────────┘   │
│                         │ 완료 통보                        │
│                         ▼                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │               Worker 스레드 풀                       │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐             │   │
│  │  │Worker 1 │  │Worker 2 │  │Worker 3 │  ...        │   │
│  │  │         │  │         │  │         │             │   │
│  │  │ 패킷    │  │ 패킷    │  │ 패킷    │             │   │
│  │  │ 처리    │  │ 처리    │  │ 처리    │             │   │
│  │  └─────────┘  └─────────┘  └─────────┘             │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 게임서버 관점에서의 핵심 포인트

### 1. 스레드 개수 결정
```c
// CPU 코어 수 확인
SYSTEM_INFO si;
GetSystemInfo(&si);
DWORD numCores = si.dwNumberOfProcessors;

// IOCP Worker 스레드: 일반적으로 코어 수 * 2
DWORD numWorkers = numCores * 2;

// 왜 * 2?
// - I/O 대기 중인 스레드가 있을 수 있음
// - 스레드가 대기 상태가 되면 다른 스레드가 즉시 실행
```

### 2. 스레드 친화성 (Affinity)
```c
// 특정 스레드를 특정 CPU 코어에 바인딩
HANDLE hThread = GetCurrentThread();
SetThreadAffinityMask(hThread, 1 << coreIndex);  // 코어 N에 바인딩

// 게임서버에서 사용 예:
// - 네트워크 스레드: 코어 0-1
// - 게임 로직 스레드: 코어 2-3
// - DB 스레드: 코어 4-5
```

### 3. 스레드 우선순위
```c
// 우선순위 설정
SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);

// 우선순위 레벨
// THREAD_PRIORITY_HIGHEST      (+2)
// THREAD_PRIORITY_ABOVE_NORMAL (+1)
// THREAD_PRIORITY_NORMAL       ( 0)  기본
// THREAD_PRIORITY_BELOW_NORMAL (-1)
// THREAD_PRIORITY_LOWEST       (-2)
```

### 4. 스레드 안전한 데이터 구조
```c
// 게임서버의 전형적인 스레드 안전 패턴
class PlayerManager {
private:
    CRITICAL_SECTION m_cs;
    std::map<UINT64, Player*> m_players;

public:
    PlayerManager() {
        InitializeCriticalSection(&m_cs);
    }

    ~PlayerManager() {
        DeleteCriticalSection(&m_cs);
    }

    void AddPlayer(UINT64 id, Player* player) {
        EnterCriticalSection(&m_cs);
        m_players[id] = player;
        LeaveCriticalSection(&m_cs);
    }

    Player* GetPlayer(UINT64 id) {
        EnterCriticalSection(&m_cs);
        auto it = m_players.find(id);
        Player* result = (it != m_players.end()) ? it->second : nullptr;
        LeaveCriticalSection(&m_cs);
        return result;
    }
};
```

---

## 정리

| 개념 | 핵심 내용 |
|------|-----------|
| 프로세스 | 실행 중인 프로그램, 독립된 주소 공간 |
| 스레드 | 프로세스 내 실행 단위, 리소스 공유 |
| CreateProcess | 새 프로세스 생성, STARTUPINFO로 설정 |
| CreateThread | 새 스레드 생성, _beginthreadex 권장 |
| Critical Section | 빠른 동기화, 단일 프로세스용 |
| Mutex | 프로세스 간 동기화, 이름 지정 가능 |
| 스레드 풀 | 스레드 재사용으로 오버헤드 감소 |

다음 장에서는 메모리 관리에 대해 알아봅니다.
