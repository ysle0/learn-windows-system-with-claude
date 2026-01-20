# 12장: 디버깅과 분석

## 12.1 디버거 API

### 디버거 연결
```c
// 프로세스 디버깅 시작
BOOL DebugActiveProcess(DWORD dwProcessId);

// 디버깅 종료 (프로세스 종료 없이)
BOOL DebugActiveProcessStop(DWORD dwProcessId);

// 자식 프로세스를 디버그 모드로 생성
CreateProcess(..., DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, ...);
```

### 디버그 이벤트 루프
```c
DEBUG_EVENT debugEvent;

while (TRUE) {
    // 디버그 이벤트 대기
    if (!WaitForDebugEvent(&debugEvent, INFINITE)) break;

    DWORD continueStatus = DBG_CONTINUE;

    switch (debugEvent.dwDebugEventCode) {
    case EXCEPTION_DEBUG_EVENT:
        printf("예외: 0x%08X at %p\n",
            debugEvent.u.Exception.ExceptionRecord.ExceptionCode,
            debugEvent.u.Exception.ExceptionRecord.ExceptionAddress);

        if (!debugEvent.u.Exception.dwFirstChance) {
            // 두 번째 기회 예외 (처리되지 않음)
            continueStatus = DBG_EXCEPTION_NOT_HANDLED;
        }
        break;

    case CREATE_PROCESS_DEBUG_EVENT:
        printf("프로세스 생성: PID %lu\n", debugEvent.dwProcessId);
        CloseHandle(debugEvent.u.CreateProcessInfo.hFile);
        break;

    case CREATE_THREAD_DEBUG_EVENT:
        printf("스레드 생성: TID %lu\n", debugEvent.dwThreadId);
        break;

    case EXIT_PROCESS_DEBUG_EVENT:
        printf("프로세스 종료: 코드 %lu\n",
            debugEvent.u.ExitProcess.dwExitCode);
        return;

    case LOAD_DLL_DEBUG_EVENT:
        printf("DLL 로드: %p\n", debugEvent.u.LoadDll.lpBaseOfDll);
        CloseHandle(debugEvent.u.LoadDll.hFile);
        break;

    case OUTPUT_DEBUG_STRING_EVENT:
        // OutputDebugString 출력
        break;
    }

    // 디버기 계속 실행
    ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId,
                       continueStatus);
}
```

---

## 12.2 심볼과 PDB 파일

### PDB (Program Database)
```
┌─────────────────────────────────────────────────────────────────┐
│                    PDB 파일 내용                                 │
├─────────────────────────────────────────────────────────────────┤
│  • 함수 이름과 주소 매핑                                        │
│  • 변수 이름과 타입 정보                                        │
│  • 소스 파일 경로와 라인 번호                                   │
│  • 구조체/클래스 레이아웃                                       │
└─────────────────────────────────────────────────────────────────┘
```

### DbgHelp 심볼 API
```c
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

// 심볼 초기화
SymInitialize(hProcess, NULL, TRUE);

// 주소로 심볼 조회
char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
SYMBOL_INFO* pSymbol = (SYMBOL_INFO*)buffer;
pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
pSymbol->MaxNameLen = MAX_SYM_NAME;

DWORD64 displacement;
if (SymFromAddr(hProcess, address, &displacement, pSymbol)) {
    printf("심볼: %s + 0x%llX\n", pSymbol->Name, displacement);
}

// 라인 번호 조회
IMAGEHLP_LINE64 line;
line.SizeOfStruct = sizeof(line);
DWORD lineDisp;
if (SymGetLineFromAddr64(hProcess, address, &lineDisp, &line)) {
    printf("소스: %s:%lu\n", line.FileName, line.LineNumber);
}

// 정리
SymCleanup(hProcess);
```

### 콜 스택 획득
```c
void CaptureStackTrace() {
    void* stack[64];
    WORD frames = CaptureStackBackTrace(0, 64, stack, NULL);

    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    for (WORD i = 0; i < frames; i++) {
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        SymFromAddr(GetCurrentProcess(), (DWORD64)stack[i], NULL, symbol);
        printf("%2d: %s (0x%p)\n", i, symbol->Name, stack[i]);
    }

    SymCleanup(GetCurrentProcess());
}
```

---

## 12.3 WinDbg 기초

### 주요 명령어
```
// 브레이크포인트
bp MyFunction          // 함수에 브레이크포인트
bp module!function     // 특정 모듈의 함수
bl                     // 브레이크포인트 목록
bc *                   // 모든 브레이크포인트 삭제

// 실행 제어
g                      // 실행 계속
p                      // Step Over
t                      // Step Into
gu                     // Step Out

// 정보 표시
k                      // 콜 스택
~*k                    // 모든 스레드 콜 스택
r                      // 레지스터
dt                     // 구조체 표시
dv                     // 로컬 변수

// 메모리
db address             // 바이트 덤프
dd address             // DWORD 덤프
dq address             // QWORD 덤프
da/du address          // ASCII/유니코드 문자열

// 분석
!analyze -v            // 자동 분석 (크래시 덤프)
lm                     // 로드된 모듈
!heap                  // 힙 분석
!threads               // 스레드 목록
```

### 덤프 파일 분석
```
// 덤프 열기
.opendump c:\crash.dmp

// 분석 시작
!analyze -v

// 충돌 스레드 확인
~. k

// 예외 레코드
.exr -1

// 컨텍스트 레코드
.ecxr
```

---

## 12.4 ETW (Event Tracing for Windows)

### ETW 구조
```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Provider    │───>│  Session     │───>│  Consumer    │
│  (이벤트발생)│    │  (버퍼링)    │    │  (수집/분석) │
└──────────────┘    └──────────────┘    └──────────────┘
```

### 이벤트 발행
```c
#include <evntprov.h>

// 프로바이더 등록
REGHANDLE hProvider;
EventRegister(&PROVIDER_GUID, NULL, NULL, &hProvider);

// 이벤트 발행
EVENT_DESCRIPTOR eventDesc = { 0 };
eventDesc.Id = 1;
eventDesc.Level = TRACE_LEVEL_INFORMATION;
EventWrite(hProvider, &eventDesc, 0, NULL);

// 해제
EventUnregister(hProvider);
```

### 시스템 트레이싱 (xperf)
```batch
:: 시스템 이벤트 수집 시작
xperf -on PROC_THREAD+LOADER+DISK_IO+FILE_IO -f trace.etl

:: 수집 중지
xperf -stop

:: 분석
xperf trace.etl
```

---

## 12.5 성능 카운터

### 성능 모니터링
```c
#include <pdh.h>
#pragma comment(lib, "pdh.lib")

PDH_HQUERY hQuery;
PDH_HCOUNTER hCounter;

// 쿼리 생성
PdhOpenQuery(NULL, 0, &hQuery);

// 카운터 추가
PdhAddCounterW(hQuery, L"\\Processor(_Total)\\% Processor Time",
               0, &hCounter);

// 수집
while (monitoring) {
    PdhCollectQueryData(hQuery);

    PDH_FMT_COUNTERVALUE value;
    PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &value);

    printf("CPU: %.1f%%\n", value.doubleValue);
    Sleep(1000);
}

PdhCloseQuery(hQuery);
```

### 주요 카운터
| 카운터 | 설명 |
|--------|------|
| \Processor(_Total)\% Processor Time | CPU 사용률 |
| \Memory\Available MBytes | 가용 메모리 |
| \Process(name)\Working Set | 프로세스 메모리 |
| \PhysicalDisk(_Total)\Disk Bytes/sec | 디스크 I/O |
| \TCPv4\Connections Established | TCP 연결 수 |

---

## 게임서버 디버깅 팁

### 1. 로깅 시스템
```c
#define LOG_DEBUG(fmt, ...) \
    OutputDebugStringA(FormatLog("DEBUG", fmt, __VA_ARGS__))

#define LOG_ERROR(fmt, ...) do { \
    char* msg = FormatLog("ERROR", fmt, __VA_ARGS__); \
    OutputDebugStringA(msg); \
    WriteToLogFile(msg); \
} while(0)
```

### 2. 프로파일링
```c
class ScopedTimer {
    const char* m_name;
    LARGE_INTEGER m_start;
public:
    ScopedTimer(const char* name) : m_name(name) {
        QueryPerformanceCounter(&m_start);
    }
    ~ScopedTimer() {
        LARGE_INTEGER end, freq;
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&freq);
        double ms = (end.QuadPart - m_start.QuadPart) * 1000.0 / freq.QuadPart;
        if (ms > 10.0) {
            LOG_DEBUG("%s: %.2f ms", m_name, ms);
        }
    }
};

#define PROFILE_SCOPE() ScopedTimer _timer(__FUNCTION__)
```

### 3. 원격 디버깅
```
// 서버에서 디버거 서버 시작
dbgsrv -t tcp:port=1234

// 클라이언트에서 연결
windbg -remote tcp:server=serverip,port=1234
```
