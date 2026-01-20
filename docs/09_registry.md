# 9장: 레지스트리

## 9.1 레지스트리 구조와 하이브

### 레지스트리란?
Windows의 계층적 데이터베이스로, 시스템 설정과 응용 프로그램 설정을 저장합니다.

### 루트 키 (Hives)
```
HKEY_CLASSES_ROOT (HKCR)     ← 파일 확장자, COM 객체
    │
HKEY_CURRENT_USER (HKCU)     ← 현재 사용자 설정
    │
HKEY_LOCAL_MACHINE (HKLM)    ← 시스템 전체 설정
    │
HKEY_USERS (HKU)             ← 모든 사용자 프로필
    │
HKEY_CURRENT_CONFIG (HKCC)   ← 현재 하드웨어 프로필
```

### 레지스트리 구조
```
HKEY_LOCAL_MACHINE
└── SOFTWARE
    └── MyGameServer
        ├── Settings
        │   ├── Port (REG_DWORD) = 9000
        │   ├── MaxPlayers (REG_DWORD) = 1000
        │   └── ServerName (REG_SZ) = "GameServer1"
        └── Paths
            └── DataPath (REG_SZ) = "C:\GameData"
```

### 데이터 타입
| 타입 | 설명 |
|------|------|
| REG_SZ | 문자열 |
| REG_EXPAND_SZ | 확장 가능한 문자열 (%PATH%) |
| REG_MULTI_SZ | 다중 문자열 (널 구분) |
| REG_DWORD | 32비트 정수 |
| REG_QWORD | 64비트 정수 |
| REG_BINARY | 바이너리 데이터 |

---

## 9.2 레지스트리 읽기/쓰기

### 키 열기
```c
HKEY hKey;
LONG result = RegOpenKeyExW(
    HKEY_LOCAL_MACHINE,
    L"SOFTWARE\\MyGameServer\\Settings",
    0,
    KEY_READ,  // 또는 KEY_WRITE, KEY_ALL_ACCESS
    &hKey
);

if (result == ERROR_SUCCESS) {
    // 사용...
    RegCloseKey(hKey);
}
```

### 값 읽기
```c
// DWORD 읽기
DWORD port, size = sizeof(port);
RegQueryValueExW(hKey, L"Port", NULL, NULL, (LPBYTE)&port, &size);

// 문자열 읽기
WCHAR serverName[256];
DWORD nameSize = sizeof(serverName);
RegQueryValueExW(hKey, L"ServerName", NULL, NULL,
                 (LPBYTE)serverName, &nameSize);
```

### 키 생성 및 값 쓰기
```c
HKEY hKey;
DWORD disposition;

// 키 생성 (없으면 생성, 있으면 열기)
RegCreateKeyExW(
    HKEY_LOCAL_MACHINE,
    L"SOFTWARE\\MyGameServer\\Settings",
    0, NULL,
    REG_OPTION_NON_VOLATILE,
    KEY_WRITE,
    NULL,
    &hKey,
    &disposition  // REG_CREATED_NEW_KEY 또는 REG_OPENED_EXISTING_KEY
);

// 값 쓰기
DWORD port = 9000;
RegSetValueExW(hKey, L"Port", 0, REG_DWORD, (LPBYTE)&port, sizeof(port));

WCHAR name[] = L"GameServer1";
RegSetValueExW(hKey, L"ServerName", 0, REG_SZ,
               (LPBYTE)name, (wcslen(name) + 1) * sizeof(WCHAR));

RegCloseKey(hKey);
```

---

## 9.3 레지스트리 변경 감시

### RegNotifyChangeKeyValue
```c
HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

// 비동기 감시 등록
RegNotifyChangeKeyValue(
    hKey,
    TRUE,                           // 하위 키 포함
    REG_NOTIFY_CHANGE_LAST_SET,     // 값 변경 감시
    hEvent,
    TRUE                            // 비동기
);

// 변경 대기
while (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
    printf("레지스트리 변경 감지!\n");
    ResetEvent(hEvent);

    // 다시 감시 등록
    RegNotifyChangeKeyValue(hKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET,
                           hEvent, TRUE);
}
```

### 감시 플래그
| 플래그 | 감시 대상 |
|--------|-----------|
| REG_NOTIFY_CHANGE_NAME | 키 추가/삭제 |
| REG_NOTIFY_CHANGE_ATTRIBUTES | 속성 변경 |
| REG_NOTIFY_CHANGE_LAST_SET | 값 변경 |
| REG_NOTIFY_CHANGE_SECURITY | 보안 설정 변경 |

---

## 9.4 게임서버 설정 관리

### 설정 클래스 예제
```c
class ServerConfig {
private:
    HKEY m_hKey;

public:
    ServerConfig() : m_hKey(NULL) {
        RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\MyGameServer", 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
            NULL, &m_hKey, NULL);
    }

    ~ServerConfig() {
        if (m_hKey) RegCloseKey(m_hKey);
    }

    DWORD GetPort(DWORD defaultValue = 9000) {
        DWORD value, size = sizeof(value);
        if (RegQueryValueExW(m_hKey, L"Port", NULL, NULL,
            (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            return value;
        }
        return defaultValue;
    }

    void SetPort(DWORD port) {
        RegSetValueExW(m_hKey, L"Port", 0, REG_DWORD,
                       (LPBYTE)&port, sizeof(port));
    }

    std::wstring GetServerName() {
        WCHAR buffer[256];
        DWORD size = sizeof(buffer);
        if (RegQueryValueExW(m_hKey, L"ServerName", NULL, NULL,
            (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
            return buffer;
        }
        return L"DefaultServer";
    }
};
```

### 주의사항
- **HKLM 쓰기는 관리자 권한 필요**
- **HKCU는 사용자별 설정에 사용**
- **대용량 데이터는 레지스트리에 저장하지 말 것** (파일 사용)
- **백업 고려**: RegSaveKey / RegRestoreKey
