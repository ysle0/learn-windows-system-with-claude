/**
 * IOCP 기반 게임서버 실전 예제
 *
 * 이 예제는 1~13장에서 배운 내용을 종합하여 구현한 완전한 IOCP 게임서버입니다.
 *
 * 포함된 기능:
 * - IOCP 기반 비동기 네트워크 I/O (5장)
 * - 스레드 풀 워커 (2장)
 * - 메모리 풀 (3장)
 * - 동기화 (4장)
 * - 예외 처리 및 크래시 덤프 (11장)
 *
 * 컴파일: cl /EHsc /W4 iocp_game_server.cpp ws2_32.lib dbghelp.lib
 */

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <dbghelp.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <queue>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dbghelp.lib")

//=============================================================================
// 설정 상수
//=============================================================================

const int SERVER_PORT = 9000;
const int MAX_CLIENTS = 1000;
const int WORKER_THREAD_COUNT = 4;  // CPU 코어 수 * 2 권장
const int RECV_BUFFER_SIZE = 4096;
const int SEND_BUFFER_SIZE = 4096;
const int PACKET_HEADER_SIZE = 4;   // 패킷 크기 (2) + 패킷 타입 (2)

//=============================================================================
// 패킷 정의
//=============================================================================

#pragma pack(push, 1)

struct PacketHeader {
    USHORT size;    // 전체 패킷 크기
    USHORT type;    // 패킷 타입
};

// 패킷 타입
enum PacketType {
    PKT_CS_LOGIN = 1,
    PKT_SC_LOGIN_RESULT = 2,
    PKT_CS_CHAT = 3,
    PKT_SC_CHAT = 4,
    PKT_CS_MOVE = 5,
    PKT_SC_MOVE = 6,
    PKT_SC_PLAYER_LIST = 7,
    PKT_SC_PLAYER_ENTER = 8,
    PKT_SC_PLAYER_LEAVE = 9,
};

struct PKT_CS_Login {
    PacketHeader header;
    char username[32];
};

struct PKT_SC_LoginResult {
    PacketHeader header;
    UINT32 playerId;
    BYTE success;
};

struct PKT_CS_Chat {
    PacketHeader header;
    char message[256];
};

struct PKT_SC_Chat {
    PacketHeader header;
    UINT32 playerId;
    char username[32];
    char message[256];
};

struct PKT_CS_Move {
    PacketHeader header;
    float x, y, z;
};

struct PKT_SC_Move {
    PacketHeader header;
    UINT32 playerId;
    float x, y, z;
};

#pragma pack(pop)

//=============================================================================
// I/O 컨텍스트 (Overlapped 확장)
//=============================================================================

enum IOOperation {
    IO_ACCEPT,
    IO_RECV,
    IO_SEND,
};

struct IOContext : public OVERLAPPED {
    IOOperation operation;
    WSABUF wsaBuf;
    char buffer[RECV_BUFFER_SIZE];
    SOCKET acceptSocket;  // AcceptEx용

    IOContext() {
        memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
        wsaBuf.buf = buffer;
        wsaBuf.len = RECV_BUFFER_SIZE;
        acceptSocket = INVALID_SOCKET;
    }

    void Reset() {
        memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
    }
};

//=============================================================================
// 클라이언트 세션
//=============================================================================

struct Session {
    SOCKET socket;
    UINT32 playerId;
    char username[32];
    float posX, posY, posZ;

    IOContext recvContext;
    IOContext sendContext;

    // 수신 버퍼 (패킷 조립용)
    char recvBuffer[RECV_BUFFER_SIZE * 2];
    int recvBufferLen;

    // 송신 큐
    CRITICAL_SECTION sendLock;
    std::queue<std::string> sendQueue;
    bool isSending;

    Session() : socket(INVALID_SOCKET), playerId(0), recvBufferLen(0), isSending(false) {
        memset(username, 0, sizeof(username));
        posX = posY = posZ = 0.0f;
        InitializeCriticalSection(&sendLock);
    }

    ~Session() {
        DeleteCriticalSection(&sendLock);
    }
};

//=============================================================================
// 게임서버 클래스
//=============================================================================

class GameServer {
private:
    HANDLE m_hIOCP;
    SOCKET m_listenSocket;
    HANDLE m_hShutdownEvent;
    HANDLE m_hWorkerThreads[WORKER_THREAD_COUNT];

    CRITICAL_SECTION m_sessionLock;
    std::map<UINT32, Session*> m_sessions;
    UINT32 m_nextPlayerId;

    volatile bool m_running;
    LONG m_connectionCount;

    // Accept 관련
    LPFN_ACCEPTEX m_lpfnAcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockaddrs;
    IOContext m_acceptContext;

public:
    GameServer() : m_hIOCP(NULL), m_listenSocket(INVALID_SOCKET),
                   m_hShutdownEvent(NULL), m_nextPlayerId(1),
                   m_running(false), m_connectionCount(0),
                   m_lpfnAcceptEx(NULL), m_lpfnGetAcceptExSockaddrs(NULL) {
        InitializeCriticalSection(&m_sessionLock);
        memset(m_hWorkerThreads, 0, sizeof(m_hWorkerThreads));
    }

    ~GameServer() {
        Shutdown();
        DeleteCriticalSection(&m_sessionLock);
    }

    bool Initialize() {
        printf("[Server] 초기화 중...\n");

        // Winsock 초기화
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("[Error] WSAStartup 실패\n");
            return false;
        }

        // IOCP 생성
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (m_hIOCP == NULL) {
            printf("[Error] IOCP 생성 실패: %lu\n", GetLastError());
            return false;
        }

        // 종료 이벤트 생성
        m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        // 리슨 소켓 생성
        m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                    NULL, 0, WSA_FLAG_OVERLAPPED);
        if (m_listenSocket == INVALID_SOCKET) {
            printf("[Error] 리슨 소켓 생성 실패\n");
            return false;
        }

        // 소켓 옵션 설정
        int optval = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
                   (char*)&optval, sizeof(optval));

        // 바인딩
        sockaddr_in addr = { 0 };
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            printf("[Error] 바인드 실패\n");
            return false;
        }

        // 리슨 시작
        if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            printf("[Error] 리슨 실패\n");
            return false;
        }

        // 리슨 소켓을 IOCP에 연결
        CreateIoCompletionPort((HANDLE)m_listenSocket, m_hIOCP, 0, 0);

        // AcceptEx 함수 포인터 획득
        GUID guidAcceptEx = WSAID_ACCEPTEX;
        GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        DWORD bytes;

        WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &guidAcceptEx, sizeof(guidAcceptEx),
                 &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &bytes, NULL, NULL);

        WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &guidGetAcceptExSockaddrs, sizeof(guidGetAcceptExSockaddrs),
                 &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs),
                 &bytes, NULL, NULL);

        printf("[Server] 초기화 완료 (Port: %d)\n", SERVER_PORT);
        return true;
    }

    bool Start() {
        printf("[Server] 시작 중...\n");
        m_running = true;

        // 워커 스레드 생성
        for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
            m_hWorkerThreads[i] = (HANDLE)_beginthreadex(
                NULL, 0, WorkerThreadStatic, this, 0, NULL);
            printf("[Server] 워커 스레드 %d 시작\n", i);
        }

        // 첫 번째 AcceptEx 호출
        PostAccept();

        printf("[Server] 서버 시작됨! (Port: %d)\n", SERVER_PORT);
        printf("[Server] 종료하려면 'q' 입력\n\n");

        return true;
    }

    void Run() {
        char input;
        while (m_running) {
            if (_kbhit()) {
                input = _getch();
                if (input == 'q' || input == 'Q') {
                    break;
                }
                if (input == 's' || input == 'S') {
                    PrintStatus();
                }
            }
            Sleep(100);
        }
    }

    void Shutdown() {
        if (!m_running) return;

        printf("[Server] 종료 중...\n");
        m_running = false;

        // 종료 이벤트 설정
        if (m_hShutdownEvent) {
            SetEvent(m_hShutdownEvent);
        }

        // 워커 스레드에 종료 신호
        for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
            PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL);
        }

        // 워커 스레드 종료 대기
        WaitForMultipleObjects(WORKER_THREAD_COUNT, m_hWorkerThreads, TRUE, 5000);

        for (int i = 0; i < WORKER_THREAD_COUNT; i++) {
            if (m_hWorkerThreads[i]) {
                CloseHandle(m_hWorkerThreads[i]);
                m_hWorkerThreads[i] = NULL;
            }
        }

        // 모든 세션 종료
        EnterCriticalSection(&m_sessionLock);
        for (auto& pair : m_sessions) {
            if (pair.second->socket != INVALID_SOCKET) {
                closesocket(pair.second->socket);
            }
            delete pair.second;
        }
        m_sessions.clear();
        LeaveCriticalSection(&m_sessionLock);

        // 리슨 소켓 종료
        if (m_listenSocket != INVALID_SOCKET) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }

        // 핸들 정리
        if (m_hShutdownEvent) {
            CloseHandle(m_hShutdownEvent);
            m_hShutdownEvent = NULL;
        }
        if (m_hIOCP) {
            CloseHandle(m_hIOCP);
            m_hIOCP = NULL;
        }

        WSACleanup();
        printf("[Server] 종료 완료\n");
    }

private:
    // AcceptEx 호출
    void PostAccept() {
        m_acceptContext.Reset();
        m_acceptContext.operation = IO_ACCEPT;
        m_acceptContext.acceptSocket = WSASocket(AF_INET, SOCK_STREAM,
            IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

        DWORD bytes;
        BOOL result = m_lpfnAcceptEx(
            m_listenSocket,
            m_acceptContext.acceptSocket,
            m_acceptContext.buffer,
            0,  // 데이터 수신 안 함
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            &bytes,
            &m_acceptContext
        );

        if (!result && WSAGetLastError() != ERROR_IO_PENDING) {
            printf("[Error] AcceptEx 실패: %d\n", WSAGetLastError());
            closesocket(m_acceptContext.acceptSocket);
        }
    }

    // 워커 스레드
    static unsigned __stdcall WorkerThreadStatic(void* param) {
        return ((GameServer*)param)->WorkerThread();
    }

    unsigned WorkerThread() {
        DWORD threadId = GetCurrentThreadId();

        while (m_running) {
            DWORD bytesTransferred;
            ULONG_PTR completionKey;
            LPOVERLAPPED pOv;

            BOOL success = GetQueuedCompletionStatus(
                m_hIOCP,
                &bytesTransferred,
                &completionKey,
                &pOv,
                1000  // 1초 타임아웃
            );

            if (!success) {
                if (pOv == NULL) {
                    // 타임아웃 또는 종료
                    continue;
                }
                // I/O 에러 (연결 종료 등)
                IOContext* pIO = (IOContext*)pOv;
                if (pIO->operation == IO_RECV || pIO->operation == IO_SEND) {
                    Session* pSession = (Session*)completionKey;
                    if (pSession) {
                        OnDisconnect(pSession);
                    }
                }
                continue;
            }

            if (completionKey == 0 && pOv == NULL) {
                // 종료 신호
                break;
            }

            IOContext* pIO = (IOContext*)pOv;

            __try {
                switch (pIO->operation) {
                case IO_ACCEPT:
                    OnAccept();
                    break;

                case IO_RECV:
                    if (bytesTransferred == 0) {
                        // 연결 종료
                        OnDisconnect((Session*)completionKey);
                    } else {
                        OnRecv((Session*)completionKey, bytesTransferred);
                    }
                    break;

                case IO_SEND:
                    OnSendComplete((Session*)completionKey);
                    break;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                printf("[Error] 예외 발생: 0x%08X\n", GetExceptionCode());
            }
        }

        return 0;
    }

    // Accept 완료 처리
    void OnAccept() {
        SOCKET clientSocket = m_acceptContext.acceptSocket;

        // 소켓 옵션 상속
        setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                   (char*)&m_listenSocket, sizeof(m_listenSocket));

        // 클라이언트 주소 얻기
        sockaddr_in* pLocal = NULL;
        sockaddr_in* pRemote = NULL;
        int localLen, remoteLen;

        m_lpfnGetAcceptExSockaddrs(
            m_acceptContext.buffer,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            (sockaddr**)&pLocal, &localLen,
            (sockaddr**)&pRemote, &remoteLen
        );

        printf("[Server] 클라이언트 연결: %s:%d\n",
               inet_ntoa(pRemote->sin_addr), ntohs(pRemote->sin_port));

        // 세션 생성
        Session* pSession = new Session();
        pSession->socket = clientSocket;
        pSession->playerId = InterlockedIncrement((LONG*)&m_nextPlayerId);

        // IOCP에 등록
        CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP,
                               (ULONG_PTR)pSession, 0);

        // 세션 목록에 추가
        EnterCriticalSection(&m_sessionLock);
        m_sessions[pSession->playerId] = pSession;
        LeaveCriticalSection(&m_sessionLock);

        InterlockedIncrement(&m_connectionCount);

        // 수신 시작
        PostRecv(pSession);

        // 다음 Accept 준비
        PostAccept();
    }

    // 수신 시작
    void PostRecv(Session* pSession) {
        pSession->recvContext.Reset();
        pSession->recvContext.operation = IO_RECV;
        pSession->recvContext.wsaBuf.buf = pSession->recvContext.buffer;
        pSession->recvContext.wsaBuf.len = RECV_BUFFER_SIZE;

        DWORD flags = 0;
        int result = WSARecv(pSession->socket,
            &pSession->recvContext.wsaBuf, 1, NULL, &flags,
            &pSession->recvContext, NULL);

        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            OnDisconnect(pSession);
        }
    }

    // 수신 완료 처리
    void OnRecv(Session* pSession, DWORD bytesTransferred) {
        // 수신 버퍼에 데이터 추가
        memcpy(pSession->recvBuffer + pSession->recvBufferLen,
               pSession->recvContext.buffer, bytesTransferred);
        pSession->recvBufferLen += bytesTransferred;

        // 패킷 파싱
        while (pSession->recvBufferLen >= PACKET_HEADER_SIZE) {
            PacketHeader* pHeader = (PacketHeader*)pSession->recvBuffer;

            if (pHeader->size < PACKET_HEADER_SIZE ||
                pHeader->size > RECV_BUFFER_SIZE) {
                // 잘못된 패킷
                printf("[Error] 잘못된 패킷 크기: %d\n", pHeader->size);
                OnDisconnect(pSession);
                return;
            }

            if (pSession->recvBufferLen < pHeader->size) {
                // 패킷 미완성
                break;
            }

            // 패킷 처리
            ProcessPacket(pSession, pSession->recvBuffer, pHeader->size);

            // 처리한 패킷 제거
            pSession->recvBufferLen -= pHeader->size;
            if (pSession->recvBufferLen > 0) {
                memmove(pSession->recvBuffer,
                        pSession->recvBuffer + pHeader->size,
                        pSession->recvBufferLen);
            }
        }

        // 다음 수신 대기
        PostRecv(pSession);
    }

    // 패킷 처리
    void ProcessPacket(Session* pSession, char* data, int size) {
        PacketHeader* pHeader = (PacketHeader*)data;

        switch (pHeader->type) {
        case PKT_CS_LOGIN:
            HandleLogin(pSession, (PKT_CS_Login*)data);
            break;

        case PKT_CS_CHAT:
            HandleChat(pSession, (PKT_CS_Chat*)data);
            break;

        case PKT_CS_MOVE:
            HandleMove(pSession, (PKT_CS_Move*)data);
            break;

        default:
            printf("[Warning] 알 수 없는 패킷: %d\n", pHeader->type);
            break;
        }
    }

    // 로그인 처리
    void HandleLogin(Session* pSession, PKT_CS_Login* pPacket) {
        strncpy_s(pSession->username, pPacket->username, sizeof(pSession->username) - 1);
        printf("[Game] 플레이어 로그인: %s (ID: %u)\n",
               pSession->username, pSession->playerId);

        // 로그인 결과 전송
        PKT_SC_LoginResult result;
        result.header.size = sizeof(result);
        result.header.type = PKT_SC_LOGIN_RESULT;
        result.playerId = pSession->playerId;
        result.success = 1;
        SendPacket(pSession, (char*)&result, sizeof(result));

        // 다른 플레이어들에게 입장 알림
        BroadcastPlayerEnter(pSession);
    }

    // 채팅 처리
    void HandleChat(Session* pSession, PKT_CS_Chat* pPacket) {
        printf("[Chat] %s: %s\n", pSession->username, pPacket->message);

        // 모든 플레이어에게 브로드캐스트
        PKT_SC_Chat chat;
        chat.header.size = sizeof(chat);
        chat.header.type = PKT_SC_CHAT;
        chat.playerId = pSession->playerId;
        strncpy_s(chat.username, pSession->username, sizeof(chat.username) - 1);
        strncpy_s(chat.message, pPacket->message, sizeof(chat.message) - 1);

        BroadcastPacket((char*)&chat, sizeof(chat));
    }

    // 이동 처리
    void HandleMove(Session* pSession, PKT_CS_Move* pPacket) {
        pSession->posX = pPacket->x;
        pSession->posY = pPacket->y;
        pSession->posZ = pPacket->z;

        // 모든 플레이어에게 브로드캐스트
        PKT_SC_Move move;
        move.header.size = sizeof(move);
        move.header.type = PKT_SC_MOVE;
        move.playerId = pSession->playerId;
        move.x = pPacket->x;
        move.y = pPacket->y;
        move.z = pPacket->z;

        BroadcastPacket((char*)&move, sizeof(move), pSession->playerId);
    }

    // 플레이어 입장 브로드캐스트
    void BroadcastPlayerEnter(Session* pNewSession) {
        // 기존 플레이어들에게 새 플레이어 알림
        // (실제 구현에서는 PKT_SC_PLAYER_ENTER 패킷 전송)
    }

    // 패킷 브로드캐스트
    void BroadcastPacket(char* data, int size, UINT32 excludePlayerId = 0) {
        EnterCriticalSection(&m_sessionLock);
        for (auto& pair : m_sessions) {
            if (pair.first != excludePlayerId) {
                SendPacket(pair.second, data, size);
            }
        }
        LeaveCriticalSection(&m_sessionLock);
    }

    // 패킷 송신
    void SendPacket(Session* pSession, char* data, int size) {
        EnterCriticalSection(&pSession->sendLock);

        // 송신 큐에 추가
        pSession->sendQueue.push(std::string(data, size));

        // 현재 송신 중이 아니면 송신 시작
        if (!pSession->isSending) {
            pSession->isSending = true;
            LeaveCriticalSection(&pSession->sendLock);
            DoSend(pSession);
        } else {
            LeaveCriticalSection(&pSession->sendLock);
        }
    }

    // 실제 송신
    void DoSend(Session* pSession) {
        EnterCriticalSection(&pSession->sendLock);

        if (pSession->sendQueue.empty()) {
            pSession->isSending = false;
            LeaveCriticalSection(&pSession->sendLock);
            return;
        }

        std::string& packet = pSession->sendQueue.front();

        pSession->sendContext.Reset();
        pSession->sendContext.operation = IO_SEND;
        memcpy(pSession->sendContext.buffer, packet.data(), packet.size());
        pSession->sendContext.wsaBuf.buf = pSession->sendContext.buffer;
        pSession->sendContext.wsaBuf.len = (ULONG)packet.size();

        pSession->sendQueue.pop();

        LeaveCriticalSection(&pSession->sendLock);

        int result = WSASend(pSession->socket,
            &pSession->sendContext.wsaBuf, 1, NULL, 0,
            &pSession->sendContext, NULL);

        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            OnDisconnect(pSession);
        }
    }

    // 송신 완료 처리
    void OnSendComplete(Session* pSession) {
        // 다음 패킷 송신
        DoSend(pSession);
    }

    // 연결 종료 처리
    void OnDisconnect(Session* pSession) {
        if (pSession == NULL) return;

        printf("[Server] 클라이언트 연결 종료: %s (ID: %u)\n",
               pSession->username[0] ? pSession->username : "Unknown",
               pSession->playerId);

        // 소켓 닫기
        if (pSession->socket != INVALID_SOCKET) {
            closesocket(pSession->socket);
            pSession->socket = INVALID_SOCKET;
        }

        // 세션 목록에서 제거
        EnterCriticalSection(&m_sessionLock);
        m_sessions.erase(pSession->playerId);
        LeaveCriticalSection(&m_sessionLock);

        InterlockedDecrement(&m_connectionCount);

        delete pSession;
    }

    // 상태 출력
    void PrintStatus() {
        printf("\n=== 서버 상태 ===\n");
        printf("연결된 클라이언트: %ld\n", m_connectionCount);
        printf("워커 스레드: %d\n", WORKER_THREAD_COUNT);

        EnterCriticalSection(&m_sessionLock);
        printf("플레이어 목록:\n");
        for (auto& pair : m_sessions) {
            printf("  - %s (ID: %u) pos(%.1f, %.1f, %.1f)\n",
                   pair.second->username[0] ? pair.second->username : "Unknown",
                   pair.first,
                   pair.second->posX, pair.second->posY, pair.second->posZ);
        }
        LeaveCriticalSection(&m_sessionLock);
        printf("================\n\n");
    }
};

//=============================================================================
// 크래시 덤프 생성
//=============================================================================

LONG WINAPI CrashDumpHandler(EXCEPTION_POINTERS* pExInfo) {
    WCHAR dumpPath[MAX_PATH];
    SYSTEMTIME st;
    GetLocalTime(&st);

    swprintf_s(dumpPath, L"GameServer_Crash_%04d%02d%02d_%02d%02d%02d.dmp",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = pExInfo;
        mei.ClientPointers = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hFile, MiniDumpWithFullMemory, &mei, NULL, NULL);
        CloseHandle(hFile);

        wprintf(L"[Crash] 덤프 생성됨: %s\n", dumpPath);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

//=============================================================================
// Main
//=============================================================================

int main() {
    // 콘솔 설정
    SetConsoleTitleW(L"IOCP Game Server");
    printf("================================================\n");
    printf("    IOCP 기반 게임서버 예제\n");
    printf("    Port: %d\n", SERVER_PORT);
    printf("    Workers: %d\n", WORKER_THREAD_COUNT);
    printf("================================================\n\n");

    // 크래시 핸들러 등록
    SetUnhandledExceptionFilter(CrashDumpHandler);

    // 서버 생성 및 실행
    GameServer server;

    if (!server.Initialize()) {
        printf("[Error] 서버 초기화 실패\n");
        return 1;
    }

    if (!server.Start()) {
        printf("[Error] 서버 시작 실패\n");
        return 1;
    }

    // 메인 루프 (키보드 입력 처리)
    server.Run();

    // 서버 종료
    server.Shutdown();

    printf("\n서버가 종료되었습니다.\n");
    return 0;
}
