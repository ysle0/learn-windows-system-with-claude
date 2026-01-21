# 윈도우즈 시스템 프로그래밍 학습 로드맵

## 0. Windows 버전 히스토리 ✅
📄 [docs/00_windows_history.md](docs/00_windows_history.md)

시스템 프로그래밍 관점에서 본 Windows 버전별 핵심 변화

---

## 프로젝트 구조

```
learn-windows-system-with-claude/
├── docs/                    # 챕터별 개념 정리 문서
│   ├── 00_windows_history.md
│   ├── 01_basics.md
│   ├── 02_process_thread.md
│   ├── 03_memory.md
│   ├── 04_synchronization.md
│   ├── 05_file_io.md
│   ├── 06_ipc.md
│   ├── 07_dll.md
│   ├── 08_security.md
│   ├── 09_registry.md
│   ├── 10_service.md
│   ├── 11_seh.md
│   ├── 12_debugging.md
│   └── 13_advanced.md
│
├── src/                     # 예제 코드
│   ├── 01_basics/
│   │   └── system_info.cpp
│   ├── 02_process_thread/
│   │   └── thread_example.cpp
│   ├── 03_memory/
│   │   └── memory_example.cpp
│   ├── 04_synchronization/
│   │   └── sync_example.cpp
│   ├── 05_file_io/
│   │   └── io_example.cpp
│   └── game_server/         # IOCP 게임서버 종합 예제
│       ├── iocp_game_server.cpp
│       └── test_client.cpp
│
└── ROADMAP.md              # 이 파일
```

---

## 1. 기초 개념 ✅
📄 [docs/01_basics.md](docs/01_basics.md) | 💻 [src/01_basics/](src/01_basics/)

- 1.1 운영체제의 역할과 구조
- 1.2 커널 모드 vs 유저 모드
- 1.3 Windows 아키텍처 개요 (NT 커널, HAL, 서브시스템)
- 1.4 Win32 API 소개
- 1.5 핸들과 커널 객체

## 2. 프로세스와 스레드 ✅
📄 [docs/02_process_thread.md](docs/02_process_thread.md) | 💻 [src/02_process_thread/](src/02_process_thread/)

- 2.1 프로세스의 개념과 생명주기
- 2.2 프로세스 생성 (CreateProcess)
- 2.3 스레드의 개념과 생성 (CreateThread)
- 2.4 스레드 동기화 기초 (Critical Section, Mutex)
- 2.5 스레드 풀 (Thread Pool)

## 3. 메모리 관리 ✅
📄 [docs/03_memory.md](docs/03_memory.md) | 💻 [src/03_memory/](src/03_memory/)

- 3.1 가상 메모리 개념
- 3.2 메모리 할당 (VirtualAlloc, HeapAlloc, malloc)
- 3.3 메모리 보호 속성
- 3.4 메모리 매핑 파일 (Memory-Mapped Files)
- 3.5 프로세스 간 메모리 공유 (게임서버용 메모리 풀, 링 버퍼)

## 4. 동기화 객체 ✅
📄 [docs/04_synchronization.md](docs/04_synchronization.md) | 💻 [src/04_synchronization/](src/04_synchronization/)

- 4.1 커널 객체의 개념
- 4.2 이벤트 (Event)
- 4.3 세마포어 (Semaphore)
- 4.4 뮤텍스 (Mutex)
- 4.5 대기 함수들 (WaitForSingleObject, WaitForMultipleObjects)
- 4.6 고급 동기화 (SRWLock, Condition Variable, Interlocked)

## 5. 파일 시스템과 I/O ✅
📄 [docs/05_file_io.md](docs/05_file_io.md) | 💻 [src/05_file_io/](src/05_file_io/)

- 5.1 파일 핸들과 기본 I/O
- 5.2 동기 vs 비동기 I/O
- 5.3 Overlapped I/O
- 5.4 **I/O Completion Port (IOCP)** ⭐ 핵심!
- 5.5 디렉토리 변경 감시

## 6. 프로세스 간 통신 (IPC) ✅
📄 [docs/06_ipc.md](docs/06_ipc.md)

- 6.1 파이프 (Anonymous Pipe, Named Pipe)
- 6.2 메일슬롯 (Mailslot)
- 6.3 공유 메모리
- 6.4 소켓 (Winsock)
- 6.5 RPC (Remote Procedure Call)

## 7. DLL과 모듈 ✅
📄 [docs/07_dll.md](docs/07_dll.md)

- 7.1 DLL의 개념과 장점
- 7.2 암시적 링킹 vs 명시적 링킹
- 7.3 DllMain과 진입점
- 7.4 DLL 인젝션 기법
- 7.5 후킹 (Hooking) 기초

## 8. 보안과 권한 ✅
📄 [docs/08_security.md](docs/08_security.md)

- 8.1 보안 식별자 (SID)
- 8.2 액세스 토큰 (Access Token)
- 8.3 보안 기술자 (Security Descriptor)
- 8.4 권한 상승 (Privilege Escalation) 이해
- 8.5 무결성 수준 (Integrity Levels)

## 9. 레지스트리 ✅
📄 [docs/09_registry.md](docs/09_registry.md)

- 9.1 레지스트리 구조와 하이브
- 9.2 레지스트리 읽기/쓰기
- 9.3 레지스트리 변경 감시
- 9.4 게임서버 설정 관리

## 10. 서비스 프로그래밍 ✅
📄 [docs/10_service.md](docs/10_service.md)

- 10.1 Windows 서비스의 개념
- 10.2 서비스 생성과 등록
- 10.3 서비스 제어 관리자 (SCM)
- 10.4 서비스 상태 관리

## 11. 구조적 예외 처리 (SEH) ✅
📄 [docs/11_seh.md](docs/11_seh.md)

- 11.1 예외의 개념
- 11.2 __try/__except/__finally
- 11.3 벡터화된 예외 처리 (VEH)
- 11.4 미니덤프 생성

## 12. 디버깅과 분석 ✅
📄 [docs/12_debugging.md](docs/12_debugging.md)

- 12.1 디버거 API
- 12.2 심볼과 PDB 파일
- 12.3 WinDbg 기초
- 12.4 ETW (Event Tracing for Windows)
- 12.5 성능 카운터

## 13. 고급 주제 ✅
📄 [docs/13_advanced.md](docs/13_advanced.md)

- 13.1 COM (Component Object Model) 기초
- 13.2 Windows 내부 구조 (Undocumented APIs)
- 13.3 커널 드라이버 개요
- 13.4 최신 Windows API (WinRT, UWP)

---

## 🎮 IOCP 게임서버 실전 예제 ✅
💻 [src/game_server/](src/game_server/)

1~13장에서 배운 내용을 종합한 **완전한 IOCP 기반 게임서버**:

- `iocp_game_server.cpp` - IOCP 서버 (로그인, 채팅, 이동 처리)
- `test_client.cpp` - 테스트 클라이언트

### 컴파일 방법 (Visual Studio Developer Command Prompt)
```batch
cd src/game_server
cl /EHsc /W4 iocp_game_server.cpp ws2_32.lib dbghelp.lib
cl /EHsc /W4 test_client.cpp ws2_32.lib
```

### 실행 방법
```batch
:: 서버 실행
iocp_game_server.exe

:: 다른 터미널에서 클라이언트 실행
test_client.exe
```

---

## 추천 학습 순서

```
기초 개념 (1장)
    ↓
프로세스/스레드 (2장) ← 가장 중요한 기초
    ↓
메모리 관리 (3장)
    ↓
동기화 (4장)
    ↓
파일 I/O (5장) ← IOCP 핵심!
    ↓
IPC (6장)
    ↓
DLL (7장)
    ↓
나머지 주제들 (8~13장)
    ↓
IOCP 게임서버 종합 예제 ← 최종 목표!
```

---

## 참고 자료

### 필독서
- "Windows Internals" - Mark Russinovich, David Solomon
- "Windows via C/C++" - Jeffrey Richter
- "Windows System Programming" - Johnson M. Hart

### 온라인 자료
- [Microsoft Learn](https://learn.microsoft.com/windows/)
- [Sysinternals Suite](https://learn.microsoft.com/sysinternals/)

---

## 게임서버 개발 핵심 기술 요약

| 기술 | 관련 챕터 | 용도 |
|------|-----------|------|
| IOCP | 5장 | 비동기 네트워크 I/O |
| Thread Pool | 2장 | 워커 스레드 관리 |
| Memory Pool | 3장 | 패킷/객체 메모리 관리 |
| SRWLOCK | 4장 | 읽기/쓰기 동기화 |
| Interlocked | 4장 | 원자적 카운터 |
| SEH | 11장 | 예외 처리, 크래시 덤프 |
| Service | 10장 | 백그라운드 서비스 |
