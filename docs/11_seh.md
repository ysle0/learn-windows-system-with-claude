# 11장: 구조적 예외 처리 (SEH)

## 11.1 예외의 개념

### 하드웨어 예외 vs 소프트웨어 예외
```
┌─────────────────────────────────────────────────────────────────┐
│                        예외 종류                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  하드웨어 예외 (CPU가 발생):                                    │
│  • EXCEPTION_ACCESS_VIOLATION (0xC0000005) - 잘못된 메모리 접근 │
│  • EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094) - 0으로 나눔       │
│  • EXCEPTION_STACK_OVERFLOW (0xC00000FD) - 스택 오버플로우      │
│  • EXCEPTION_BREAKPOINT (0x80000003) - 브레이크포인트           │
│                                                                 │
│  소프트웨어 예외 (코드가 발생):                                  │
│  • RaiseException() 호출                                        │
│  • C++ throw (내부적으로 SEH 사용)                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### SEH vs C++ Exception
| 특성 | SEH | C++ Exception |
|------|-----|---------------|
| 언어 | C/C++ | C++만 |
| 범위 | OS 수준 | 언어 수준 |
| 하드웨어 예외 | 처리 가능 | 처리 불가 (일반적으로) |
| 소멸자 호출 | /EHa 필요 | 자동 |
| 성능 | 빠름 | 상대적으로 느림 |

---

## 11.2 __try / __except / __finally

### 기본 구조
```c
__try {
    // 보호할 코드
    int* p = NULL;
    *p = 10;  // 예외 발생!
}
__except (EXCEPTION_EXECUTE_HANDLER) {
    // 예외 처리
    printf("예외 발생!\n");
}
```

### 필터 표현식
```c
__except (filter_expression) {
    // ...
}

// filter_expression 반환값:
// EXCEPTION_EXECUTE_HANDLER (1)  - 이 핸들러에서 처리
// EXCEPTION_CONTINUE_SEARCH (0)  - 다음 핸들러 찾기
// EXCEPTION_CONTINUE_EXECUTION (-1) - 예외 발생 지점으로 돌아가 계속
```

### 예외 정보 획득
```c
__try {
    int* p = NULL;
    *p = 10;
}
__except (ExceptionFilter(GetExceptionInformation())) {
    printf("예외 처리됨\n");
}

LONG ExceptionFilter(EXCEPTION_POINTERS* pExInfo) {
    EXCEPTION_RECORD* pRecord = pExInfo->ExceptionRecord;
    CONTEXT* pContext = pExInfo->ContextRecord;

    printf("예외 코드: 0x%08X\n", pRecord->ExceptionCode);
    printf("예외 주소: %p\n", pRecord->ExceptionAddress);

    if (pRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        // 0: 읽기, 1: 쓰기, 8: DEP
        printf("접근 유형: %s\n",
            pRecord->ExceptionInformation[0] == 0 ? "읽기" : "쓰기");
        printf("접근 주소: %p\n",
            (void*)pRecord->ExceptionInformation[1]);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
```

### __finally (정리 코드)
```c
HANDLE hFile = INVALID_HANDLE_VALUE;

__try {
    hFile = CreateFileW(...);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;  // __finally 실행됨!
    }
    // 파일 작업...

    if (errorCondition) {
        __leave;  // try 블록 종료, __finally로 이동
    }
}
__finally {
    // 예외 발생 여부와 관계없이 항상 실행
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }

    if (AbnormalTermination()) {
        printf("비정상 종료 (예외 또는 return)\n");
    }
}
```

---

## 11.3 벡터화된 예외 처리 (VEH)

### VEH란?
SEH보다 먼저 호출되는 전역 예외 핸들러입니다.

```c
LONG WINAPI VectoredExceptionHandler(EXCEPTION_POINTERS* pExInfo) {
    printf("[VEH] 예외 코드: 0x%08X\n", pExInfo->ExceptionRecord->ExceptionCode);

    // 예외 처리 또는 계속
    return EXCEPTION_CONTINUE_SEARCH;  // SEH로 전달
}

// 등록
PVOID hVeh = AddVectoredExceptionHandler(
    1,  // 1: 첫 번째로 호출, 0: 마지막으로 호출
    VectoredExceptionHandler
);

// 해제
RemoveVectoredExceptionHandler(hVeh);
```

### 예외 처리 순서
```
예외 발생
    │
    ▼
┌───────────────────┐
│ VEH (First)       │ ← AddVectoredExceptionHandler(..., 1, ...)
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│ SEH               │ ← __try / __except
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│ VEH (Last)        │ ← AddVectoredExceptionHandler(..., 0, ...)
└─────────┬─────────┘
          │
          ▼
┌───────────────────┐
│ Unhandled Exception│
│ Filter            │ ← SetUnhandledExceptionFilter
└─────────┬─────────┘
          │
          ▼
     프로세스 종료
```

---

## 11.4 미니덤프 생성

### 자동 덤프 생성기
```c
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExInfo) {
    // 덤프 파일 생성
    WCHAR dumpPath[MAX_PATH];
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(dumpPath, L"crash_%04d%02d%02d_%02d%02d%02d.dmp",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = pExInfo;
        mei.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpWithFullMemory,  // 또는 MiniDumpNormal
            &mei,
            NULL,
            NULL
        );

        CloseHandle(hFile);
        printf("덤프 생성: %ls\n", dumpPath);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// 프로그램 시작 시 등록
SetUnhandledExceptionFilter(UnhandledExceptionHandler);
```

### 덤프 타입
| 타입 | 크기 | 내용 |
|------|------|------|
| MiniDumpNormal | 작음 | 스택, 레지스터만 |
| MiniDumpWithDataSegs | 중간 | + 전역 변수 |
| MiniDumpWithFullMemory | 큼 | 전체 메모리 |

---

## 게임서버 예외 처리 패턴

### 안전한 워커 스레드
```c
DWORD WINAPI WorkerThread(LPVOID param) {
    while (g_running) {
        __try {
            ProcessClientPacket();
        }
        __except (ServerExceptionFilter(GetExceptionInformation())) {
            LogError("패킷 처리 중 예외");
            // 해당 클라이언트만 연결 종료
            DisconnectCurrentClient();
        }
    }
    return 0;
}

LONG ServerExceptionFilter(EXCEPTION_POINTERS* pEx) {
    DWORD code = pEx->ExceptionRecord->ExceptionCode;

    // 치명적 예외는 덤프 후 종료
    if (code == EXCEPTION_STACK_OVERFLOW) {
        WriteCrashDump(pEx);
        return EXCEPTION_CONTINUE_SEARCH;  // 종료
    }

    // 복구 가능한 예외는 처리
    return EXCEPTION_EXECUTE_HANDLER;
}
```

### 스택 오버플로우 복구
```c
// 스택 오버플로우는 특별 처리 필요
__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW ?
          EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {

    // 스택 가드 페이지 복원
    _resetstkoflw();

    LogError("스택 오버플로우 복구됨");
}
```
