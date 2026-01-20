# 7장: DLL과 모듈

## 7.1 DLL의 개념과 장점

### DLL (Dynamic Link Library)
실행 시간에 로드되는 공유 라이브러리입니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                      정적 링킹 vs 동적 링킹                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  정적 링킹 (.lib):                                              │
│  ┌─────────┐  ┌─────────┐                                      │
│  │ Game.exe│  │Other.exe│   ← 각각 라이브러리 코드 포함         │
│  │ ┌─────┐ │  │ ┌─────┐ │      (메모리 낭비)                    │
│  │ │ LIB │ │  │ │ LIB │ │                                      │
│  │ └─────┘ │  │ └─────┘ │                                      │
│  └─────────┘  └─────────┘                                      │
│                                                                 │
│  동적 링킹 (.dll):                                              │
│  ┌─────────┐  ┌─────────┐                                      │
│  │ Game.exe│  │Other.exe│   ← DLL 코드 공유                    │
│  └────┬────┘  └────┬────┘      (메모리 절약)                    │
│       │            │                                            │
│       └──────┬─────┘                                            │
│              ▼                                                  │
│        ┌─────────┐                                              │
│        │ LIB.dll │   ← 메모리에 한 번만 로드                    │
│        └─────────┘                                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### DLL 장점
1. **메모리 절약**: 여러 프로세스가 코드 공유
2. **모듈화**: 기능별 분리, 유지보수 용이
3. **업데이트**: EXE 수정 없이 DLL만 교체
4. **플러그인**: 런타임에 기능 확장

---

## 7.2 암시적 링킹 vs 명시적 링킹

### 암시적 링킹 (Load-Time Linking)
```c
// 컴파일 시 .lib 파일 필요
#pragma comment(lib, "MyLib.lib")

// 헤더에서 함수 선언
__declspec(dllimport) void MyFunction();

// 직접 호출
MyFunction();
```

### 명시적 링킹 (Run-Time Linking)
```c
// 런타임에 DLL 로드
HMODULE hDll = LoadLibraryW(L"MyLib.dll");

if (hDll) {
    // 함수 주소 획득
    typedef void (*MyFuncType)();
    MyFuncType pFunc = (MyFuncType)GetProcAddress(hDll, "MyFunction");

    if (pFunc) {
        pFunc();  // 함수 호출
    }

    FreeLibrary(hDll);  // 언로드
}
```

### 비교
| 특성 | 암시적 링킹 | 명시적 링킹 |
|------|------------|------------|
| 로드 시점 | 프로그램 시작 | 필요할 때 |
| DLL 없으면 | 실행 안 됨 | 에러 처리 가능 |
| 사용 편의성 | 간단 | 복잡 |
| 플러그인 | 부적합 | 적합 |

---

## 7.3 DllMain과 진입점

### DllMain
```c
BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // DLL 인스턴스 핸들
    DWORD     fdwReason, // 호출 이유
    LPVOID    lpvReserved
) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        // 프로세스에 DLL 로드됨
        // 전역 초기화 (주의: 복잡한 작업 금지!)
        break;

    case DLL_PROCESS_DETACH:
        // 프로세스에서 DLL 언로드됨
        // 전역 정리
        break;

    case DLL_THREAD_ATTACH:
        // 새 스레드 생성됨
        break;

    case DLL_THREAD_DETACH:
        // 스레드 종료됨
        break;
    }
    return TRUE;
}
```

### DllMain 주의사항
```
┌─────────────────────────────────────────────────────────────────┐
│                DllMain에서 하면 안 되는 것들                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ✗ LoadLibrary / FreeLibrary 호출                              │
│  ✗ 다른 DLL의 함수 호출 (순서 보장 안 됨)                       │
│  ✗ 동기화 객체 대기 (데드락 위험)                               │
│  ✗ 레지스트리 읽기/쓰기                                        │
│  ✗ CreateProcess / CreateThread                                │
│  ✗ COM 함수 호출                                               │
│  ✗ 소켓 함수 호출                                              │
│                                                                 │
│  로더 락(Loader Lock) 때문에 교착 상태 발생 가능!               │
│                                                                 │
│  ✓ 간단한 변수 초기화                                          │
│  ✓ TLS 인덱스 할당                                             │
│  ✓ Critical Section 초기화                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 함수 내보내기
```c
// 방법 1: __declspec
__declspec(dllexport) int Add(int a, int b) {
    return a + b;
}

// 방법 2: .def 파일
// MyLib.def:
// LIBRARY MyLib
// EXPORTS
//     Add @1
//     Subtract @2
```

---

## 7.4 DLL 인젝션 기법

### CreateRemoteThread 방식
```c
// 대상 프로세스에 DLL 경로 쓰기
LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathLen,
    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
WriteProcessMemory(hProcess, pRemotePath, dllPath, pathLen, NULL);

// LoadLibraryW 주소 획득
LPVOID pLoadLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
    "LoadLibraryW");

// 원격 스레드로 LoadLibrary 호출
HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
    (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemotePath, 0, NULL);
```

### 용도
- 게임 모딩 / 치트 (비권장)
- 보안 소프트웨어
- 디버깅 도구
- API 후킹

---

## 7.5 후킹 (Hooking) 기초

### IAT (Import Address Table) 후킹
```c
// 원본 함수 저장
typedef int (WINAPI* MessageBoxWFunc)(HWND, LPCWSTR, LPCWSTR, UINT);
MessageBoxWFunc g_originalMessageBoxW;

// 대체 함수
int WINAPI HookedMessageBoxW(HWND hWnd, LPCWSTR lpText,
                              LPCWSTR lpCaption, UINT uType) {
    // 가로채기!
    return g_originalMessageBoxW(hWnd, L"Hooked!", lpCaption, uType);
}

// IAT에서 함수 주소 교체
void HookIAT(HMODULE hModule, const char* dllName, const char* funcName,
             LPVOID hookFunc, LPVOID* originalFunc) {
    // PE 헤더 파싱하여 IAT 찾기
    // IAT 엔트리를 hookFunc 주소로 교체
}
```

### Windows 메시지 후킹
```c
// 전역 키보드 후킹
HHOOK hHook = SetWindowsHookExW(
    WH_KEYBOARD_LL,      // 저수준 키보드
    KeyboardProc,        // 콜백 함수
    hInstance,           // DLL 인스턴스
    0                    // 모든 스레드
);

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKbd = (KBDLLHOOKSTRUCT*)lParam;
        // 키 입력 처리
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}
```

---

## 게임서버에서의 DLL 활용

### 플러그인 시스템
```c
// 플러그인 인터페이스
typedef struct {
    const char* (*GetName)();
    bool (*Initialize)();
    void (*Shutdown)();
    void (*OnPacket)(int packetId, void* data, int len);
} PluginInterface;

// 플러그인 로드
void LoadPlugin(const WCHAR* path) {
    HMODULE hDll = LoadLibraryW(path);
    if (hDll) {
        typedef PluginInterface* (*GetPluginFunc)();
        GetPluginFunc getPlugin = (GetPluginFunc)
            GetProcAddress(hDll, "GetPlugin");

        if (getPlugin) {
            PluginInterface* plugin = getPlugin();
            plugin->Initialize();
            // 플러그인 목록에 추가
        }
    }
}
```
