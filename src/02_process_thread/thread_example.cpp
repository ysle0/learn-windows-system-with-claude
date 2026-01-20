/**
 * 2장 예제: 프로세스와 스레드
 *
 * 스레드 생성, 동기화, 스레드 풀 사용법을 보여주는 예제입니다.
 * 게임서버에서 자주 사용되는 패턴을 포함합니다.
 *
 * 컴파일: cl /EHsc /W4 thread_example.cpp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>  // _beginthreadex
#include <stdio.h>
#include <stdlib.h>

//=============================================================================
// 1. 기본 스레드 예제
//=============================================================================

// 스레드 함수 (CreateThread용)
DWORD WINAPI SimpleThreadFunc(LPVOID lpParam) {
    int threadNum = *(int*)lpParam;
    printf("[스레드 %d] 시작 (ID: %lu)\n", threadNum, GetCurrentThreadId());

    // 작업 시뮬레이션
    Sleep(100);

    printf("[스레드 %d] 종료\n", threadNum);
    return threadNum * 10;  // 반환값
}

void BasicThreadExample() {
    printf("\n=== 기본 스레드 예제 ===\n");

    const int THREAD_COUNT = 4;
    HANDLE hThreads[THREAD_COUNT];
    int threadParams[THREAD_COUNT];

    // 스레드 생성
    for (int i = 0; i < THREAD_COUNT; i++) {
        threadParams[i] = i + 1;
        hThreads[i] = CreateThread(
            NULL,                   // 보안 속성
            0,                      // 스택 크기 (기본값)
            SimpleThreadFunc,       // 스레드 함수
            &threadParams[i],       // 매개변수
            0,                      // 플래그 (즉시 실행)
            NULL                    // 스레드 ID (불필요)
        );

        if (hThreads[i] == NULL) {
            printf("스레드 %d 생성 실패: %lu\n", i, GetLastError());
        }
    }

    // 모든 스레드 완료 대기
    WaitForMultipleObjects(THREAD_COUNT, hThreads, TRUE, INFINITE);

    // 반환값 확인 및 핸들 정리
    for (int i = 0; i < THREAD_COUNT; i++) {
        DWORD exitCode;
        GetExitCodeThread(hThreads[i], &exitCode);
        printf("스레드 %d 반환값: %lu\n", i + 1, exitCode);
        CloseHandle(hThreads[i]);
    }
}

//=============================================================================
// 2. _beginthreadex 예제 (C 런타임 안전)
//=============================================================================

// _beginthreadex용 스레드 함수
unsigned __stdcall SafeThreadFunc(void* arg) {
    int id = *(int*)arg;

    // C 런타임 함수 안전하게 사용 가능
    printf("[SafeThread %d] C 런타임 함수 사용 가능\n", id);

    // errno도 스레드별로 안전
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "Thread %d message", id);
    printf("[SafeThread %d] %s\n", id, buffer);

    return 0;
}

void SafeThreadExample() {
    printf("\n=== _beginthreadex 예제 ===\n");

    int param = 1;
    unsigned threadId;

    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,           // 보안 속성
        0,              // 스택 크기
        SafeThreadFunc, // 스레드 함수
        &param,         // 매개변수
        0,              // 플래그
        &threadId       // 스레드 ID
    );

    if (hThread != 0) {
        printf("스레드 생성됨 (ID: %u)\n", threadId);
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
}

//=============================================================================
// 3. 동기화 문제 데모
//=============================================================================

volatile LONG g_unsafeCounter = 0;
volatile LONG g_safeCounter = 0;
CRITICAL_SECTION g_cs;

DWORD WINAPI UnsafeIncrementThread(LPVOID param) {
    for (int i = 0; i < 100000; i++) {
        g_unsafeCounter++;  // 안전하지 않음!
    }
    return 0;
}

DWORD WINAPI SafeIncrementThread(LPVOID param) {
    for (int i = 0; i < 100000; i++) {
        EnterCriticalSection(&g_cs);
        g_safeCounter++;
        LeaveCriticalSection(&g_cs);
    }
    return 0;
}

DWORD WINAPI InterlockedIncrementThread(LPVOID param) {
    for (int i = 0; i < 100000; i++) {
        InterlockedIncrement(&g_safeCounter);  // 원자적 연산
    }
    return 0;
}

void SynchronizationExample() {
    printf("\n=== 동기화 예제 ===\n");

    InitializeCriticalSection(&g_cs);

    const int THREAD_COUNT = 4;
    HANDLE hThreads[THREAD_COUNT];

    // 안전하지 않은 증가 테스트
    g_unsafeCounter = 0;
    printf("동기화 없이 %d 스레드가 각각 100,000번 증가...\n", THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; i++) {
        hThreads[i] = CreateThread(NULL, 0, UnsafeIncrementThread, NULL, 0, NULL);
    }
    WaitForMultipleObjects(THREAD_COUNT, hThreads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(hThreads[i]);

    printf("예상 값: %d\n", THREAD_COUNT * 100000);
    printf("실제 값: %ld (경쟁 상태로 인한 손실!)\n", g_unsafeCounter);

    // Critical Section 사용
    g_safeCounter = 0;
    printf("\nCriticalSection으로 동기화...\n");

    for (int i = 0; i < THREAD_COUNT; i++) {
        hThreads[i] = CreateThread(NULL, 0, SafeIncrementThread, NULL, 0, NULL);
    }
    WaitForMultipleObjects(THREAD_COUNT, hThreads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(hThreads[i]);

    printf("결과: %ld (정확!)\n", g_safeCounter);

    // Interlocked 함수 사용
    g_safeCounter = 0;
    printf("\nInterlockedIncrement로 동기화...\n");

    for (int i = 0; i < THREAD_COUNT; i++) {
        hThreads[i] = CreateThread(NULL, 0, InterlockedIncrementThread, NULL, 0, NULL);
    }
    WaitForMultipleObjects(THREAD_COUNT, hThreads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(hThreads[i]);

    printf("결과: %ld (정확!)\n", g_safeCounter);

    DeleteCriticalSection(&g_cs);
}

//=============================================================================
// 4. 뮤텍스 예제 (프로세스 간 동기화 가능)
//=============================================================================

HANDLE g_hMutex = NULL;
int g_sharedResource = 0;

DWORD WINAPI MutexThread(LPVOID param) {
    int threadId = *(int*)param;

    for (int i = 0; i < 5; i++) {
        // 뮤텍스 획득 (타임아웃 지원)
        DWORD result = WaitForSingleObject(g_hMutex, 5000);

        switch (result) {
        case WAIT_OBJECT_0:
            // 뮤텍스 획득 성공
            g_sharedResource++;
            printf("[Thread %d] 리소스 접근: %d\n", threadId, g_sharedResource);
            Sleep(10);  // 작업 시뮬레이션
            ReleaseMutex(g_hMutex);
            break;

        case WAIT_TIMEOUT:
            printf("[Thread %d] 타임아웃!\n", threadId);
            break;

        case WAIT_ABANDONED:
            printf("[Thread %d] 이전 소유자가 해제 없이 종료됨\n", threadId);
            break;
        }
    }

    return 0;
}

void MutexExample() {
    printf("\n=== 뮤텍스 예제 ===\n");

    // 이름 있는 뮤텍스 생성 (프로세스 간 공유 가능)
    g_hMutex = CreateMutexW(
        NULL,               // 보안 속성
        FALSE,              // 초기 소유권
        L"MyGameMutex"      // 이름
    );

    if (g_hMutex == NULL) {
        printf("뮤텍스 생성 실패: %lu\n", GetLastError());
        return;
    }

    // 이미 존재하는 뮤텍스인지 확인
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("기존 뮤텍스에 연결됨\n");
    } else {
        printf("새 뮤텍스 생성됨\n");
    }

    const int THREAD_COUNT = 3;
    HANDLE hThreads[THREAD_COUNT];
    int threadIds[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        threadIds[i] = i + 1;
        hThreads[i] = CreateThread(NULL, 0, MutexThread, &threadIds[i], 0, NULL);
    }

    WaitForMultipleObjects(THREAD_COUNT, hThreads, TRUE, INFINITE);

    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(hThreads[i]);
    CloseHandle(g_hMutex);

    printf("최종 리소스 값: %d\n", g_sharedResource);
}

//=============================================================================
// 5. 이벤트 예제 (스레드 간 신호 전달)
//=============================================================================

HANDLE g_hStartEvent = NULL;
HANDLE g_hDoneEvents[4] = { NULL };

DWORD WINAPI EventWorkerThread(LPVOID param) {
    int id = *(int*)param;

    printf("[Worker %d] 시작 신호 대기 중...\n", id);

    // 시작 신호 대기
    WaitForSingleObject(g_hStartEvent, INFINITE);

    printf("[Worker %d] 작업 시작!\n", id);
    Sleep(100 + (id * 50));  // 작업 시뮬레이션
    printf("[Worker %d] 작업 완료!\n", id);

    // 완료 신호 전송
    SetEvent(g_hDoneEvents[id]);

    return 0;
}

void EventExample() {
    printf("\n=== 이벤트 예제 ===\n");

    // 수동 리셋 이벤트 (SetEvent 후 자동으로 리셋되지 않음)
    // 모든 대기 스레드가 동시에 깨어남
    g_hStartEvent = CreateEventW(
        NULL,   // 보안 속성
        TRUE,   // 수동 리셋
        FALSE,  // 초기 상태 (비신호)
        NULL    // 이름
    );

    const int WORKER_COUNT = 4;
    HANDLE hThreads[WORKER_COUNT];
    int threadIds[WORKER_COUNT];

    // 완료 이벤트 생성 (자동 리셋)
    for (int i = 0; i < WORKER_COUNT; i++) {
        g_hDoneEvents[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
        threadIds[i] = i;
        hThreads[i] = CreateThread(NULL, 0, EventWorkerThread, &threadIds[i], 0, NULL);
    }

    printf("[Main] 2초 후 시작 신호 전송...\n");
    Sleep(2000);

    // 모든 워커 스레드에 시작 신호 전송
    printf("[Main] 시작!\n");
    SetEvent(g_hStartEvent);

    // 모든 워커 완료 대기
    WaitForMultipleObjects(WORKER_COUNT, g_hDoneEvents, TRUE, INFINITE);
    printf("[Main] 모든 워커 완료!\n");

    // 정리
    for (int i = 0; i < WORKER_COUNT; i++) {
        CloseHandle(hThreads[i]);
        CloseHandle(g_hDoneEvents[i]);
    }
    CloseHandle(g_hStartEvent);
}

//=============================================================================
// 6. 게임서버용 스레드 풀 패턴 (수동 구현)
//=============================================================================

#define MAX_QUEUE_SIZE 100
#define POOL_THREAD_COUNT 4

// 작업 구조체
typedef struct {
    void (*func)(void* arg);
    void* arg;
} WorkItem;

// 스레드 풀 구조체
typedef struct {
    CRITICAL_SECTION queueLock;
    HANDLE hWorkEvent;      // 작업 있음 신호
    HANDLE hStopEvent;      // 종료 신호
    HANDLE hThreads[POOL_THREAD_COUNT];

    WorkItem queue[MAX_QUEUE_SIZE];
    int queueHead;
    int queueTail;
    int queueCount;
} ThreadPool;

ThreadPool g_pool;

// 작업 큐에서 꺼내기
BOOL DequeueWork(WorkItem* item) {
    BOOL result = FALSE;

    EnterCriticalSection(&g_pool.queueLock);
    if (g_pool.queueCount > 0) {
        *item = g_pool.queue[g_pool.queueHead];
        g_pool.queueHead = (g_pool.queueHead + 1) % MAX_QUEUE_SIZE;
        g_pool.queueCount--;
        result = TRUE;
    }
    LeaveCriticalSection(&g_pool.queueLock);

    return result;
}

// 풀 워커 스레드
DWORD WINAPI PoolWorkerThread(LPVOID param) {
    int workerId = *(int*)param;
    printf("[PoolWorker %d] 시작\n", workerId);

    HANDLE waitHandles[2] = { g_pool.hStopEvent, g_pool.hWorkEvent };

    while (TRUE) {
        // 종료 또는 작업 대기
        DWORD result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (result == WAIT_OBJECT_0) {
            // 종료 신호
            printf("[PoolWorker %d] 종료 신호 수신\n", workerId);
            break;
        }

        // 작업 처리
        WorkItem item;
        while (DequeueWork(&item)) {
            printf("[PoolWorker %d] 작업 실행\n", workerId);
            item.func(item.arg);
        }
    }

    printf("[PoolWorker %d] 종료\n", workerId);
    return 0;
}

// 풀 초기화
void InitThreadPool() {
    InitializeCriticalSection(&g_pool.queueLock);
    g_pool.hWorkEvent = CreateEventW(NULL, FALSE, FALSE, NULL);  // 자동 리셋
    g_pool.hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);   // 수동 리셋
    g_pool.queueHead = 0;
    g_pool.queueTail = 0;
    g_pool.queueCount = 0;

    static int workerIds[POOL_THREAD_COUNT];
    for (int i = 0; i < POOL_THREAD_COUNT; i++) {
        workerIds[i] = i;
        g_pool.hThreads[i] = CreateThread(NULL, 0, PoolWorkerThread, &workerIds[i], 0, NULL);
    }
}

// 작업 제출
BOOL SubmitWork(void (*func)(void*), void* arg) {
    BOOL result = FALSE;

    EnterCriticalSection(&g_pool.queueLock);
    if (g_pool.queueCount < MAX_QUEUE_SIZE) {
        g_pool.queue[g_pool.queueTail].func = func;
        g_pool.queue[g_pool.queueTail].arg = arg;
        g_pool.queueTail = (g_pool.queueTail + 1) % MAX_QUEUE_SIZE;
        g_pool.queueCount++;
        result = TRUE;

        // 워커 깨우기
        SetEvent(g_pool.hWorkEvent);
    }
    LeaveCriticalSection(&g_pool.queueLock);

    return result;
}

// 풀 종료
void ShutdownThreadPool() {
    // 종료 신호 전송
    SetEvent(g_pool.hStopEvent);

    // 모든 워커 종료 대기
    WaitForMultipleObjects(POOL_THREAD_COUNT, g_pool.hThreads, TRUE, INFINITE);

    // 정리
    for (int i = 0; i < POOL_THREAD_COUNT; i++) {
        CloseHandle(g_pool.hThreads[i]);
    }
    CloseHandle(g_pool.hWorkEvent);
    CloseHandle(g_pool.hStopEvent);
    DeleteCriticalSection(&g_pool.queueLock);
}

// 테스트 작업
void TestWork(void* arg) {
    int taskId = *(int*)arg;
    printf("  [Task %d] 실행 중... (Thread: %lu)\n", taskId, GetCurrentThreadId());
    Sleep(50);
    printf("  [Task %d] 완료\n", taskId);
}

void ThreadPoolExample() {
    printf("\n=== 수동 스레드 풀 예제 ===\n");

    InitThreadPool();

    printf("작업 제출 중...\n");
    static int taskIds[10];
    for (int i = 0; i < 10; i++) {
        taskIds[i] = i + 1;
        SubmitWork(TestWork, &taskIds[i]);
    }

    // 작업 완료 대기 (간단한 방법)
    Sleep(2000);

    printf("스레드 풀 종료 중...\n");
    ShutdownThreadPool();
    printf("완료!\n");
}

//=============================================================================
// 7. 프로세스 생성 예제
//=============================================================================

void ProcessCreationExample() {
    printf("\n=== 프로세스 생성 예제 ===\n");

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // 창 숨김

    PROCESS_INFORMATION pi = { 0 };

    // notepad 실행 (Windows에서)
    WCHAR cmdLine[] = L"cmd.exe /c echo Hello from child process!";

    printf("자식 프로세스 생성 중...\n");

    BOOL success = CreateProcessW(
        NULL,               // 응용 프로그램 이름
        cmdLine,            // 커맨드 라인 (수정 가능해야 함!)
        NULL,               // 프로세스 보안 속성
        NULL,               // 스레드 보안 속성
        FALSE,              // 핸들 상속
        CREATE_NO_WINDOW,   // 생성 플래그
        NULL,               // 환경 변수
        NULL,               // 현재 디렉토리
        &si,                // 시작 정보
        &pi                 // 프로세스 정보
    );

    if (success) {
        printf("프로세스 생성 성공!\n");
        printf("  프로세스 ID: %lu\n", pi.dwProcessId);
        printf("  메인 스레드 ID: %lu\n", pi.dwThreadId);

        // 자식 프로세스 종료 대기
        printf("자식 프로세스 종료 대기 중...\n");
        WaitForSingleObject(pi.hProcess, INFINITE);

        // 종료 코드 확인
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        printf("자식 프로세스 종료 코드: %lu\n", exitCode);

        // 핸들 정리
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        printf("프로세스 생성 실패: %lu\n", GetLastError());
    }
}

//=============================================================================
// Main
//=============================================================================

int main() {
    printf("========================================\n");
    printf("    프로세스와 스레드 예제\n");
    printf("========================================\n");

    BasicThreadExample();
    SafeThreadExample();
    SynchronizationExample();
    MutexExample();
    EventExample();
    ThreadPoolExample();
    ProcessCreationExample();

    printf("\n========================================\n");
    printf("    모든 예제 완료\n");
    printf("========================================\n");

    return 0;
}
