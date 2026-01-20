/**
 * 4장 예제: 동기화 객체
 *
 * Event, Mutex, Semaphore, SRWLock, Condition Variable 등
 * 다양한 동기화 기법을 보여주는 예제입니다.
 *
 * 컴파일: cl /EHsc /W4 sync_example.cpp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <queue>

//=============================================================================
// 1. Event 예제
//=============================================================================

HANDLE g_hStartEvent = NULL;   // 수동 리셋 (브로드캐스트)
HANDLE g_hDoneEvent = NULL;    // 자동 리셋 (1:1 통신)

unsigned __stdcall EventWorker(void* param) {
    int id = *(int*)param;

    printf("[Worker %d] 시작 신호 대기 중...\n", id);
    WaitForSingleObject(g_hStartEvent, INFINITE);

    printf("[Worker %d] 작업 시작! (시뮬레이션 %dms)\n", id, 100 + id * 50);
    Sleep(100 + id * 50);

    printf("[Worker %d] 완료!\n", id);
    SetEvent(g_hDoneEvent);

    return 0;
}

void EventExample() {
    printf("\n=== Event 예제 ===\n");

    // 수동 리셋 이벤트: 모든 워커가 동시에 시작
    g_hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    // 자동 리셋 이벤트: 완료 통보
    g_hDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    const int WORKER_COUNT = 3;
    HANDLE hThreads[WORKER_COUNT];
    int ids[WORKER_COUNT];

    for (int i = 0; i < WORKER_COUNT; i++) {
        ids[i] = i + 1;
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, EventWorker, &ids[i], 0, NULL);
    }

    printf("\n[Main] 2초 후 시작 신호...\n");
    Sleep(2000);

    printf("[Main] SetEvent - 모든 워커 시작!\n");
    SetEvent(g_hStartEvent);

    // 각 워커 완료 대기
    for (int i = 0; i < WORKER_COUNT; i++) {
        WaitForSingleObject(g_hDoneEvent, INFINITE);
        printf("[Main] Worker 하나 완료 확인\n");
    }

    WaitForMultipleObjects(WORKER_COUNT, hThreads, TRUE, INFINITE);

    for (int i = 0; i < WORKER_COUNT; i++) {
        CloseHandle(hThreads[i]);
    }
    CloseHandle(g_hStartEvent);
    CloseHandle(g_hDoneEvent);
}

//=============================================================================
// 2. Semaphore 예제 (연결 풀)
//=============================================================================

HANDLE g_hPoolSemaphore = NULL;
CRITICAL_SECTION g_csPool;
int g_connections[5] = { 0 };  // 0 = 사용 가능, 1 = 사용 중

int AcquireConnection() {
    // 세마포어로 사용 가능한 연결 대기
    WaitForSingleObject(g_hPoolSemaphore, INFINITE);

    EnterCriticalSection(&g_csPool);
    int connId = -1;
    for (int i = 0; i < 5; i++) {
        if (g_connections[i] == 0) {
            g_connections[i] = 1;
            connId = i;
            break;
        }
    }
    LeaveCriticalSection(&g_csPool);

    return connId;
}

void ReleaseConnection(int connId) {
    EnterCriticalSection(&g_csPool);
    g_connections[connId] = 0;
    LeaveCriticalSection(&g_csPool);

    // 세마포어 카운트 증가
    ReleaseSemaphore(g_hPoolSemaphore, 1, NULL);
}

unsigned __stdcall PoolWorker(void* param) {
    int workerId = *(int*)param;

    for (int i = 0; i < 3; i++) {
        printf("[Worker %d] 연결 획득 시도...\n", workerId);

        int connId = AcquireConnection();
        printf("[Worker %d] 연결 %d 획득! 작업 중...\n", workerId, connId);

        Sleep(100);  // 작업 시뮬레이션

        printf("[Worker %d] 연결 %d 반환\n", workerId, connId);
        ReleaseConnection(connId);
    }

    return 0;
}

void SemaphoreExample() {
    printf("\n=== Semaphore 예제 (연결 풀) ===\n");

    // 5개 연결 풀
    g_hPoolSemaphore = CreateSemaphore(NULL, 5, 5, NULL);
    InitializeCriticalSection(&g_csPool);

    const int WORKER_COUNT = 8;  // 5개 연결에 8개 워커
    HANDLE hThreads[WORKER_COUNT];
    int ids[WORKER_COUNT];

    printf("연결 풀: 5개, 워커: 8개\n\n");

    for (int i = 0; i < WORKER_COUNT; i++) {
        ids[i] = i + 1;
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, PoolWorker, &ids[i], 0, NULL);
    }

    WaitForMultipleObjects(WORKER_COUNT, hThreads, TRUE, INFINITE);

    for (int i = 0; i < WORKER_COUNT; i++) {
        CloseHandle(hThreads[i]);
    }
    CloseHandle(g_hPoolSemaphore);
    DeleteCriticalSection(&g_csPool);
}

//=============================================================================
// 3. Mutex 예제 (단일 인스턴스)
//=============================================================================

void MutexExample() {
    printf("\n=== Mutex 예제 (단일 인스턴스) ===\n");

    const WCHAR* mutexName = L"Global\\MySingleInstanceMutex";

    HANDLE hMutex = CreateMutexW(NULL, FALSE, mutexName);
    DWORD error = GetLastError();

    if (hMutex == NULL) {
        printf("뮤텍스 생성 실패: %lu\n", GetLastError());
        return;
    }

    if (error == ERROR_ALREADY_EXISTS) {
        printf("뮤텍스가 이미 존재합니다 - 다른 인스턴스 실행 중!\n");
    } else {
        printf("새 뮤텍스 생성됨 - 첫 번째 인스턴스\n");
    }

    // 뮤텍스 획득 테스트
    printf("\n뮤텍스 획득 시도 (1초 타임아웃)...\n");
    DWORD result = WaitForSingleObject(hMutex, 1000);

    switch (result) {
    case WAIT_OBJECT_0:
        printf("뮤텍스 획득 성공!\n");
        ReleaseMutex(hMutex);
        printf("뮤텍스 해제\n");
        break;
    case WAIT_ABANDONED:
        printf("WAIT_ABANDONED - 이전 소유자가 비정상 종료\n");
        break;
    case WAIT_TIMEOUT:
        printf("타임아웃\n");
        break;
    }

    CloseHandle(hMutex);
}

//=============================================================================
// 4. SRWLock 예제 (읽기/쓰기 분리)
//=============================================================================

SRWLOCK g_srwLock = SRWLOCK_INIT;
int g_sharedData = 0;
volatile LONG g_readCount = 0;
volatile LONG g_writeCount = 0;

unsigned __stdcall ReaderThread(void* param) {
    int id = *(int*)param;

    for (int i = 0; i < 10; i++) {
        AcquireSRWLockShared(&g_srwLock);

        // 읽기 작업
        int value = g_sharedData;
        InterlockedIncrement(&g_readCount);

        // 여러 리더가 동시에 읽을 수 있음
        Sleep(10);

        ReleaseSRWLockShared(&g_srwLock);
        Sleep(5);
    }

    return 0;
}

unsigned __stdcall WriterThread(void* param) {
    int id = *(int*)param;

    for (int i = 0; i < 5; i++) {
        AcquireSRWLockExclusive(&g_srwLock);

        // 쓰기 작업 (단독)
        g_sharedData++;
        InterlockedIncrement(&g_writeCount);

        Sleep(20);

        ReleaseSRWLockExclusive(&g_srwLock);
        Sleep(50);
    }

    return 0;
}

void SRWLockExample() {
    printf("\n=== SRWLock 예제 ===\n");

    g_readCount = 0;
    g_writeCount = 0;
    g_sharedData = 0;

    const int READER_COUNT = 5;
    const int WRITER_COUNT = 2;

    HANDLE hReaders[READER_COUNT];
    HANDLE hWriters[WRITER_COUNT];
    int readerIds[READER_COUNT];
    int writerIds[WRITER_COUNT];

    printf("리더 %d개, 라이터 %d개 시작\n", READER_COUNT, WRITER_COUNT);

    for (int i = 0; i < READER_COUNT; i++) {
        readerIds[i] = i + 1;
        hReaders[i] = (HANDLE)_beginthreadex(NULL, 0, ReaderThread, &readerIds[i], 0, NULL);
    }

    for (int i = 0; i < WRITER_COUNT; i++) {
        writerIds[i] = i + 1;
        hWriters[i] = (HANDLE)_beginthreadex(NULL, 0, WriterThread, &writerIds[i], 0, NULL);
    }

    WaitForMultipleObjects(READER_COUNT, hReaders, TRUE, INFINITE);
    WaitForMultipleObjects(WRITER_COUNT, hWriters, TRUE, INFINITE);

    printf("읽기 횟수: %ld\n", g_readCount);
    printf("쓰기 횟수: %ld\n", g_writeCount);
    printf("최종 값: %d\n", g_sharedData);

    for (int i = 0; i < READER_COUNT; i++) CloseHandle(hReaders[i]);
    for (int i = 0; i < WRITER_COUNT; i++) CloseHandle(hWriters[i]);
}

//=============================================================================
// 5. Condition Variable 예제 (Producer-Consumer)
//=============================================================================

CRITICAL_SECTION g_csQueue;
CONDITION_VARIABLE g_cvNotEmpty;
CONDITION_VARIABLE g_cvNotFull;
std::queue<int> g_workQueue;
const int MAX_QUEUE_SIZE = 5;
volatile bool g_producerDone = false;

unsigned __stdcall Producer(void* param) {
    for (int i = 1; i <= 20; i++) {
        EnterCriticalSection(&g_csQueue);

        // 큐가 가득 찼으면 대기
        while (g_workQueue.size() >= MAX_QUEUE_SIZE) {
            printf("[Producer] 큐 가득 참, 대기...\n");
            SleepConditionVariableCS(&g_cvNotFull, &g_csQueue, INFINITE);
        }

        // 아이템 추가
        g_workQueue.push(i);
        printf("[Producer] 생산: %d (큐 크기: %zu)\n", i, g_workQueue.size());

        LeaveCriticalSection(&g_csQueue);

        // Consumer 깨우기
        WakeConditionVariable(&g_cvNotEmpty);

        Sleep(50);
    }

    g_producerDone = true;
    WakeAllConditionVariable(&g_cvNotEmpty);  // 모든 Consumer 깨움

    printf("[Producer] 완료!\n");
    return 0;
}

unsigned __stdcall Consumer(void* param) {
    int id = *(int*)param;

    while (true) {
        EnterCriticalSection(&g_csQueue);

        // 큐가 비었으면 대기
        while (g_workQueue.empty() && !g_producerDone) {
            SleepConditionVariableCS(&g_cvNotEmpty, &g_csQueue, INFINITE);
        }

        if (g_workQueue.empty() && g_producerDone) {
            LeaveCriticalSection(&g_csQueue);
            break;
        }

        // 아이템 소비
        int item = g_workQueue.front();
        g_workQueue.pop();
        printf("[Consumer %d] 소비: %d (큐 크기: %zu)\n", id, item, g_workQueue.size());

        LeaveCriticalSection(&g_csQueue);

        // Producer 깨우기
        WakeConditionVariable(&g_cvNotFull);

        Sleep(100);  // 처리 시간
    }

    printf("[Consumer %d] 종료\n", id);
    return 0;
}

void ConditionVariableExample() {
    printf("\n=== Condition Variable 예제 (Producer-Consumer) ===\n");

    InitializeCriticalSection(&g_csQueue);
    InitializeConditionVariable(&g_cvNotEmpty);
    InitializeConditionVariable(&g_cvNotFull);
    g_producerDone = false;
    while (!g_workQueue.empty()) g_workQueue.pop();

    HANDLE hProducer;
    HANDLE hConsumers[3];
    int consumerIds[3] = { 1, 2, 3 };

    hProducer = (HANDLE)_beginthreadex(NULL, 0, Producer, NULL, 0, NULL);

    for (int i = 0; i < 3; i++) {
        hConsumers[i] = (HANDLE)_beginthreadex(NULL, 0, Consumer, &consumerIds[i], 0, NULL);
    }

    WaitForSingleObject(hProducer, INFINITE);
    WaitForMultipleObjects(3, hConsumers, TRUE, INFINITE);

    CloseHandle(hProducer);
    for (int i = 0; i < 3; i++) CloseHandle(hConsumers[i]);
    DeleteCriticalSection(&g_csQueue);

    printf("모든 작업 완료!\n");
}

//=============================================================================
// 6. Interlocked 함수 예제
//=============================================================================

volatile LONG g_atomicCounter = 0;

unsigned __stdcall InterlockedWorker(void* param) {
    for (int i = 0; i < 100000; i++) {
        InterlockedIncrement(&g_atomicCounter);
    }
    return 0;
}

void InterlockedExample() {
    printf("\n=== Interlocked 함수 예제 ===\n");

    g_atomicCounter = 0;

    const int THREAD_COUNT = 4;
    HANDLE hThreads[THREAD_COUNT];

    printf("%d 스레드가 각각 100,000번 InterlockedIncrement\n", THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; i++) {
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, InterlockedWorker, NULL, 0, NULL);
    }

    WaitForMultipleObjects(THREAD_COUNT, hThreads, TRUE, INFINITE);

    printf("예상 값: %d\n", THREAD_COUNT * 100000);
    printf("실제 값: %ld (정확!)\n", g_atomicCounter);

    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(hThreads[i]);

    // Compare and Exchange 예제
    printf("\nInterlockedCompareExchange 예제:\n");
    LONG value = 100;
    LONG old;

    old = InterlockedCompareExchange(&value, 200, 100);
    printf("value=100일 때 100→200 교환: old=%ld, new=%ld\n", old, value);

    old = InterlockedCompareExchange(&value, 300, 100);
    printf("value=200일 때 100→300 교환 시도: old=%ld, new=%ld (변경 없음)\n", old, value);
}

//=============================================================================
// 7. WaitForMultipleObjects 예제
//=============================================================================

void WaitForMultipleObjectsExample() {
    printf("\n=== WaitForMultipleObjects 예제 ===\n");

    HANDLE hEvents[4];
    for (int i = 0; i < 4; i++) {
        hEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    // 비동기로 이벤트 설정
    for (int i = 0; i < 4; i++) {
        int* pIndex = new int(i);
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0,
            [](void* param) -> unsigned {
                int idx = *(int*)param;
                Sleep(100 * (idx + 1));
                printf("Event %d 신호!\n", idx);
                return 0;
            }, pIndex, 0, NULL);

        // 스레드가 이벤트를 설정하도록
        _beginthreadex(NULL, 0,
            [](void* param) -> unsigned {
                HANDLE* data = (HANDLE*)param;
                Sleep(100 * ((int)(LONG_PTR)data[1] + 1));
                SetEvent(data[0]);
                delete[] data;
                return 0;
            }, new HANDLE[2]{ hEvents[i], (HANDLE)(LONG_PTR)i }, 0, NULL);

        CloseHandle(hThread);
    }

    // 하나라도 신호 대기
    printf("아무 이벤트나 대기...\n");
    DWORD result = WaitForMultipleObjects(4, hEvents, FALSE, INFINITE);
    printf("Event %lu 가 먼저 신호됨!\n", result - WAIT_OBJECT_0);

    // 나머지 모두 대기
    printf("나머지 모두 대기...\n");
    WaitForMultipleObjects(4, hEvents, TRUE, INFINITE);
    printf("모든 이벤트 신호됨!\n");

    for (int i = 0; i < 4; i++) {
        CloseHandle(hEvents[i]);
    }
}

//=============================================================================
// Main
//=============================================================================

int main() {
    printf("========================================\n");
    printf("    동기화 객체 예제\n");
    printf("========================================\n");

    EventExample();
    SemaphoreExample();
    MutexExample();
    SRWLockExample();
    ConditionVariableExample();
    InterlockedExample();
    WaitForMultipleObjectsExample();

    printf("\n========================================\n");
    printf("    모든 예제 완료\n");
    printf("========================================\n");

    return 0;
}
