#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h> // 콘솔 설정 및 색상 제어

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

const char* PORT = "9999";
const char* SERVER_IP = "127.0.0.1";

// --- [UI] ANSI 색상 코드 정의 ---
#define RESET   "\033[0m"
#define RED     "\033[31m"      // 시스템/오류
#define GREEN   "\033[32m"      // 나 (My Chat)
#define YELLOW  "\033[33m"      // 공지/명령어
#define BLUE    "\033[34m"      // 정보/로고
#define MAGENTA "\033[35m"      // 귓속말
#define CYAN    "\033[36m"      // 상대방
#define BOLD    "\033[1m"       // 굵게
#define DIM     "\033[2m"       // 흐리게

// [UI] 로고 출력 함수
void print_logo() {
    // 커서 초기화
    system("cls");

    cout << BOLD CYAN;
    cout << "===================================================" << endl;
    cout << R"(
  ____ _           _     ____                  
 / ___| |__   __ _| |_  / ___|___  _ __ ___    
| |   | '_ \ / _` | __|| |   / _ \| '__/ _ \   
| |___| | | | (_| | |_ | |__| (_) | | |  __/   
 \____|_| |_|\__,_|\__| \____\___/|_|  \___|   
                                               
      [ TCP/IP Multi-Thread Chat Core ]        
)" << endl;
    cout << "===================================================" << endl;
    cout << RESET << endl;
}

// [UI] 콘솔에서 ANSI 색상 코드 활성화
void enable_virtual_terminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    system("chcp 65001 > nul"); // 한글 깨짐 방지 (UTF-8)
}

// 메시지 수신 스레드
void receive_msg(SOCKET sock) {
    char buf[1024];
    while (true) {

        int len = recv(sock, buf, 1024, 0);
        if (len > 0) {
            cout << "\r" << string(60, ' ') << "\r"; // 출력 라인 정리 (입력 중인 줄 덮어쓰기 방지)
            buf[len] = '\0';
            string msg = buf;
            // 메시지 종류별 색상 분기
            if (msg.find("[NOTICE]") != string::npos) {
                cout << BOLD YELLOW << "📢 " << msg << RESET << endl;
            }
            else if (msg.find("[Whisper]") != string::npos) {
                cout << MAGENTA << "🔒 " << msg << RESET << endl;
            }
            else if (msg.find("joined") != string::npos || msg.find("left") != string::npos) {
                cout << DIM << BLUE << "ℹ️ " << msg << RESET << endl;
            }
            else if (msg.find("Server:") != string::npos || msg.find("Kicked") != string::npos) {
                cout << RED << "⚠️ " << msg << RESET << endl;
                exit(0);
            }
            else {
                // 일반 채팅
                cout << CYAN << msg << RESET << endl;
            }
        }
        // 입력 프롬프트 복구 (초록색)
        cout << GREEN << "Me > " << RESET << flush;
    }
}
int main() {
    // 윈도우 콘솔에서 ANSI 코드 활성화
    system("chcp 65001"); // UTF-8 설정
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // [UI] 로고 출력
    print_logo();

    struct addrinfo hints = { 0 }, * result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    getaddrinfo(SERVER_IP, PORT, &hints, &result);
    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        cout << RED << "서버 연결 실패!" << RESET << endl;
        system("pause");
        return 1;
    }

    cout << BLUE << "서버 연결 성공! 닉네임을 입력하세요: " << RESET;

    // 닉네임 입력 (초록색)
    string nick;
    cout << GREEN;
    getline(cin, nick);
    cout << RESET;
    send(sock, nick.c_str(), (int)nick.length(), 0);

    // 안내 메시지
    cout << "---------------------------------------------------" << endl;
    cout << " " << YELLOW << "/help 를 사용하여 명령어 사용법을 확인할 수 있습니다!" << RESET << endl;
    cout << "---------------------------------------------------" << endl;

    thread(receive_msg, sock).detach();

    string line;
    while (true) {
        // 내 입력 프롬프트 (초록색)
        cout << GREEN << "Me > " << RESET << flush;

        getline(cin, line);

        if (line == "exit") break;
        send(sock, line.c_str(), (int)line.length(), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}