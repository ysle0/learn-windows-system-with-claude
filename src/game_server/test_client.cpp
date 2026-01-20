/**
 * IOCP 게임서버 테스트 클라이언트
 *
 * 게임서버에 접속하여 로그인, 채팅, 이동 테스트를 수행합니다.
 *
 * 컴파일: cl /EHsc /W4 test_client.cpp ws2_32.lib
 */

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

//=============================================================================
// 패킷 정의 (서버와 동일)
//=============================================================================

#pragma pack(push, 1)

struct PacketHeader {
    USHORT size;
    USHORT type;
};

enum PacketType {
    PKT_CS_LOGIN = 1,
    PKT_SC_LOGIN_RESULT = 2,
    PKT_CS_CHAT = 3,
    PKT_SC_CHAT = 4,
    PKT_CS_MOVE = 5,
    PKT_SC_MOVE = 6,
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
// 전역 변수
//=============================================================================

SOCKET g_socket = INVALID_SOCKET;
UINT32 g_myPlayerId = 0;
char g_myUsername[32] = { 0 };
float g_posX = 0, g_posY = 0, g_posZ = 0;
volatile bool g_running = true;
HANDLE g_hRecvThread = NULL;

//=============================================================================
// 패킷 송신
//=============================================================================

void SendPacket(void* data, int size) {
    int sent = send(g_socket, (char*)data, size, 0);
    if (sent == SOCKET_ERROR) {
        printf("[Error] 송신 실패: %d\n", WSAGetLastError());
    }
}

void SendLogin(const char* username) {
    PKT_CS_Login packet;
    packet.header.size = sizeof(packet);
    packet.header.type = PKT_CS_LOGIN;
    strncpy_s(packet.username, username, sizeof(packet.username) - 1);
    SendPacket(&packet, sizeof(packet));
}

void SendChat(const char* message) {
    PKT_CS_Chat packet;
    packet.header.size = sizeof(packet);
    packet.header.type = PKT_CS_CHAT;
    strncpy_s(packet.message, message, sizeof(packet.message) - 1);
    SendPacket(&packet, sizeof(packet));
}

void SendMove(float x, float y, float z) {
    PKT_CS_Move packet;
    packet.header.size = sizeof(packet);
    packet.header.type = PKT_CS_MOVE;
    packet.x = x;
    packet.y = y;
    packet.z = z;
    SendPacket(&packet, sizeof(packet));
}

//=============================================================================
// 패킷 처리
//=============================================================================

void HandleLoginResult(PKT_SC_LoginResult* packet) {
    if (packet->success) {
        g_myPlayerId = packet->playerId;
        printf("[System] 로그인 성공! Player ID: %u\n", g_myPlayerId);
    } else {
        printf("[System] 로그인 실패!\n");
    }
}

void HandleChat(PKT_SC_Chat* packet) {
    printf("[%s] %s\n", packet->username, packet->message);
}

void HandleMove(PKT_SC_Move* packet) {
    printf("[Move] Player %u: (%.1f, %.1f, %.1f)\n",
           packet->playerId, packet->x, packet->y, packet->z);
}

void ProcessPacket(char* data, int size) {
    PacketHeader* header = (PacketHeader*)data;

    switch (header->type) {
    case PKT_SC_LOGIN_RESULT:
        HandleLoginResult((PKT_SC_LoginResult*)data);
        break;
    case PKT_SC_CHAT:
        HandleChat((PKT_SC_Chat*)data);
        break;
    case PKT_SC_MOVE:
        HandleMove((PKT_SC_Move*)data);
        break;
    default:
        printf("[Warning] 알 수 없는 패킷: %d\n", header->type);
        break;
    }
}

//=============================================================================
// 수신 스레드
//=============================================================================

DWORD WINAPI RecvThread(LPVOID param) {
    char buffer[4096];
    char recvBuffer[8192];
    int recvBufferLen = 0;

    while (g_running) {
        int received = recv(g_socket, buffer, sizeof(buffer), 0);

        if (received <= 0) {
            if (g_running) {
                printf("[System] 서버 연결 끊김\n");
            }
            g_running = false;
            break;
        }

        // 수신 버퍼에 추가
        memcpy(recvBuffer + recvBufferLen, buffer, received);
        recvBufferLen += received;

        // 패킷 파싱
        while (recvBufferLen >= sizeof(PacketHeader)) {
            PacketHeader* header = (PacketHeader*)recvBuffer;

            if (recvBufferLen < header->size) {
                break;  // 패킷 미완성
            }

            ProcessPacket(recvBuffer, header->size);

            // 처리한 패킷 제거
            recvBufferLen -= header->size;
            if (recvBufferLen > 0) {
                memmove(recvBuffer, recvBuffer + header->size, recvBufferLen);
            }
        }
    }

    return 0;
}

//=============================================================================
// 메인
//=============================================================================

int main(int argc, char* argv[]) {
    const char* serverIp = "127.0.0.1";
    int serverPort = 9000;

    printf("================================================\n");
    printf("    IOCP 게임서버 테스트 클라이언트\n");
    printf("================================================\n\n");

    // Winsock 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[Error] WSAStartup 실패\n");
        return 1;
    }

    // 소켓 생성
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        printf("[Error] 소켓 생성 실패\n");
        WSACleanup();
        return 1;
    }

    // 서버 연결
    printf("[System] 서버에 연결 중... (%s:%d)\n", serverIp, serverPort);

    sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverPort);
    addr.sin_addr.s_addr = inet_addr(serverIp);

    if (connect(g_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[Error] 서버 연결 실패: %d\n", WSAGetLastError());
        closesocket(g_socket);
        WSACleanup();
        return 1;
    }

    printf("[System] 서버에 연결됨!\n\n");

    // 수신 스레드 시작
    g_hRecvThread = CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);

    // 사용자 이름 입력
    printf("사용자 이름 입력: ");
    fgets(g_myUsername, sizeof(g_myUsername), stdin);
    g_myUsername[strcspn(g_myUsername, "\n")] = 0;

    // 로그인
    SendLogin(g_myUsername);

    printf("\n명령어:\n");
    printf("  /chat <메시지> - 채팅\n");
    printf("  /move <x> <y> <z> - 이동\n");
    printf("  /quit - 종료\n\n");

    // 입력 처리
    char input[512];
    while (g_running) {
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) continue;

        if (strncmp(input, "/chat ", 6) == 0) {
            SendChat(input + 6);
        }
        else if (strncmp(input, "/move ", 6) == 0) {
            float x, y, z;
            if (sscanf_s(input + 6, "%f %f %f", &x, &y, &z) == 3) {
                SendMove(x, y, z);
                g_posX = x; g_posY = y; g_posZ = z;
            } else {
                printf("[Error] 사용법: /move <x> <y> <z>\n");
            }
        }
        else if (strcmp(input, "/quit") == 0) {
            break;
        }
        else {
            // 기본: 채팅으로 처리
            SendChat(input);
        }
    }

    // 정리
    g_running = false;
    closesocket(g_socket);

    if (g_hRecvThread) {
        WaitForSingleObject(g_hRecvThread, 1000);
        CloseHandle(g_hRecvThread);
    }

    WSACleanup();

    printf("\n클라이언트 종료\n");
    return 0;
}
