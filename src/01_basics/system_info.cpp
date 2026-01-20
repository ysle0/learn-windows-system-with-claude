/**
 * 1장 예제: Windows 시스템 정보 조회
 *
 * Windows 아키텍처와 기본 API 사용법을 익히는 예제입니다.
 * 시스템 정보, 프로세서 정보, 메모리 상태 등을 조회합니다.
 *
 * 컴파일: cl /EHsc /W4 system_info.cpp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

// 프로세서 아키텍처를 문자열로 변환
const char* GetProcessorArchitectureString(WORD arch) {
    switch (arch) {
        case PROCESSOR_ARCHITECTURE_AMD64:  return "x64 (AMD64)";
        case PROCESSOR_ARCHITECTURE_INTEL:  return "x86 (Intel)";
        case PROCESSOR_ARCHITECTURE_ARM:    return "ARM";
        case PROCESSOR_ARCHITECTURE_ARM64:  return "ARM64";
        default:                            return "Unknown";
    }
}

// Windows 버전 정보 출력
void PrintWindowsVersion() {
    printf("=== Windows 버전 정보 ===\n");

    // RtlGetVersion을 사용 (GetVersionEx는 deprecated)
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion =
            (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");

        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW osvi = { 0 };
            osvi.dwOSVersionInfoSize = sizeof(osvi);

            if (pRtlGetVersion(&osvi) == 0) {
                printf("Windows Version: %lu.%lu.%lu\n",
                       osvi.dwMajorVersion,
                       osvi.dwMinorVersion,
                       osvi.dwBuildNumber);
            }
        }
    }
    printf("\n");
}

// 시스템 정보 출력
void PrintSystemInfo() {
    printf("=== 시스템 정보 ===\n");

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    printf("프로세서 아키텍처: %s\n",
           GetProcessorArchitectureString(si.wProcessorArchitecture));
    printf("프로세서 개수: %lu\n", si.dwNumberOfProcessors);
    printf("페이지 크기: %lu bytes\n", si.dwPageSize);
    printf("할당 단위: %lu bytes\n", si.dwAllocationGranularity);
    printf("최소 응용 프로그램 주소: 0x%p\n", si.lpMinimumApplicationAddress);
    printf("최대 응용 프로그램 주소: 0x%p\n", si.lpMaximumApplicationAddress);
    printf("Active 프로세서 마스크: 0x%llX\n",
           (unsigned long long)si.dwActiveProcessorMask);
    printf("\n");
}

// 메모리 상태 출력
void PrintMemoryStatus() {
    printf("=== 메모리 상태 ===\n");

    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);

    if (GlobalMemoryStatusEx(&ms)) {
        printf("메모리 사용률: %lu%%\n", ms.dwMemoryLoad);
        printf("전체 물리 메모리: %.2f GB\n",
               ms.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
        printf("사용 가능 물리 메모리: %.2f GB\n",
               ms.ullAvailPhys / (1024.0 * 1024.0 * 1024.0));
        printf("전체 가상 메모리: %.2f GB\n",
               ms.ullTotalVirtual / (1024.0 * 1024.0 * 1024.0));
        printf("사용 가능 가상 메모리: %.2f GB\n",
               ms.ullAvailVirtual / (1024.0 * 1024.0 * 1024.0));
        printf("전체 페이지 파일: %.2f GB\n",
               ms.ullTotalPageFile / (1024.0 * 1024.0 * 1024.0));
        printf("사용 가능 페이지 파일: %.2f GB\n",
               ms.ullAvailPageFile / (1024.0 * 1024.0 * 1024.0));
    } else {
        printf("메모리 상태 조회 실패: %lu\n", GetLastError());
    }
    printf("\n");
}

// 현재 프로세스 정보 출력
void PrintProcessInfo() {
    printf("=== 현재 프로세스 정보 ===\n");

    // 프로세스 핸들과 ID
    HANDLE hProcess = GetCurrentProcess();  // 의사 핸들 (pseudo-handle)
    DWORD processId = GetCurrentProcessId();

    printf("프로세스 ID: %lu\n", processId);
    printf("프로세스 핸들 (의사): 0x%p\n", hProcess);

    // 스레드 정보
    HANDLE hThread = GetCurrentThread();  // 의사 핸들
    DWORD threadId = GetCurrentThreadId();

    printf("스레드 ID: %lu\n", threadId);
    printf("스레드 핸들 (의사): 0x%p\n", hThread);

    // 커맨드 라인
    LPWSTR cmdLine = GetCommandLineW();
    printf("커맨드 라인: %ls\n", cmdLine);

    // 현재 디렉토리
    WCHAR currentDir[MAX_PATH];
    if (GetCurrentDirectoryW(MAX_PATH, currentDir)) {
        printf("현재 디렉토리: %ls\n", currentDir);
    }

    // 실행 파일 경로
    WCHAR exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        printf("실행 파일: %ls\n", exePath);
    }

    printf("\n");
}

// 핸들 정보 예제
void DemonstrateHandles() {
    printf("=== 핸들 사용 예제 ===\n");

    // 1. 파일 핸들 생성
    HANDLE hFile = CreateFileW(
        L"test_handle.tmp",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        printf("파일 핸들 생성 성공: 0x%p\n", hFile);

        // 파일에 데이터 쓰기
        const char* data = "Hello, Windows System Programming!";
        DWORD written;
        if (WriteFile(hFile, data, (DWORD)strlen(data), &written, NULL)) {
            printf("파일에 %lu 바이트 기록\n", written);
        }

        // 핸들 닫기
        CloseHandle(hFile);
        printf("파일 핸들 닫음\n");

        // 임시 파일 삭제
        DeleteFileW(L"test_handle.tmp");
    } else {
        printf("파일 핸들 생성 실패: %lu\n", GetLastError());
    }

    // 2. 이벤트 핸들 생성
    HANDLE hEvent = CreateEventW(
        NULL,   // 보안 속성
        TRUE,   // 수동 리셋
        FALSE,  // 초기 상태 (비신호)
        NULL    // 이름 (익명)
    );

    if (hEvent != NULL) {
        printf("이벤트 핸들 생성 성공: 0x%p\n", hEvent);
        CloseHandle(hEvent);
        printf("이벤트 핸들 닫음\n");
    }

    // 3. 뮤텍스 핸들 생성
    HANDLE hMutex = CreateMutexW(
        NULL,   // 보안 속성
        FALSE,  // 초기 소유권
        NULL    // 이름 (익명)
    );

    if (hMutex != NULL) {
        printf("뮤텍스 핸들 생성 성공: 0x%p\n", hMutex);
        CloseHandle(hMutex);
        printf("뮤텍스 핸들 닫음\n");
    }

    printf("\n");
}

// 에러 처리 예제
void DemonstrateErrorHandling() {
    printf("=== 에러 처리 예제 ===\n");

    // 존재하지 않는 파일 열기 시도
    HANDLE hFile = CreateFileW(
        L"non_existent_file_12345.txt",
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD dwError = GetLastError();
        printf("에러 코드: %lu\n", dwError);

        // 에러 메시지 얻기
        LPWSTR pMessage = NULL;
        DWORD len = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dwError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&pMessage,
            0,
            NULL
        );

        if (len > 0 && pMessage) {
            printf("에러 메시지: %ls", pMessage);
            LocalFree(pMessage);
        }
    }

    printf("\n");
}

int main() {
    printf("========================================\n");
    printf("    Windows 시스템 정보 조회 예제\n");
    printf("========================================\n\n");

    PrintWindowsVersion();
    PrintSystemInfo();
    PrintMemoryStatus();
    PrintProcessInfo();
    DemonstrateHandles();
    DemonstrateErrorHandling();

    printf("========================================\n");
    printf("    예제 완료\n");
    printf("========================================\n");

    return 0;
}
