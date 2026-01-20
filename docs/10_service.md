# 10장: 서비스 프로그래밍

## 10.1 Windows 서비스의 개념

### 서비스란?
백그라운드에서 실행되는 프로그램으로, 사용자 로그인 없이도 시스템 부팅 시 자동 시작됩니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                    서비스 vs 일반 프로그램                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  일반 프로그램:                                                  │
│  • 사용자 로그인 필요                                           │
│  • 사용자 세션에서 실행                                         │
│  • 사용자 로그오프 시 종료                                      │
│  • UI 표시 가능                                                 │
│                                                                 │
│  서비스:                                                        │
│  • 시스템 부팅 시 자동 시작 가능                                │
│  • 세션 0 (백그라운드)에서 실행                                 │
│  • 사용자 로그오프와 무관                                       │
│  • UI 직접 표시 불가 (Vista 이후)                              │
│  • 특정 계정으로 실행 (LocalSystem, LocalService 등)           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 서비스 상태
```
                    SERVICE_START_PENDING
                           │
          ┌────────────────┴────────────────┐
          ▼                                 │
    SERVICE_RUNNING ◄───────────────────────┤
          │                                 │
          │ SERVICE_PAUSE_PENDING           │
          ▼                                 │
    SERVICE_PAUSED                          │
          │                                 │
          │ SERVICE_CONTINUE_PENDING        │
          └─────────────────────────────────┘
          │
          │ SERVICE_STOP_PENDING
          ▼
    SERVICE_STOPPED
```

---

## 10.2 서비스 생성과 등록

### 서비스 메인 함수
```c
// 서비스 엔트리 포인트
int main(int argc, char* argv[]) {
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        { L"MyGameServer", (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { NULL, NULL }
    };

    // 서비스 제어 관리자에 연결
    if (!StartServiceCtrlDispatcherW(serviceTable)) {
        // 콘솔 모드로 실행된 경우
        printf("콘솔 모드로 실행\n");
        RunAsConsole();
    }

    return 0;
}
```

### ServiceMain 함수
```c
SERVICE_STATUS g_serviceStatus = { 0 };
SERVICE_STATUS_HANDLE g_statusHandle = NULL;
HANDLE g_stopEvent = NULL;

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    // 서비스 제어 핸들러 등록
    g_statusHandle = RegisterServiceCtrlHandlerW(
        L"MyGameServer",
        ServiceCtrlHandler
    );

    // 서비스 상태 초기화
    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                          SERVICE_ACCEPT_SHUTDOWN;
    g_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_statusHandle, &g_serviceStatus);

    // 초기화
    g_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // 서비스 실행 중
    g_serviceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_statusHandle, &g_serviceStatus);

    // 메인 루프
    RunServer();

    // 종료
    g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_statusHandle, &g_serviceStatus);
}
```

### 서비스 제어 핸들러
```c
void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        g_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_statusHandle, &g_serviceStatus);
        SetEvent(g_stopEvent);  // 종료 신호
        break;

    case SERVICE_CONTROL_PAUSE:
        // 일시 정지 처리
        break;

    case SERVICE_CONTROL_CONTINUE:
        // 재개 처리
        break;

    case SERVICE_CONTROL_INTERROGATE:
        // 상태 조회 (현재 상태 반환)
        break;
    }
}
```

---

## 10.3 서비스 제어 관리자 (SCM)

### 서비스 설치
```c
void InstallService() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    SC_HANDLE hService = CreateServiceW(
        hSCM,
        L"MyGameServer",           // 서비스 이름
        L"My Game Server",         // 표시 이름
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,        // 자동 시작
        SERVICE_ERROR_NORMAL,
        path,                      // 실행 파일 경로
        NULL, NULL, NULL,
        NULL,                      // LocalSystem 계정
        NULL
    );

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
}
```

### 서비스 삭제
```c
void UninstallService() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE hService = OpenServiceW(hSCM, L"MyGameServer", DELETE);

    DeleteService(hService);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
}
```

### 서비스 시작/중지
```c
void StartMyService() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE hService = OpenServiceW(hSCM, L"MyGameServer", SERVICE_START);

    StartServiceW(hService, 0, NULL);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
}

void StopMyService() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE hService = OpenServiceW(hSCM, L"MyGameServer", SERVICE_STOP);

    SERVICE_STATUS status;
    ControlService(hService, SERVICE_CONTROL_STOP, &status);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
}
```

---

## 10.4 서비스 상태 관리

### 상태 보고 패턴
```c
void ReportServiceStatus(DWORD currentState, DWORD waitHint = 0) {
    static DWORD checkPoint = 1;

    g_serviceStatus.dwCurrentState = currentState;
    g_serviceStatus.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING ||
        currentState == SERVICE_STOP_PENDING) {
        g_serviceStatus.dwCheckPoint = checkPoint++;
    } else {
        g_serviceStatus.dwCheckPoint = 0;
    }

    SetServiceStatus(g_statusHandle, &g_serviceStatus);
}

// 사용 예
void ServiceMain(...) {
    ReportServiceStatus(SERVICE_START_PENDING, 3000);

    // 초기화 단계 1
    ReportServiceStatus(SERVICE_START_PENDING, 3000);

    // 초기화 단계 2
    ReportServiceStatus(SERVICE_START_PENDING, 3000);

    // 실행 중
    ReportServiceStatus(SERVICE_RUNNING);

    // 메인 루프...

    // 종료
    ReportServiceStatus(SERVICE_STOPPED);
}
```

---

## 게임서버 서비스 패턴

### 완전한 서비스 예제
```c
class GameServerService {
    SERVICE_STATUS_HANDLE m_statusHandle;
    SERVICE_STATUS m_status;
    HANDLE m_stopEvent;
    GameServer* m_server;

public:
    static GameServerService* s_instance;

    void Run() {
        SERVICE_TABLE_ENTRYW table[] = {
            { L"GameServer", ServiceMainStatic },
            { NULL, NULL }
        };
        StartServiceCtrlDispatcherW(table);
    }

private:
    static void WINAPI ServiceMainStatic(DWORD argc, LPWSTR* argv) {
        s_instance->ServiceMain(argc, argv);
    }

    void ServiceMain(DWORD argc, LPWSTR* argv) {
        m_statusHandle = RegisterServiceCtrlHandlerExW(
            L"GameServer", CtrlHandlerStatic, this);

        SetStatus(SERVICE_START_PENDING);

        m_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        m_server = new GameServer();

        if (m_server->Initialize()) {
            SetStatus(SERVICE_RUNNING);
            m_server->Run(m_stopEvent);
        }

        SetStatus(SERVICE_STOPPED);
    }

    static DWORD WINAPI CtrlHandlerStatic(
        DWORD ctrl, DWORD type, LPVOID data, LPVOID context) {
        return ((GameServerService*)context)->CtrlHandler(ctrl, type, data);
    }

    DWORD CtrlHandler(DWORD ctrl, DWORD type, LPVOID data) {
        if (ctrl == SERVICE_CONTROL_STOP) {
            SetStatus(SERVICE_STOP_PENDING);
            SetEvent(m_stopEvent);
        }
        return NO_ERROR;
    }

    void SetStatus(DWORD state) {
        m_status.dwCurrentState = state;
        SetServiceStatus(m_statusHandle, &m_status);
    }
};
```

### 콘솔/서비스 모드 겸용
```c
int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1 && wcscmp(argv[1], L"-console") == 0) {
        // 콘솔 모드 (디버깅용)
        GameServer server;
        server.Initialize();
        server.RunConsole();
    } else if (argc > 1 && wcscmp(argv[1], L"-install") == 0) {
        InstallService();
    } else if (argc > 1 && wcscmp(argv[1], L"-uninstall") == 0) {
        UninstallService();
    } else {
        // 서비스 모드
        GameServerService::s_instance = new GameServerService();
        GameServerService::s_instance->Run();
    }
    return 0;
}
```
