# Windows 버전 히스토리 (시스템 프로그래밍 관점)

## 버전 계보

```
DOS 기반                          NT 기반
─────────                         ─────────
Windows 1.0 (1985)
    ↓
Windows 3.1 (1992)                Windows NT 3.1 (1993)
    ↓                                 ↓
Windows 95 (1995) ←─────────────→ Windows NT 4.0 (1996)
    ↓                                 ↓
Windows 98 (1998)                 Windows 2000 (NT 5.0)
    ↓                                 ↓
Windows ME (2000)                 Windows XP (NT 5.1) ←─── 통합
                                      ↓
                                  Windows Vista (NT 6.0)
                                      ↓
                                  Windows 7 (NT 6.1)
                                      ↓
                                  Windows 8 (NT 6.2)
                                      ↓
                                  Windows 8.1 (NT 6.3)
                                      ↓
                                  Windows 10 (NT 10.0)
                                      ↓
                                  Windows 11 (NT 10.0)
```

---

## 주요 버전별 핵심 변화

### Windows NT 3.1 (1993)
- **NT 커널 탄생**: 완전히 새로운 32비트 커널
- HAL(Hardware Abstraction Layer) 도입
- 선점형 멀티태스킹
- 보호된 메모리 (프로세스 격리)
- NTFS 파일 시스템
- Win32 API 기반

### Windows NT 4.0 (1996)
- GDI를 커널 모드로 이동 (성능 향상)
- DirectX 지원

### Windows 2000 (NT 5.0, 2000)
- **Active Directory**
- Plug and Play 완전 지원
- **I/O Completion Port 개선**
- 암호화 파일 시스템 (EFS)
- 커널 모드 드라이버 모델 (WDM)

### Windows XP (NT 5.1, 2001)
- DOS 기반과 NT 통합 (소비자/기업 통합)
- **Windows 방화벽**
- 원격 데스크톱
- ClearType 폰트
- 64비트 에디션 (Itanium, x64)
- DEP(Data Execution Prevention) - SP2

### Windows Vista (NT 6.0, 2006)
- **UAC (User Account Control)** - 권한 분리
- **무결성 수준 (Integrity Levels)** - Low/Medium/High
- **ASLR (Address Space Layout Randomization)**
- **Windows Filtering Platform** (네트워크 필터링)
- WDDM (Windows Display Driver Model)
- ReadyBoost, SuperFetch
- **Transactional NTFS (TxF)**
- IPv6 기본 지원
- 새 스레드 풀 API (CreateThreadpoolWork 등)

### Windows 7 (NT 6.1, 2009)
- DirectX 11
- 멀티터치 지원
- **개선된 IOCP 성능**
- AppLocker (프로그램 실행 제어)
- 라이브러리 폴더
- 점프 리스트

### Windows 8 (NT 6.2, 2012)
- **WinRT (Windows Runtime)** - 새로운 API 모델
- UWP (Universal Windows Platform) 앱
- UEFI Secure Boot
- Hyper-V 내장
- ReFS 파일 시스템
- **Storage Spaces**
- NVMe 네이티브 지원

### Windows 8.1 (NT 6.3, 2013)
- 시작 버튼 복귀
- Miracast 지원
- 3D 프린팅 API

### Windows 10 (NT 10.0, 2015~)
- **WSL (Windows Subsystem for Linux)**
- Windows Hello (생체 인증)
- DirectX 12
- **Virtual Secure Mode / Credential Guard**
- **Memory Integrity (HVCI)**
- 반기 업데이트 모델
- **WSL 2** (2019) - 실제 Linux 커널
- **Windows Terminal**
- **WinGet** 패키지 관리자
- ARM64 지원 확대

### Windows 11 (NT 10.0, 2021~)
- TPM 2.0 필수
- Android 앱 지원 (WSA)
- DirectStorage (게임용 빠른 로딩)
- Auto HDR
- **Pluton 보안 프로세서** 지원
- 새 UI (Fluent Design)

---

## 시스템 프로그래밍 주요 변화 요약

| 버전 | 핵심 변화 | 영향 |
|------|-----------|------|
| NT 3.1 | NT 커널 탄생 | 현대 Windows의 기반 |
| 2000 | IOCP 개선 | 고성능 서버 가능 |
| XP SP2 | DEP | 보안 강화, 코드 실행 제한 |
| Vista | UAC, ASLR, IL | 보안 모델 전면 개편 |
| 7 | IOCP 성능 | 게임서버 성능 향상 |
| 8 | WinRT | 새로운 API 패러다임 |
| 10 | WSL | Linux 개발 통합 |

---

## 게임서버 개발자 관점 핵심

### Windows Server 버전
| 클라이언트 | 서버 | 특징 |
|------------|------|------|
| XP | Server 2003 | 안정적인 IOCP |
| Vista/7 | Server 2008/R2 | 개선된 네트워크 스택 |
| 8/8.1 | Server 2012/R2 | SMB 3.0, ReFS |
| 10 | Server 2016/2019/2022 | 컨테이너, 보안 강화 |

### API 호환성
```c
// 버전 확인
OSVERSIONINFOEXW osvi = { sizeof(osvi) };
GetVersionExW((OSVERSIONINFOW*)&osvi);  // deprecated

// 권장: VersionHelpers.h (Windows 8.1+)
#include <VersionHelpers.h>
if (IsWindows10OrGreater()) { ... }
if (IsWindowsServer()) { ... }

// 또는 RtlGetVersion (ntdll.dll)
```

### 주요 API 도입 시기
| API | 도입 버전 | 용도 |
|-----|-----------|------|
| IOCP | NT 3.5 | 비동기 I/O |
| Thread Pool API | Vista | 스레드 관리 |
| SRWLock | Vista | 읽기/쓰기 락 |
| Condition Variable | Vista | 조건 대기 |
| InitOnceExecuteOnce | Vista | 스레드 안전 초기화 |
| GetQueuedCompletionStatusEx | Vista | 다중 완료 수신 |

---

## 현재 권장 최소 지원 버전

**게임서버 개발 시 권장**: **Windows Server 2016** 이상
- 모든 최신 API 사용 가능
- 보안 기능 완비
- 컨테이너 지원
- 장기 지원 (LTSC)

**클라이언트**: **Windows 10** 이상
- DirectX 12
- 보안 기능 완비
- 정기 업데이트
