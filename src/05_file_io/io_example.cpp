/**
 * 5장 예제: 파일 시스템과 I/O
 *
 * 동기/비동기 파일 I/O, Overlapped I/O, IOCP 기초를 보여주는 예제입니다.
 * 게임서버의 IOCP 패턴의 기초가 됩니다.
 *
 * 컴파일: cl /EHsc /W4 io_example.cpp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <process.h>

//=============================================================================
// 1. 기본 파일 I/O 예제
//=============================================================================

void BasicFileIOExample() {
    printf("\n=== 기본 파일 I/O 예제 ===\n");

    const WCHAR* fileName = L"test_basic_io.txt";

    // 파일 생성/쓰기
    HANDLE hFile = CreateFileW(
        fileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("파일 생성 실패: %lu\n", GetLastError());
        return;
    }

    const char* writeData = "Hello, Windows File I/O!\r\nThis is a test file.\r\n";
    DWORD written;

    if (WriteFile(hFile, writeData, (DWORD)strlen(writeData), &written, NULL)) {
        printf("쓰기 완료: %lu bytes\n", written);
    }

    CloseHandle(hFile);

    // 파일 읽기
    hFile = CreateFileW(
        fileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        char buffer[256] = { 0 };
        DWORD read;

        if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &read, NULL)) {
            printf("읽기 완료: %lu bytes\n", read);
            printf("내용:\n%s\n", buffer);
        }

        // 파일 크기 확인
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        printf("파일 크기: %lld bytes\n", fileSize.QuadPart);

        CloseHandle(hFile);
    }

    // 테스트 파일 삭제
    DeleteFileW(fileName);
}

//=============================================================================
// 2. Overlapped I/O 예제 (이벤트 기반)
//=============================================================================

void OverlappedIOExample() {
    printf("\n=== Overlapped I/O 예제 ===\n");

    const WCHAR* fileName = L"test_overlapped.bin";
    const int FILE_SIZE = 1024 * 1024;  // 1MB

    // 테스트 파일 생성
    printf("1MB 테스트 파일 생성 중...\n");
    HANDLE hFile = CreateFileW(
        fileName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("파일 생성 실패: %lu\n", GetLastError());
        return;
    }

    // 데이터 준비
    char* writeBuffer = new char[FILE_SIZE];
    for (int i = 0; i < FILE_SIZE; i++) {
        writeBuffer[i] = (char)(i % 256);
    }

    // 비동기 쓰기
    OVERLAPPED ovWrite = { 0 };
    ovWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ovWrite.Offset = 0;

    LARGE_INTEGER startTime, endTime, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&startTime);

    BOOL result = WriteFile(hFile, writeBuffer, FILE_SIZE, NULL, &ovWrite);

    if (!result && GetLastError() == ERROR_IO_PENDING) {
        printf("비동기 쓰기 시작됨 (ERROR_IO_PENDING)\n");

        // 다른 작업 수행 가능 (여기서는 바로 대기)
        WaitForSingleObject(ovWrite.hEvent, INFINITE);

        DWORD bytesWritten;
        GetOverlappedResult(hFile, &ovWrite, &bytesWritten, FALSE);

        QueryPerformanceCounter(&endTime);
        double writeTime = (double)(endTime.QuadPart - startTime.QuadPart) / freq.QuadPart * 1000;

        printf("비동기 쓰기 완료: %lu bytes (%.2f ms)\n", bytesWritten, writeTime);
    }

    CloseHandle(ovWrite.hEvent);

    // 비동기 읽기
    char* readBuffer = new char[FILE_SIZE];
    OVERLAPPED ovRead = { 0 };
    ovRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ovRead.Offset = 0;

    QueryPerformanceCounter(&startTime);

    result = ReadFile(hFile, readBuffer, FILE_SIZE, NULL, &ovRead);

    if (!result && GetLastError() == ERROR_IO_PENDING) {
        printf("비동기 읽기 시작됨 (ERROR_IO_PENDING)\n");

        WaitForSingleObject(ovRead.hEvent, INFINITE);

        DWORD bytesRead;
        GetOverlappedResult(hFile, &ovRead, &bytesRead, FALSE);

        QueryPerformanceCounter(&endTime);
        double readTime = (double)(endTime.QuadPart - startTime.QuadPart) / freq.QuadPart * 1000;

        printf("비동기 읽기 완료: %lu bytes (%.2f ms)\n", bytesRead, readTime);

        // 데이터 검증
        bool valid = true;
        for (int i = 0; i < FILE_SIZE; i++) {
            if (readBuffer[i] != writeBuffer[i]) {
                valid = false;
                break;
            }
        }
        printf("데이터 검증: %s\n", valid ? "성공" : "실패");
    }

    CloseHandle(ovRead.hEvent);
    CloseHandle(hFile);

    delete[] writeBuffer;
    delete[] readBuffer;

    DeleteFileW(fileName);
}

//=============================================================================
// 3. IOCP 기본 예제 (파일 I/O)
//=============================================================================

// 확장된 OVERLAPPED 구조체
struct IOOperation : public OVERLAPPED {
    enum Type { IO_READ, IO_WRITE };
    Type type;
    char* buffer;
    DWORD bufferSize;

    IOOperation(Type t, DWORD size) : type(t), bufferSize(size) {
        memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
        buffer = new char[size];
    }

    ~IOOperation() {
        delete[] buffer;
    }
};

HANDLE g_hIOCP = NULL;
volatile bool g_workerRunning = true;

unsigned __stdcall IOCPWorker(void* param) {
    printf("[Worker] IOCP 워커 스레드 시작\n");

    while (g_workerRunning) {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        LPOVERLAPPED pOv;

        BOOL success = GetQueuedCompletionStatus(
            g_hIOCP,
            &bytesTransferred,
            &completionKey,
            &pOv,
            1000  // 1초 타임아웃
        );

        if (!success) {
            if (pOv == NULL) {
                // 타임아웃 또는 에러
                continue;
            }
            // I/O 에러
            printf("[Worker] I/O 에러: %lu\n", GetLastError());
            continue;
        }

        if (completionKey == 0) {
            // 종료 신호
            printf("[Worker] 종료 신호 수신\n");
            break;
        }

        IOOperation* pIO = (IOOperation*)pOv;

        if (pIO->type == IOOperation::IO_READ) {
            printf("[Worker] 읽기 완료: %lu bytes\n", bytesTransferred);
            // 처음 32바이트 출력
            printf("[Worker] 데이터: ");
            for (DWORD i = 0; i < min(32UL, bytesTransferred); i++) {
                printf("%02X ", (unsigned char)pIO->buffer[i]);
            }
            printf("\n");
        } else {
            printf("[Worker] 쓰기 완료: %lu bytes\n", bytesTransferred);
        }

        delete pIO;
    }

    printf("[Worker] 워커 스레드 종료\n");
    return 0;
}

void IOCPFileExample() {
    printf("\n=== IOCP 파일 I/O 예제 ===\n");

    const WCHAR* fileName = L"test_iocp.bin";

    // IOCP 생성
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (g_hIOCP == NULL) {
        printf("IOCP 생성 실패: %lu\n", GetLastError());
        return;
    }
    printf("IOCP 생성됨\n");

    // 워커 스레드 시작
    g_workerRunning = true;
    HANDLE hWorker = (HANDLE)_beginthreadex(NULL, 0, IOCPWorker, NULL, 0, NULL);

    // 비동기 파일 열기
    HANDLE hFile = CreateFileW(
        fileName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("파일 생성 실패: %lu\n", GetLastError());
        CloseHandle(g_hIOCP);
        return;
    }

    // 파일을 IOCP에 연결
    CreateIoCompletionPort(hFile, g_hIOCP, (ULONG_PTR)hFile, 0);
    printf("파일을 IOCP에 연결\n");

    // 비동기 쓰기
    IOOperation* writeOp = new IOOperation(IOOperation::IO_WRITE, 1024);
    for (int i = 0; i < 1024; i++) {
        writeOp->buffer[i] = (char)(i % 256);
    }

    printf("비동기 쓰기 요청...\n");
    WriteFile(hFile, writeOp->buffer, writeOp->bufferSize, NULL, writeOp);

    Sleep(500);  // 쓰기 완료 대기

    // 비동기 읽기
    IOOperation* readOp = new IOOperation(IOOperation::IO_READ, 1024);

    printf("비동기 읽기 요청...\n");
    ReadFile(hFile, readOp->buffer, readOp->bufferSize, NULL, readOp);

    Sleep(500);  // 읽기 완료 대기

    // 종료
    printf("워커 스레드 종료 요청...\n");
    g_workerRunning = false;
    PostQueuedCompletionStatus(g_hIOCP, 0, 0, NULL);

    WaitForSingleObject(hWorker, INFINITE);

    CloseHandle(hWorker);
    CloseHandle(hFile);
    CloseHandle(g_hIOCP);

    DeleteFileW(fileName);
}

//=============================================================================
// 4. 다중 파일 동시 I/O 예제
//=============================================================================

struct FileContext {
    WCHAR fileName[MAX_PATH];
    HANDLE hFile;
    int fileId;
};

void MultipleFileIOExample() {
    printf("\n=== 다중 파일 동시 I/O 예제 ===\n");

    const int FILE_COUNT = 4;
    HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    FileContext files[FILE_COUNT];
    IOOperation* ops[FILE_COUNT];

    // 파일들 생성 및 IOCP 연결
    for (int i = 0; i < FILE_COUNT; i++) {
        swprintf_s(files[i].fileName, L"test_multi_%d.bin", i);
        files[i].fileId = i;

        files[i].hFile = CreateFileW(
            files[i].fileName,
            GENERIC_READ | GENERIC_WRITE,
            0, NULL,
            CREATE_ALWAYS,
            FILE_FLAG_OVERLAPPED,
            NULL
        );

        CreateIoCompletionPort(files[i].hFile, hIOCP, (ULONG_PTR)&files[i], 0);

        printf("파일 %d 생성됨: %ls\n", i, files[i].fileName);
    }

    // 모든 파일에 동시에 쓰기
    printf("\n모든 파일에 동시 쓰기...\n");
    for (int i = 0; i < FILE_COUNT; i++) {
        ops[i] = new IOOperation(IOOperation::IO_WRITE, 4096);
        memset(ops[i]->buffer, 'A' + i, 4096);

        WriteFile(files[i].hFile, ops[i]->buffer, ops[i]->bufferSize, NULL, ops[i]);
    }

    // 모든 완료 대기
    int completed = 0;
    while (completed < FILE_COUNT) {
        DWORD bytes;
        FileContext* pFile;
        LPOVERLAPPED pOv;

        if (GetQueuedCompletionStatus(hIOCP, &bytes, (PULONG_PTR)&pFile, &pOv, INFINITE)) {
            printf("파일 %d 쓰기 완료: %lu bytes\n", pFile->fileId, bytes);
            completed++;
        }
    }

    // 정리
    for (int i = 0; i < FILE_COUNT; i++) {
        delete ops[i];
        CloseHandle(files[i].hFile);
        DeleteFileW(files[i].fileName);
    }

    CloseHandle(hIOCP);
    printf("모든 파일 처리 완료\n");
}

//=============================================================================
// 5. 동기 vs 비동기 성능 비교
//=============================================================================

void PerformanceComparisonExample() {
    printf("\n=== 동기 vs 비동기 I/O 성능 비교 ===\n");

    const WCHAR* fileName = L"test_perf.bin";
    const int BUFFER_SIZE = 64 * 1024;  // 64KB
    const int ITERATIONS = 100;

    char* buffer = new char[BUFFER_SIZE];
    memset(buffer, 'X', BUFFER_SIZE);

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    // 동기 I/O 테스트
    printf("\n동기 I/O 테스트 (%d회 쓰기)...\n", ITERATIONS);

    HANDLE hSync = CreateFileW(fileName, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    QueryPerformanceCounter(&start);
    for (int i = 0; i < ITERATIONS; i++) {
        DWORD written;
        SetFilePointer(hSync, 0, NULL, FILE_BEGIN);
        WriteFile(hSync, buffer, BUFFER_SIZE, &written, NULL);
        FlushFileBuffers(hSync);  // 디스크에 확실히 쓰기
    }
    QueryPerformanceCounter(&end);

    double syncTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000;
    printf("동기 I/O 시간: %.2f ms\n", syncTime);

    CloseHandle(hSync);

    // 비동기 I/O 테스트 (순차적으로 완료 대기)
    printf("\n비동기 I/O 테스트 (%d회 쓰기)...\n", ITERATIONS);

    HANDLE hAsync = CreateFileW(fileName, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
                                NULL);

    // FILE_FLAG_NO_BUFFERING은 섹터 정렬 필요
    // 간단히 하기 위해 일반 모드로 재시도
    if (hAsync == INVALID_HANDLE_VALUE) {
        hAsync = CreateFileW(fileName, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
    }

    OVERLAPPED ov = { 0 };
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    QueryPerformanceCounter(&start);
    for (int i = 0; i < ITERATIONS; i++) {
        ResetEvent(ov.hEvent);
        ov.Offset = 0;
        ov.OffsetHigh = 0;

        WriteFile(hAsync, buffer, BUFFER_SIZE, NULL, &ov);
        WaitForSingleObject(ov.hEvent, INFINITE);
    }
    QueryPerformanceCounter(&end);

    double asyncTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000;
    printf("비동기 I/O 시간: %.2f ms\n", asyncTime);

    CloseHandle(ov.hEvent);
    CloseHandle(hAsync);

    printf("\n비교: 비동기가 %.2fx %s\n",
           syncTime > asyncTime ? syncTime / asyncTime : asyncTime / syncTime,
           syncTime > asyncTime ? "빠름" : "느림 (단순 순차에서는 오버헤드)");

    delete[] buffer;
    DeleteFileW(fileName);
}

//=============================================================================
// Main
//=============================================================================

int main() {
    printf("========================================\n");
    printf("    파일 시스템과 I/O 예제\n");
    printf("========================================\n");

    BasicFileIOExample();
    OverlappedIOExample();
    IOCPFileExample();
    MultipleFileIOExample();
    PerformanceComparisonExample();

    printf("\n========================================\n");
    printf("    모든 예제 완료\n");
    printf("========================================\n");

    return 0;
}
