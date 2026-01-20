# 8장: 보안과 권한

## 8.1 보안 식별자 (SID)

### SID란?
SID(Security Identifier)는 사용자, 그룹, 컴퓨터 등을 고유하게 식별하는 값입니다.

```
SID 구조: S-1-5-21-1234567890-1234567890-1234567890-1001
         │ │ │  └─────────────────────────────────────┘ └──┘
         │ │ │              도메인 식별자              상대 ID
         │ │ └── 권한 식별자
         │ └── 리비전
         └── 문자열 형식 접두사
```

### 잘 알려진 SID
| SID | 의미 |
|-----|------|
| S-1-5-18 | Local System |
| S-1-5-19 | Local Service |
| S-1-5-20 | Network Service |
| S-1-5-32-544 | Administrators 그룹 |
| S-1-5-32-545 | Users 그룹 |

### SID 조회
```c
PSID pSid = NULL;
DWORD sidSize = 0, domainSize = 0;
SID_NAME_USE sidType;

// 크기 확인
LookupAccountNameW(NULL, L"Administrator", NULL, &sidSize,
                   NULL, &domainSize, &sidType);

// SID 획득
pSid = (PSID)malloc(sidSize);
WCHAR* domain = (WCHAR*)malloc(domainSize * sizeof(WCHAR));

LookupAccountNameW(NULL, L"Administrator", pSid, &sidSize,
                   domain, &domainSize, &sidType);

// SID를 문자열로 변환
LPWSTR sidString;
ConvertSidToStringSidW(pSid, &sidString);
printf("SID: %ls\n", sidString);

LocalFree(sidString);
free(domain);
free(pSid);
```

---

## 8.2 액세스 토큰 (Access Token)

### 액세스 토큰이란?
프로세스/스레드의 보안 컨텍스트를 나타내는 객체입니다.

```
┌─────────────────────────────────────────────────────────────────┐
│                      액세스 토큰 구조                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 사용자 SID                                               │   │
│  │ S-1-5-21-...-1001 (현재 사용자)                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 그룹 SID 목록                                            │   │
│  │ • Administrators                                        │   │
│  │ • Users                                                 │   │
│  │ • Everyone                                              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 권한 (Privileges)                                        │   │
│  │ • SeDebugPrivilege (디버깅)                             │   │
│  │ • SeBackupPrivilege (백업)                              │   │
│  │ • SeShutdownPrivilege (종료)                            │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ 무결성 수준 (Integrity Level)                            │   │
│  │ • Untrusted / Low / Medium / High / System              │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 토큰 획득 및 정보 조회
```c
HANDLE hToken;
OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken);

// 사용자 정보
TOKEN_USER* pUser = NULL;
DWORD size = 0;
GetTokenInformation(hToken, TokenUser, NULL, 0, &size);
pUser = (TOKEN_USER*)malloc(size);
GetTokenInformation(hToken, TokenUser, pUser, size, &size);

// 권한 활성화
TOKEN_PRIVILEGES tp = { 0 };
tp.PrivilegeCount = 1;
LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
```

---

## 8.3 보안 기술자 (Security Descriptor)

### 보안 기술자 구성
```
┌─────────────────────────────────────────────────────────────────┐
│                    Security Descriptor                          │
├─────────────────────────────────────────────────────────────────┤
│  Owner SID     │ 소유자                                         │
├────────────────┼────────────────────────────────────────────────┤
│  Group SID     │ 주 그룹                                        │
├────────────────┼────────────────────────────────────────────────┤
│  DACL          │ Discretionary ACL (누가 접근 가능?)            │
│                │ ┌──────────────────────────────────────────┐   │
│                │ │ ACE 1: Allow Admin Full Control         │   │
│                │ │ ACE 2: Allow Users Read                 │   │
│                │ │ ACE 3: Deny Guest All                   │   │
│                │ └──────────────────────────────────────────┘   │
├────────────────┼────────────────────────────────────────────────┤
│  SACL          │ System ACL (감사 설정)                         │
└────────────────┴────────────────────────────────────────────────┘
```

### 보안 기술자 사용
```c
SECURITY_ATTRIBUTES sa = { 0 };
sa.nLength = sizeof(sa);
sa.bInheritHandle = FALSE;

// 문자열로 보안 기술자 생성 (SDDL)
ConvertStringSecurityDescriptorToSecurityDescriptorW(
    L"D:(A;;GA;;;BA)(A;;GR;;;BU)",  // Admin: Full, Users: Read
    SDDL_REVISION_1,
    &sa.lpSecurityDescriptor,
    NULL
);

// 파일 생성 시 적용
HANDLE hFile = CreateFileW(L"secure.txt", ..., &sa, ...);
```

---

## 8.4 권한 상승 이해

### UAC (User Account Control)
```
일반 사용자 프로세스         관리자 권한 프로세스
┌─────────────────┐          ┌─────────────────┐
│ Medium IL       │   UAC    │ High IL         │
│ 제한된 토큰     │ ──────>  │ 전체 토큰       │
└─────────────────┘  프롬프트 └─────────────────┘
```

### 관리자 권한 확인
```c
BOOL IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup);

    CheckTokenMembership(NULL, adminGroup, &isAdmin);

    FreeSid(adminGroup);
    return isAdmin;
}
```

### 권한 상승 요청
```c
// 관리자 권한으로 재실행
ShellExecuteW(NULL, L"runas", L"MyApp.exe", NULL, NULL, SW_SHOWNORMAL);
```

---

## 8.5 무결성 수준 (Integrity Levels)

### 무결성 수준
| 레벨 | 값 | 용도 |
|------|-----|------|
| Untrusted | 0x0000 | 가장 제한적 |
| Low | 0x1000 | 샌드박스 (IE 보호 모드) |
| Medium | 0x2000 | 일반 사용자 프로세스 |
| High | 0x3000 | 관리자 프로세스 |
| System | 0x4000 | 시스템 서비스 |

### 무결성 수준 확인
```c
DWORD GetProcessIntegrityLevel() {
    HANDLE hToken;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken);

    TOKEN_MANDATORY_LABEL* pLabel = NULL;
    DWORD size = 0;
    GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &size);
    pLabel = (TOKEN_MANDATORY_LABEL*)malloc(size);
    GetTokenInformation(hToken, TokenIntegrityLevel, pLabel, size, &size);

    DWORD level = *GetSidSubAuthority(pLabel->Label.Sid,
        (DWORD)(UCHAR)(*GetSidSubAuthorityCount(pLabel->Label.Sid) - 1));

    free(pLabel);
    CloseHandle(hToken);
    return level;
}
```

---

## 게임서버 보안 고려사항

1. **최소 권한 원칙**: 필요한 권한만 사용
2. **서비스 계정**: Local Service 또는 Network Service 사용
3. **네트워크 보안**: 방화벽, 암호화
4. **입력 검증**: 패킷 유효성 검사
5. **로깅**: 보안 이벤트 기록
