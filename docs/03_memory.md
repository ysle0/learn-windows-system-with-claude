# 3장: 메모리 관리

## 3.1 가상 메모리 개념

### 가상 메모리란?
가상 메모리는 각 프로세스에게 **독립적인 연속된 주소 공간**을 제공하는 메모리 관리 기법입니다.

### 가상 주소 공간
```
32비트 프로세스 (4GB):                 64비트 프로세스 (128TB):

┌───────────────────┐ 0xFFFFFFFF      ┌───────────────────┐ 0x7FFFFFFFFFFF
│                   │                  │                   │
│    커널 영역      │ 2GB              │    커널 영역      │
│  (시스템 전용)    │                  │  (시스템 전용)    │
│                   │                  │                   │
├───────────────────┤ 0x80000000      ├───────────────────┤
│                   │                  │                   │
│    유저 영역      │                  │    유저 영역      │
│  (응용 프로그램)  │ 2GB              │  (응용 프로그램)  │ 128TB
│                   │                  │                   │
│   - 코드          │                  │   - 코드          │
│   - 데이터        │                  │   - 데이터        │
│   - 힙            │                  │   - 힙            │
│   - 스택          │                  │   - 스택          │
│   - DLL           │                  │   - DLL           │
│                   │                  │                   │
└───────────────────┘ 0x00000000      └───────────────────┘ 0x000000000000
```

### 가상 → 물리 주소 변환
```
┌─────────────────────────────────────────────────────────────────────┐
│                          가상 메모리                                 │
│                                                                     │
│  프로세스 A           프로세스 B           프로세스 C               │
│  ┌─────────┐          ┌─────────┐          ┌─────────┐             │
│  │ 0x1000  │──┐       │ 0x1000  │──┐       │ 0x1000  │──┐          │
│  │ 0x2000  │─┐│       │ 0x2000  │─┐│       │ 0x2000  │─┐│          │
│  │ 0x3000  │┐││       │ 0x3000  │┐││       │ 0x3000  │┐││          │
│  └─────────┘│││       └─────────┘│││       └─────────┘│││          │
│             │││                  │││                  │││          │
└─────────────┼┼┼──────────────────┼┼┼──────────────────┼┼┼──────────┘
              │││                  │││                  │││
              │││  페이지 테이블    │││                  │││
              │││    (MMU)         │││                  │││
              │││                  │││                  │││
┌─────────────┼┼┼──────────────────┼┼┼──────────────────┼┼┼──────────┐
│             ▼▼▼                  ▼▼▼                  ▼▼▼          │
│                          물리 메모리 (RAM)                         │
│  ┌───────────────────────────────────────────────────────────┐    │
│  │ 0x10000 │ 0x20000 │ 0x30000 │ 0x40000 │ 0x50000 │  ...    │    │
│  └───────────────────────────────────────────────────────────┘    │
│                                                                   │
│  같은 가상 주소라도 다른 물리 주소에 매핑됨                        │
└───────────────────────────────────────────────────────────────────┘
```

### 페이지와 페이지 테이블
```
가상 주소 (32비트 예시):
┌──────────────────────────────────────────────────────────┐
│ 31        22 │ 21        12 │ 11                       0 │
├──────────────┼──────────────┼────────────────────────────┤
│ Page Dir     │ Page Table   │ Offset                     │
│ Index (10b)  │ Index (10b)  │ (12 bits = 4KB)            │
└──────────────┴──────────────┴────────────────────────────┘

주소 변환 과정:
┌─────────────────┐
│ 가상 주소       │
│ 0x12345678      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐    ┌─────────────────┐
│ Page Directory  │───>│ Page Table      │
│ Entry [0x48]    │    │ Entry [0x345]   │
└─────────────────┘    └────────┬────────┘
                                │
                                ▼
                       ┌─────────────────┐
                       │ 물리 주소       │
                       │ = PFN + 0x678   │
                       └─────────────────┘
```

### 페이지 상태
| 상태 | 설명 |
|------|------|
| Free | 사용되지 않음, 할당 가능 |
| Reserved | 예약됨, 물리 메모리 미할당 |
| Committed | 커밋됨, 물리 메모리 할당됨 |
| Page File | 디스크로 스왑됨 |

---

## 3.2 메모리 할당 (VirtualAlloc, HeapAlloc, malloc)

### 메모리 할당 계층
```
┌─────────────────────────────────────────────────────────────────┐
│                     응용 프로그램                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐        │
│   │   malloc    │    │    new      │    │ LocalAlloc  │        │
│   │   free      │    │   delete    │    │ LocalFree   │        │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘        │
│          │                  │                  │                │
│          └─────────────────┬┴──────────────────┘                │
│                            ▼                                    │
│                    ┌───────────────┐                            │
│                    │   HeapAlloc   │ ← 힙 관리자                │
│                    │   HeapFree    │   (프로세스 기본 힙)       │
│                    └───────┬───────┘                            │
│                            │                                    │
│                            ▼                                    │
│                    ┌───────────────┐                            │
│                    │ VirtualAlloc  │ ← 가상 메모리 API          │
│                    │ VirtualFree   │   (페이지 단위 할당)       │
│                    └───────┬───────┘                            │
│                            │                                    │
├────────────────────────────┼────────────────────────────────────┤
│                            ▼                    커널 모드       │
│                    ┌───────────────┐                            │
│                    │ Memory Manager│                            │
│                    │ (ntoskrnl.exe)│                            │
│                    └───────────────┘                            │
└─────────────────────────────────────────────────────────────────┘
```

### VirtualAlloc - 저수준 메모리 할당
```c
LPVOID VirtualAlloc(
    LPVOID lpAddress,          // 원하는 주소 (NULL = 시스템이 결정)
    SIZE_T dwSize,             // 크기
    DWORD  flAllocationType,   // 할당 타입
    DWORD  flProtect           // 보호 속성
);

// 할당 타입
MEM_RESERVE    // 주소 공간 예약 (물리 메모리 할당 안 함)
MEM_COMMIT     // 물리 메모리 커밋
MEM_RESET      // 내용 버려도 됨 (최적화 힌트)
MEM_LARGE_PAGES// 대형 페이지 사용

// 보호 속성
PAGE_READONLY           // 읽기 전용
PAGE_READWRITE          // 읽기/쓰기
PAGE_EXECUTE            // 실행 전용
PAGE_EXECUTE_READ       // 실행/읽기
PAGE_EXECUTE_READWRITE  // 실행/읽기/쓰기
PAGE_NOACCESS           // 접근 불가
PAGE_GUARD              // 가드 페이지 (첫 접근 시 예외)
```

### VirtualAlloc 사용 패턴
```c
// 패턴 1: 한 번에 예약 + 커밋
void* pMem = VirtualAlloc(
    NULL,
    1024 * 1024,  // 1MB
    MEM_RESERVE | MEM_COMMIT,
    PAGE_READWRITE
);

// 패턴 2: 예약 후 필요할 때 커밋 (대용량 메모리)
// 1. 큰 주소 공간 예약
void* pBase = VirtualAlloc(
    NULL,
    100 * 1024 * 1024,  // 100MB 예약
    MEM_RESERVE,
    PAGE_NOACCESS
);

// 2. 필요한 부분만 커밋
VirtualAlloc(
    pBase,              // 예약된 영역 내 주소
    4096,               // 4KB 커밋
    MEM_COMMIT,
    PAGE_READWRITE
);

// 해제
VirtualFree(pMem, 0, MEM_RELEASE);
```

### HeapAlloc - 힙 메모리 할당
```c
// 프로세스 기본 힙 사용
HANDLE hHeap = GetProcessHeap();

void* pMem = HeapAlloc(
    hHeap,
    HEAP_ZERO_MEMORY,  // 0으로 초기화
    1024               // 크기
);

HeapFree(hHeap, 0, pMem);

// 전용 힙 생성 (스레드 안전)
HANDLE hPrivateHeap = HeapCreate(
    0,          // 옵션 (HEAP_NO_SERIALIZE = 락 없음, 단일 스레드용)
    0,          // 초기 크기 (0 = 시스템 기본값)
    0           // 최대 크기 (0 = 확장 가능)
);

void* pData = HeapAlloc(hPrivateHeap, 0, 256);
HeapFree(hPrivateHeap, 0, pData);
HeapDestroy(hPrivateHeap);
```

### 할당 함수 비교
| 함수 | 특징 | 사용 시기 |
|------|------|-----------|
| VirtualAlloc | 페이지 단위, 보호 속성 제어 | 대용량, 메모리 보호 필요 |
| HeapAlloc | 작은 단위, 빠름 | 일반적인 동적 할당 |
| malloc | C 표준, 이식성 | 이식 가능한 코드 |
| new | C++, 생성자 호출 | C++ 객체 할당 |
| LocalAlloc | 레거시 | 호환성 (사용 자제) |
| GlobalAlloc | 레거시 | 클립보드 등 특수 목적 |

---

## 3.3 메모리 보호 속성

### 보호 속성 변경
```c
BOOL VirtualProtect(
    LPVOID lpAddress,           // 대상 주소
    SIZE_T dwSize,              // 크기
    DWORD  flNewProtect,        // 새 보호 속성
    PDWORD lpflOldProtect       // [OUT] 이전 보호 속성
);

// 예: 실행 가능 메모리 생성
void* pCode = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE,
                           PAGE_READWRITE);

// 코드 복사...
memcpy(pCode, shellcode, sizeof(shellcode));

// 실행 가능으로 변경
DWORD oldProtect;
VirtualProtect(pCode, 4096, PAGE_EXECUTE_READ, &oldProtect);

// 이제 실행 가능
typedef void (*FuncPtr)();
FuncPtr func = (FuncPtr)pCode;
func();
```

### DEP (Data Execution Prevention)
```
┌─────────────────────────────────────────────────────────────────┐
│                    DEP 활성화 시                                 │
│                                                                 │
│  ┌─────────────────┐                                            │
│  │ 코드 영역       │ PAGE_EXECUTE_READ                          │
│  │ 실행 가능 ✓     │                                            │
│  ├─────────────────┤                                            │
│  │ 데이터 영역     │ PAGE_READWRITE                             │
│  │ 실행 불가 ✗     │ ← 실행 시도하면 예외 발생!                 │
│  ├─────────────────┤                                            │
│  │ 힙              │ PAGE_READWRITE                             │
│  │ 실행 불가 ✗     │                                            │
│  ├─────────────────┤                                            │
│  │ 스택            │ PAGE_READWRITE                             │
│  │ 실행 불가 ✗     │ ← 버퍼 오버플로우 공격 방지                │
│  └─────────────────┘                                            │
└─────────────────────────────────────────────────────────────────┘
```

### 가드 페이지
```c
// 가드 페이지: 첫 접근 시 예외 발생
void* pGuarded = VirtualAlloc(NULL, 4096,
    MEM_COMMIT | MEM_RESERVE,
    PAGE_READWRITE | PAGE_GUARD
);

__try {
    // 첫 접근 시 STATUS_GUARD_PAGE_VIOLATION 예외 발생
    *(int*)pGuarded = 42;
}
__except (GetExceptionCode() == STATUS_GUARD_PAGE_VIOLATION ?
          EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    printf("가드 페이지 접근 감지!\n");
    // 가드 속성은 자동으로 제거됨
}

// 이후 접근은 정상
*(int*)pGuarded = 100;  // OK
```

---

## 3.4 메모리 매핑 파일 (Memory-Mapped Files)

### 개념
파일을 메모리에 매핑하여 마치 메모리처럼 파일에 접근하는 기법입니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                    일반 파일 I/O                                 │
│                                                                 │
│  프로그램                    OS                    디스크       │
│  ┌─────────┐              ┌─────────┐           ┌─────────┐    │
│  │ buffer  │◄─ReadFile───│ 커널    │◄──────────│  파일   │    │
│  └─────────┘  (복사!)     │ 버퍼    │  (읽기)   └─────────┘    │
│                          └─────────┘                            │
│                                                                 │
│  문제: 데이터가 커널 버퍼 → 유저 버퍼로 복사됨 (오버헤드)       │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    메모리 매핑 파일                              │
│                                                                 │
│  프로그램                    OS                    디스크       │
│  ┌─────────┐              ┌─────────┐           ┌─────────┐    │
│  │ view    │─────────────>│ 페이지  │◄──────────│  파일   │    │
│  │ (매핑)  │  직접 참조!  │ 캐시    │  (필요시) └─────────┘    │
│  └─────────┘              └─────────┘                            │
│                                                                 │
│  장점: 복사 없이 페이지 캐시 직접 접근 (Zero-copy)              │
└─────────────────────────────────────────────────────────────────┘
```

### 메모리 매핑 API 사용
```c
// 1. 파일 열기
HANDLE hFile = CreateFileW(
    L"data.bin",
    GENERIC_READ | GENERIC_WRITE,
    0, NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL
);

// 2. 파일 매핑 객체 생성
HANDLE hMapping = CreateFileMappingW(
    hFile,
    NULL,                   // 보안 속성
    PAGE_READWRITE,         // 보호 속성
    0, 0,                   // 최대 크기 (0 = 파일 크기)
    NULL                    // 이름 (프로세스 간 공유 시 지정)
);

// 3. 뷰 매핑
LPVOID pView = MapViewOfFile(
    hMapping,
    FILE_MAP_ALL_ACCESS,    // 접근 권한
    0, 0,                   // 오프셋 (상위, 하위)
    0                       // 크기 (0 = 전체)
);

// 4. 메모리처럼 사용
int* pData = (int*)pView;
pData[0] = 12345;           // 파일에 직접 쓰기!
int value = pData[1];       // 파일에서 직접 읽기!

// 5. 명시적 플러시 (선택적)
FlushViewOfFile(pView, 0);

// 6. 정리
UnmapViewOfFile(pView);
CloseHandle(hMapping);
CloseHandle(hFile);
```

### 대형 파일 부분 매핑
```c
// 10GB 파일을 부분적으로 매핑
HANDLE hFile = CreateFileW(L"huge_file.dat", ...);
HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

// 1GB 청크씩 처리
const SIZE_T CHUNK_SIZE = 1024 * 1024 * 1024;  // 1GB
ULONGLONG fileOffset = 0;

while (fileOffset < fileSize) {
    // 오프셋의 상위/하위 32비트
    DWORD offsetHigh = (DWORD)(fileOffset >> 32);
    DWORD offsetLow = (DWORD)(fileOffset & 0xFFFFFFFF);

    LPVOID pChunk = MapViewOfFile(
        hMapping,
        FILE_MAP_READ,
        offsetHigh, offsetLow,
        CHUNK_SIZE
    );

    // 청크 처리...
    ProcessChunk(pChunk, CHUNK_SIZE);

    UnmapViewOfFile(pChunk);
    fileOffset += CHUNK_SIZE;
}
```

---

## 3.5 프로세스 간 메모리 공유

### 이름 있는 메모리 매핑으로 공유
```c
//=== 서버 프로세스 ===
// 공유 메모리 생성
HANDLE hMapping = CreateFileMappingW(
    INVALID_HANDLE_VALUE,   // 파일 대신 시스템 페이지 파일 사용
    NULL,
    PAGE_READWRITE,
    0, 4096,                // 4KB 공유 영역
    L"MySharedMemory"       // 이름 (중요!)
);

LPVOID pShared = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

// 데이터 쓰기
strcpy((char*)pShared, "Hello from Server!");

//=== 클라이언트 프로세스 ===
// 기존 공유 메모리 열기
HANDLE hMapping = OpenFileMappingW(
    FILE_MAP_ALL_ACCESS,
    FALSE,
    L"MySharedMemory"       // 같은 이름!
);

LPVOID pShared = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);

// 데이터 읽기
printf("Received: %s\n", (char*)pShared);  // "Hello from Server!"
```

### 공유 메모리 구조
```
┌─────────────────────────────────────────────────────────────────┐
│                        물리 메모리                               │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              공유 메모리 페이지                            │  │
│  │                   0x12345000                               │  │
│  └───────────────────────────────────────────────────────────┘  │
│                     ▲                    ▲                      │
│                     │                    │                      │
│            ┌────────┴────────┐  ┌────────┴────────┐             │
│            │                 │  │                 │             │
│  ┌─────────┴─────────┐    ┌──┴──────────────┐                  │
│  │    프로세스 A      │    │   프로세스 B     │                  │
│  │  가상 주소        │    │  가상 주소       │                  │
│  │  0x00400000       │    │  0x00500000      │                  │
│  │       │           │    │       │          │                  │
│  │       ▼           │    │       ▼          │                  │
│  │  pShared 접근     │    │  pShared 접근    │                  │
│  └───────────────────┘    └──────────────────┘                  │
│                                                                 │
│  동일한 물리 페이지를 다른 가상 주소에 매핑                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 게임서버 관점에서의 메모리 관리

### 1. 메모리 풀 (Object Pool)
```c
// 게임서버에서 자주 할당/해제되는 객체용 메모리 풀
template<typename T, size_t POOL_SIZE = 1000>
class ObjectPool {
private:
    struct Node {
        T object;
        Node* next;
    };

    CRITICAL_SECTION m_cs;
    Node* m_freeList;
    Node* m_pool;

public:
    ObjectPool() {
        InitializeCriticalSection(&m_cs);

        // 풀 메모리 할당 (VirtualAlloc으로 대형 할당)
        m_pool = (Node*)VirtualAlloc(
            NULL,
            sizeof(Node) * POOL_SIZE,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        // 프리 리스트 구성
        m_freeList = m_pool;
        for (size_t i = 0; i < POOL_SIZE - 1; i++) {
            m_pool[i].next = &m_pool[i + 1];
        }
        m_pool[POOL_SIZE - 1].next = nullptr;
    }

    ~ObjectPool() {
        DeleteCriticalSection(&m_cs);
        VirtualFree(m_pool, 0, MEM_RELEASE);
    }

    T* Alloc() {
        EnterCriticalSection(&m_cs);
        if (m_freeList == nullptr) {
            LeaveCriticalSection(&m_cs);
            return nullptr;  // 풀 고갈
        }
        Node* node = m_freeList;
        m_freeList = m_freeList->next;
        LeaveCriticalSection(&m_cs);
        return &node->object;
    }

    void Free(T* obj) {
        Node* node = (Node*)obj;
        EnterCriticalSection(&m_cs);
        node->next = m_freeList;
        m_freeList = node;
        LeaveCriticalSection(&m_cs);
    }
};

// 사용 예
ObjectPool<Packet> g_packetPool;

Packet* pkt = g_packetPool.Alloc();
// 사용...
g_packetPool.Free(pkt);
```

### 2. 링 버퍼 (Ring Buffer)
```c
// 네트워크 송수신용 링 버퍼
class RingBuffer {
private:
    char* m_buffer;
    size_t m_size;
    size_t m_head;  // 읽기 위치
    size_t m_tail;  // 쓰기 위치
    CRITICAL_SECTION m_cs;

public:
    RingBuffer(size_t size) : m_size(size), m_head(0), m_tail(0) {
        InitializeCriticalSection(&m_cs);
        // 페이지 경계 정렬 할당
        m_buffer = (char*)VirtualAlloc(
            NULL, size,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );
    }

    ~RingBuffer() {
        DeleteCriticalSection(&m_cs);
        VirtualFree(m_buffer, 0, MEM_RELEASE);
    }

    size_t GetUsedSize() {
        return (m_tail - m_head + m_size) % m_size;
    }

    size_t GetFreeSize() {
        return m_size - GetUsedSize() - 1;
    }

    bool Write(const char* data, size_t len) {
        EnterCriticalSection(&m_cs);
        if (len > GetFreeSize()) {
            LeaveCriticalSection(&m_cs);
            return false;
        }

        // 데이터 복사 (경계 처리)
        size_t firstPart = min(len, m_size - m_tail);
        memcpy(m_buffer + m_tail, data, firstPart);
        if (len > firstPart) {
            memcpy(m_buffer, data + firstPart, len - firstPart);
        }
        m_tail = (m_tail + len) % m_size;

        LeaveCriticalSection(&m_cs);
        return true;
    }

    size_t Read(char* dest, size_t maxLen) {
        EnterCriticalSection(&m_cs);
        size_t toRead = min(maxLen, GetUsedSize());

        size_t firstPart = min(toRead, m_size - m_head);
        memcpy(dest, m_buffer + m_head, firstPart);
        if (toRead > firstPart) {
            memcpy(dest + firstPart, m_buffer, toRead - firstPart);
        }
        m_head = (m_head + toRead) % m_size;

        LeaveCriticalSection(&m_cs);
        return toRead;
    }
};
```

### 3. 메모리 정렬과 캐시 최적화
```c
// 캐시 라인 정렬 (일반적으로 64바이트)
#define CACHE_LINE_SIZE 64

// 캐시 라인 정렬된 구조체
__declspec(align(CACHE_LINE_SIZE))
struct AlignedData {
    volatile LONG counter;
    char padding[CACHE_LINE_SIZE - sizeof(LONG)];  // False sharing 방지
};

// 여러 스레드가 접근하는 카운터들
struct ServerStats {
    AlignedData packetsSent;      // 각각 다른 캐시 라인에 위치
    AlignedData packetsReceived;
    AlignedData bytesTransferred;
};
```

---

## 정리

| 개념 | 핵심 내용 |
|------|-----------|
| 가상 메모리 | 프로세스별 독립된 주소 공간 |
| VirtualAlloc | 페이지 단위 할당, 보호 속성 제어 |
| HeapAlloc | 작은 블록 할당, 힙 관리자 |
| 메모리 매핑 | 파일을 메모리처럼 접근, Zero-copy |
| 공유 메모리 | 프로세스 간 데이터 공유 |
| 메모리 풀 | 잦은 할당/해제 오버헤드 감소 |

다음 장에서는 동기화 객체에 대해 알아봅니다.
