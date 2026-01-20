# 13장: 고급 주제

## 13.1 COM (Component Object Model) 기초

### COM이란?
언어 독립적인 컴포넌트 기반 프로그래밍 모델입니다.

### COM 인터페이스
```c
// IUnknown - 모든 COM 인터페이스의 기반
interface IUnknown {
    HRESULT QueryInterface(REFIID riid, void** ppv);
    ULONG AddRef();
    ULONG Release();
};

// 사용자 정의 인터페이스
interface IGameServer : public IUnknown {
    HRESULT Start(int port);
    HRESULT Stop();
    HRESULT GetPlayerCount(int* count);
};
```

### COM 사용 기본 패턴
```c
// COM 초기화
CoInitializeEx(NULL, COINIT_MULTITHREADED);

// COM 객체 생성
IGameServer* pServer = NULL;
HRESULT hr = CoCreateInstance(
    CLSID_GameServer,
    NULL,
    CLSCTX_INPROC_SERVER,
    IID_IGameServer,
    (void**)&pServer
);

if (SUCCEEDED(hr)) {
    pServer->Start(9000);
    // ...
    pServer->Release();  // 참조 해제
}

// COM 정리
CoUninitialize();
```

### 스마트 포인터 (ATL)
```cpp
#include <atlbase.h>

CComPtr<IGameServer> spServer;
spServer.CoCreateInstance(CLSID_GameServer);
spServer->Start(9000);
// 자동으로 Release 호출됨
```

---

## 13.2 Windows 내부 구조 (Undocumented APIs)

### Native API (ntdll.dll)
```c
// 문서화되지 않은 Native API 예제
typedef NTSTATUS (NTAPI* NtQuerySystemInformation_t)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
auto NtQuerySystemInformation = (NtQuerySystemInformation_t)
    GetProcAddress(hNtdll, "NtQuerySystemInformation");

// 프로세스 목록 조회
BYTE buffer[1024 * 1024];
NtQuerySystemInformation(SystemProcessInformation, buffer, sizeof(buffer), NULL);
```

### PEB (Process Environment Block)
```c
// 현재 프로세스의 PEB 접근 (x64)
#include <winternl.h>

PEB* pPeb = NtCurrentTeb()->ProcessEnvironmentBlock;

// 로드된 모듈 목록
PEB_LDR_DATA* pLdr = pPeb->Ldr;
LIST_ENTRY* pHead = &pLdr->InMemoryOrderModuleList;
LIST_ENTRY* pEntry = pHead->Flink;

while (pEntry != pHead) {
    LDR_DATA_TABLE_ENTRY* pModule = CONTAINING_RECORD(
        pEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
    printf("Module: %ls\n", pModule->FullDllName.Buffer);
    pEntry = pEntry->Flink;
}
```

### 주의사항
- 문서화되지 않은 API는 Windows 버전마다 다를 수 있음
- 프로덕션 코드에서는 신중히 사용
- 호환성 테스트 필수

---

## 13.3 커널 드라이버 개요

### 드라이버 종류
```
┌─────────────────────────────────────────────────────────────────┐
│                      드라이버 스택                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Upper Filter Driver                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Function Driver (기능 드라이버)             │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Lower Filter Driver                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Bus Driver (버스 드라이버)                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                          하드웨어                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### WDK (Windows Driver Kit)
```c
// 간단한 드라이버 예제 (개념)
#include <ntddk.h>

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    DbgPrint("Driver loaded!\n");

    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    DbgPrint("Driver unloaded!\n");
}
```

### 드라이버와 통신 (유저 모드)
```c
// DeviceIoControl로 드라이버와 통신
HANDLE hDevice = CreateFileW(
    L"\\\\.\\MyDevice",
    GENERIC_READ | GENERIC_WRITE,
    0, NULL, OPEN_EXISTING, 0, NULL
);

DWORD bytesReturned;
DeviceIoControl(
    hDevice,
    IOCTL_MY_OPERATION,
    inputBuffer, inputSize,
    outputBuffer, outputSize,
    &bytesReturned,
    NULL
);

CloseHandle(hDevice);
```

---

## 13.4 최신 Windows API

### WinRT / UWP
```cpp
// C++/WinRT 예제
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Networking.Sockets.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Networking::Sockets;

IAsyncAction ConnectAsync() {
    StreamSocket socket;
    co_await socket.ConnectAsync(HostName(L"localhost"), L"9000");
    // 비동기 작업...
}
```

### Windows Thread Pool API
```c
// 최신 스레드 풀 API (Vista+)
PTP_WORK pWork = CreateThreadpoolWork(WorkCallback, context, NULL);
SubmitThreadpoolWork(pWork);
WaitForThreadpoolWorkCallbacks(pWork, FALSE);
CloseThreadpoolWork(pWork);

// 타이머 콜백
PTP_TIMER pTimer = CreateThreadpoolTimer(TimerCallback, context, NULL);
FILETIME dueTime;
// 100ns 단위, 음수 = 상대 시간
LARGE_INTEGER li = { .QuadPart = -10000000LL };  // 1초 후
dueTime.dwLowDateTime = li.LowPart;
dueTime.dwHighDateTime = li.HighPart;
SetThreadpoolTimer(pTimer, &dueTime, 1000, 0);  // 1초마다 반복
```

### Async I/O 패턴 (C++20 코루틴)
```cpp
// 개념적 예제 (실제 구현은 더 복잡)
task<void> ProcessClientAsync(Socket client) {
    while (true) {
        auto buffer = co_await client.ReadAsync(4096);
        if (buffer.empty()) break;

        // 패킷 처리
        auto response = ProcessPacket(buffer);

        co_await client.WriteAsync(response);
    }
}
```

---

## 요약: 게임서버 아키텍처

### 권장 기술 스택
```
┌─────────────────────────────────────────────────────────────────┐
│                    게임서버 기술 스택                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  네트워크: IOCP + Winsock2 (5장)                               │
│  스레드: Thread Pool + SRWLOCK (2장, 4장)                      │
│  메모리: Memory Pool + Ring Buffer (3장)                        │
│  동기화: Interlocked + Lock-free (4장)                         │
│  예외처리: SEH + Mini Dump (11장)                              │
│  배포: Windows Service (10장)                                  │
│  설정: Registry 또는 Config File (9장)                         │
│  로깅: ETW 또는 File-based (12장)                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 핵심 설계 원칙
1. **비동기 I/O**: 블로킹 최소화
2. **락 최소화**: Lock-free 자료구조 활용
3. **메모리 풀링**: 동적 할당 감소
4. **예외 안전**: 서버 안정성 보장
5. **모니터링**: 성능 측정 및 로깅
