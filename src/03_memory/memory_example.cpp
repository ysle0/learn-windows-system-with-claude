/**
 * 3장 예제: 메모리 관리
 *
 * VirtualAlloc, HeapAlloc, 메모리 매핑 파일, 메모리 풀 등
 * 게임서버에서 사용되는 메모리 관리 기법을 보여주는 예제입니다.
 *
 * 컴파일: cl /EHsc /W4 memory_example.cpp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

//=============================================================================
// 1. VirtualAlloc 예제
//=============================================================================

void VirtualAllocExample() {
    printf("\n=== VirtualAlloc 예제 ===\n");

    // 1. 한 번에 Reserve + Commit
    printf("\n[1] Reserve + Commit 동시:\n");
    SIZE_T size = 64 * 1024;  // 64KB
    LPVOID pMem = VirtualAlloc(
        NULL,
        size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );

    if (pMem) {
        printf("할당 성공: 주소 = %p, 크기 = %zu KB\n", pMem, size / 1024);

        // 메모리 사용
        memset(pMem, 0xAB, size);
        printf("메모리 쓰기 완료\n");

        // 해제
        VirtualFree(pMem, 0, MEM_RELEASE);
        printf("해제 완료\n");
    }

    // 2. Reserve 후 부분 Commit (대용량 메모리 관리)
    printf("\n[2] Reserve 후 필요시 Commit:\n");
    SIZE_T reserveSize = 100 * 1024 * 1024;  // 100MB 예약

    LPVOID pBase = VirtualAlloc(
        NULL,
        reserveSize,
        MEM_RESERVE,  // 예약만
        PAGE_NOACCESS
    );

    if (pBase) {
        printf("100MB 예약 성공: 주소 = %p\n", pBase);

        // 첫 4KB만 커밋
        LPVOID pCommit1 = VirtualAlloc(
            pBase,
            4096,
            MEM_COMMIT,
            PAGE_READWRITE
        );

        if (pCommit1) {
            printf("첫 4KB 커밋 성공\n");
            *(int*)pCommit1 = 12345;
            printf("값 저장: %d\n", *(int*)pCommit1);
        }

        // 두 번째 4KB 커밋 (오프셋 4096)
        LPVOID pCommit2 = VirtualAlloc(
            (char*)pBase + 4096,
            4096,
            MEM_COMMIT,
            PAGE_READWRITE
        );

        if (pCommit2) {
            printf("두 번째 4KB 커밋 성공\n");
        }

        // 전체 해제
        VirtualFree(pBase, 0, MEM_RELEASE);
        printf("전체 해제 완료\n");
    }

    // 3. 메모리 정보 조회
    printf("\n[3] 메모리 정보 조회:\n");
    pMem = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (pMem) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(pMem, &mbi, sizeof(mbi));

        printf("기본 주소: %p\n", mbi.BaseAddress);
        printf("할당 기본: %p\n", mbi.AllocationBase);
        printf("지역 크기: %zu bytes\n", mbi.RegionSize);
        printf("상태: %s\n",
            mbi.State == MEM_COMMIT ? "Committed" :
            mbi.State == MEM_RESERVE ? "Reserved" : "Free");
        printf("보호: 0x%lX\n", mbi.Protect);

        VirtualFree(pMem, 0, MEM_RELEASE);
    }
}

//=============================================================================
// 2. HeapAlloc 예제
//=============================================================================

void HeapAllocExample() {
    printf("\n=== HeapAlloc 예제 ===\n");

    // 1. 프로세스 기본 힙 사용
    printf("\n[1] 프로세스 기본 힙:\n");
    HANDLE hDefaultHeap = GetProcessHeap();

    void* pMem = HeapAlloc(hDefaultHeap, HEAP_ZERO_MEMORY, 1024);
    if (pMem) {
        printf("기본 힙에서 1KB 할당: %p\n", pMem);

        // 재할당
        void* pNew = HeapReAlloc(hDefaultHeap, HEAP_ZERO_MEMORY, pMem, 2048);
        if (pNew) {
            printf("2KB로 재할당: %p\n", pNew);
            pMem = pNew;
        }

        HeapFree(hDefaultHeap, 0, pMem);
        printf("해제 완료\n");
    }

    // 2. 전용 힙 생성
    printf("\n[2] 전용 힙 생성:\n");
    HANDLE hPrivateHeap = HeapCreate(
        0,          // 옵션 (0 = 기본, HEAP_NO_SERIALIZE = 락 없음)
        1024 * 1024,// 초기 크기 1MB
        0           // 최대 크기 (0 = 무제한)
    );

    if (hPrivateHeap) {
        printf("전용 힙 생성 성공\n");

        // 여러 블록 할당
        void* blocks[10];
        for (int i = 0; i < 10; i++) {
            blocks[i] = HeapAlloc(hPrivateHeap, 0, 100 + i * 10);
            printf("블록 %d: %p (%d bytes)\n", i, blocks[i], 100 + i * 10);
        }

        // 해제
        for (int i = 0; i < 10; i++) {
            HeapFree(hPrivateHeap, 0, blocks[i]);
        }

        // 힙 파괴 (모든 메모리 한 번에 해제)
        HeapDestroy(hPrivateHeap);
        printf("전용 힙 파괴 완료\n");
    }

    // 3. 힙 크기 조회
    printf("\n[3] 할당 크기 조회:\n");
    void* p = HeapAlloc(hDefaultHeap, 0, 100);
    if (p) {
        SIZE_T actualSize = HeapSize(hDefaultHeap, 0, p);
        printf("요청 크기: 100, 실제 크기: %zu\n", actualSize);
        HeapFree(hDefaultHeap, 0, p);
    }
}

//=============================================================================
// 3. 메모리 보호 속성 예제
//=============================================================================

void MemoryProtectionExample() {
    printf("\n=== 메모리 보호 속성 예제 ===\n");

    // 읽기/쓰기 메모리 할당
    char* pMem = (char*)VirtualAlloc(
        NULL,
        4096,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (pMem) {
        printf("PAGE_READWRITE로 할당\n");

        // 쓰기
        strcpy_s(pMem, 4096, "Hello, World!");
        printf("쓰기 성공: %s\n", pMem);

        // 읽기 전용으로 변경
        DWORD oldProtect;
        if (VirtualProtect(pMem, 4096, PAGE_READONLY, &oldProtect)) {
            printf("PAGE_READONLY로 변경 (이전: 0x%lX)\n", oldProtect);
            printf("읽기 성공: %s\n", pMem);

            // 쓰기 시도 (예외 발생!)
            printf("쓰기 시도...\n");
            __try {
                pMem[0] = 'X';  // 예외 발생!
                printf("쓰기 성공 (예상치 못함)\n");
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                printf("예외 발생! (쓰기 거부됨)\n");
            }
        }

        // 다시 읽기/쓰기로 변경
        VirtualProtect(pMem, 4096, PAGE_READWRITE, &oldProtect);

        VirtualFree(pMem, 0, MEM_RELEASE);
    }
}

//=============================================================================
// 4. 메모리 매핑 파일 예제
//=============================================================================

void MemoryMappedFileExample() {
    printf("\n=== 메모리 매핑 파일 예제 ===\n");

    const WCHAR* fileName = L"mapped_file_test.dat";

    // 1. 파일 기반 매핑
    printf("\n[1] 파일 기반 메모리 매핑:\n");

    // 파일 생성
    HANDLE hFile = CreateFileW(
        fileName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        // 파일 크기 설정 (4KB)
        SetFilePointer(hFile, 4096, NULL, FILE_BEGIN);
        SetEndOfFile(hFile);
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

        // 매핑 객체 생성
        HANDLE hMapping = CreateFileMappingW(
            hFile,
            NULL,
            PAGE_READWRITE,
            0, 0,  // 파일 크기 사용
            NULL   // 이름 없음
        );

        if (hMapping) {
            // 뷰 매핑
            int* pView = (int*)MapViewOfFile(
                hMapping,
                FILE_MAP_ALL_ACCESS,
                0, 0, 0
            );

            if (pView) {
                // 데이터 쓰기
                for (int i = 0; i < 100; i++) {
                    pView[i] = i * i;
                }
                printf("100개의 정수 기록\n");

                // 디스크로 플러시
                FlushViewOfFile(pView, 0);
                printf("디스크 플러시 완료\n");

                // 읽기 확인
                printf("첫 10개 값: ");
                for (int i = 0; i < 10; i++) {
                    printf("%d ", pView[i]);
                }
                printf("\n");

                UnmapViewOfFile(pView);
            }

            CloseHandle(hMapping);
        }

        CloseHandle(hFile);
    }

    // 2. 익명 메모리 매핑 (공유 메모리용)
    printf("\n[2] 익명 메모리 매핑 (공유 메모리):\n");

    HANDLE hShared = CreateFileMappingW(
        INVALID_HANDLE_VALUE,  // 파일 없음 (시스템 페이지 파일 사용)
        NULL,
        PAGE_READWRITE,
        0, 4096,
        L"MySharedMemoryTest"  // 이름 지정 (다른 프로세스에서 열 수 있음)
    );

    if (hShared) {
        char* pShared = (char*)MapViewOfFile(
            hShared,
            FILE_MAP_ALL_ACCESS,
            0, 0, 0
        );

        if (pShared) {
            strcpy_s(pShared, 4096, "This is shared memory!");
            printf("공유 메모리에 기록: %s\n", pShared);

            UnmapViewOfFile(pShared);
        }

        CloseHandle(hShared);
    }

    // 테스트 파일 삭제
    DeleteFileW(fileName);
}

//=============================================================================
// 5. 게임서버용 메모리 풀 예제
//=============================================================================

// 간단한 고정 크기 메모리 풀
class FixedSizePool {
private:
    struct Block {
        Block* next;
    };

    CRITICAL_SECTION m_cs;
    Block* m_freeList;
    void* m_poolMemory;
    size_t m_blockSize;
    size_t m_blockCount;
    size_t m_allocCount;

public:
    FixedSizePool(size_t blockSize, size_t blockCount)
        : m_blockSize(max(blockSize, sizeof(Block)))
        , m_blockCount(blockCount)
        , m_allocCount(0)
        , m_freeList(nullptr)
    {
        InitializeCriticalSection(&m_cs);

        // 전체 풀 메모리 할당
        m_poolMemory = VirtualAlloc(
            NULL,
            m_blockSize * m_blockCount,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        if (m_poolMemory) {
            // 프리 리스트 초기화
            char* p = (char*)m_poolMemory;
            for (size_t i = 0; i < m_blockCount - 1; i++) {
                Block* block = (Block*)(p + i * m_blockSize);
                block->next = (Block*)(p + (i + 1) * m_blockSize);
            }
            Block* lastBlock = (Block*)(p + (m_blockCount - 1) * m_blockSize);
            lastBlock->next = nullptr;

            m_freeList = (Block*)m_poolMemory;
        }
    }

    ~FixedSizePool() {
        if (m_poolMemory) {
            VirtualFree(m_poolMemory, 0, MEM_RELEASE);
        }
        DeleteCriticalSection(&m_cs);
    }

    void* Alloc() {
        EnterCriticalSection(&m_cs);

        if (m_freeList == nullptr) {
            LeaveCriticalSection(&m_cs);
            return nullptr;  // 풀 고갈
        }

        Block* block = m_freeList;
        m_freeList = m_freeList->next;
        m_allocCount++;

        LeaveCriticalSection(&m_cs);
        return block;
    }

    void Free(void* ptr) {
        if (ptr == nullptr) return;

        EnterCriticalSection(&m_cs);

        Block* block = (Block*)ptr;
        block->next = m_freeList;
        m_freeList = block;
        m_allocCount--;

        LeaveCriticalSection(&m_cs);
    }

    size_t GetAllocCount() const { return m_allocCount; }
    size_t GetBlockSize() const { return m_blockSize; }
    size_t GetBlockCount() const { return m_blockCount; }
};

// 패킷 구조체 예시
struct Packet {
    DWORD type;
    DWORD size;
    char data[256];
};

void MemoryPoolExample() {
    printf("\n=== 게임서버용 메모리 풀 예제 ===\n");

    // 패킷 풀 생성 (1000개)
    FixedSizePool packetPool(sizeof(Packet), 1000);
    printf("패킷 풀 생성: 블록 크기 = %zu, 개수 = %zu\n",
           packetPool.GetBlockSize(), packetPool.GetBlockCount());

    // 할당 테스트
    Packet* packets[100];
    printf("\n100개 패킷 할당...\n");

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    for (int i = 0; i < 100; i++) {
        packets[i] = (Packet*)packetPool.Alloc();
        if (packets[i]) {
            packets[i]->type = i;
            packets[i]->size = sizeof(Packet);
        }
    }

    QueryPerformanceCounter(&end);
    double poolTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000000;

    printf("풀 할당 완료 (현재 할당: %zu)\n", packetPool.GetAllocCount());

    // 해제 테스트
    printf("\n100개 패킷 해제...\n");
    for (int i = 0; i < 100; i++) {
        packetPool.Free(packets[i]);
    }
    printf("해제 완료 (현재 할당: %zu)\n", packetPool.GetAllocCount());

    // malloc과 비교
    QueryPerformanceCounter(&start);
    for (int i = 0; i < 100; i++) {
        packets[i] = (Packet*)malloc(sizeof(Packet));
        if (packets[i]) {
            packets[i]->type = i;
            packets[i]->size = sizeof(Packet);
        }
    }
    QueryPerformanceCounter(&end);
    double mallocTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000000;

    for (int i = 0; i < 100; i++) {
        free(packets[i]);
    }

    printf("\n성능 비교 (100회 할당):\n");
    printf("  메모리 풀: %.2f us\n", poolTime);
    printf("  malloc:    %.2f us\n", mallocTime);
}

//=============================================================================
// 6. 링 버퍼 예제 (네트워크 버퍼용)
//=============================================================================

class RingBuffer {
private:
    char* m_buffer;
    size_t m_capacity;
    size_t m_head;  // 읽기 위치
    size_t m_tail;  // 쓰기 위치
    CRITICAL_SECTION m_cs;

public:
    RingBuffer(size_t capacity)
        : m_capacity(capacity)
        , m_head(0)
        , m_tail(0)
    {
        InitializeCriticalSection(&m_cs);
        m_buffer = (char*)VirtualAlloc(
            NULL,
            capacity,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );
    }

    ~RingBuffer() {
        if (m_buffer) {
            VirtualFree(m_buffer, 0, MEM_RELEASE);
        }
        DeleteCriticalSection(&m_cs);
    }

    size_t GetUsedSize() const {
        if (m_tail >= m_head) {
            return m_tail - m_head;
        }
        return m_capacity - m_head + m_tail;
    }

    size_t GetFreeSize() const {
        return m_capacity - GetUsedSize() - 1;
    }

    bool Write(const void* data, size_t len) {
        EnterCriticalSection(&m_cs);

        if (len > GetFreeSize()) {
            LeaveCriticalSection(&m_cs);
            return false;  // 공간 부족
        }

        const char* src = (const char*)data;

        // 끝까지 쓸 수 있는 양
        size_t toEnd = m_capacity - m_tail;

        if (len <= toEnd) {
            // 연속으로 쓸 수 있음
            memcpy(m_buffer + m_tail, src, len);
        } else {
            // 두 부분으로 나눠서
            memcpy(m_buffer + m_tail, src, toEnd);
            memcpy(m_buffer, src + toEnd, len - toEnd);
        }

        m_tail = (m_tail + len) % m_capacity;

        LeaveCriticalSection(&m_cs);
        return true;
    }

    size_t Read(void* dest, size_t maxLen) {
        EnterCriticalSection(&m_cs);

        size_t used = GetUsedSize();
        size_t toRead = min(maxLen, used);

        if (toRead == 0) {
            LeaveCriticalSection(&m_cs);
            return 0;
        }

        char* dst = (char*)dest;
        size_t toEnd = m_capacity - m_head;

        if (toRead <= toEnd) {
            memcpy(dst, m_buffer + m_head, toRead);
        } else {
            memcpy(dst, m_buffer + m_head, toEnd);
            memcpy(dst + toEnd, m_buffer, toRead - toEnd);
        }

        m_head = (m_head + toRead) % m_capacity;

        LeaveCriticalSection(&m_cs);
        return toRead;
    }

    // 읽지 않고 들여다보기
    size_t Peek(void* dest, size_t maxLen) {
        EnterCriticalSection(&m_cs);

        size_t used = GetUsedSize();
        size_t toPeek = min(maxLen, used);

        if (toPeek == 0) {
            LeaveCriticalSection(&m_cs);
            return 0;
        }

        char* dst = (char*)dest;
        size_t toEnd = m_capacity - m_head;

        if (toPeek <= toEnd) {
            memcpy(dst, m_buffer + m_head, toPeek);
        } else {
            memcpy(dst, m_buffer + m_head, toEnd);
            memcpy(dst + toEnd, m_buffer, toPeek - toEnd);
        }

        // head 이동 안 함

        LeaveCriticalSection(&m_cs);
        return toPeek;
    }
};

void RingBufferExample() {
    printf("\n=== 링 버퍼 예제 ===\n");

    RingBuffer buffer(1024);
    printf("링 버퍼 생성: 용량 = 1024 bytes\n");

    // 쓰기 테스트
    const char* testData[] = {
        "Hello, ",
        "World! ",
        "This is a ring buffer test. ",
        "IOCP game server uses this pattern."
    };

    printf("\n데이터 쓰기:\n");
    for (int i = 0; i < 4; i++) {
        size_t len = strlen(testData[i]);
        if (buffer.Write(testData[i], len)) {
            printf("  쓰기 성공: '%s' (%zu bytes)\n", testData[i], len);
        }
    }
    printf("사용 중: %zu bytes\n", buffer.GetUsedSize());

    // 읽기 테스트
    printf("\n데이터 읽기:\n");
    char readBuf[256];

    // Peek (읽지 않고 확인)
    size_t peeked = buffer.Peek(readBuf, 20);
    readBuf[peeked] = '\0';
    printf("  Peek (20 bytes): '%s'\n", readBuf);
    printf("  사용 중 (변화 없음): %zu bytes\n", buffer.GetUsedSize());

    // 실제 읽기
    size_t readLen = buffer.Read(readBuf, sizeof(readBuf) - 1);
    readBuf[readLen] = '\0';
    printf("  Read: '%s' (%zu bytes)\n", readBuf, readLen);
    printf("  사용 중: %zu bytes\n", buffer.GetUsedSize());
}

//=============================================================================
// Main
//=============================================================================

int main() {
    printf("========================================\n");
    printf("    메모리 관리 예제\n");
    printf("========================================\n");

    VirtualAllocExample();
    HeapAllocExample();
    MemoryProtectionExample();
    MemoryMappedFileExample();
    MemoryPoolExample();
    RingBufferExample();

    printf("\n========================================\n");
    printf("    모든 예제 완료\n");
    printf("========================================\n");

    return 0;
}
