#include "ChatServer.h"
#include <thread>
#include <iostream>
#include <windows.h>

using namespace std;

int main() {
    createlogdir();
    logfile("Server started.");
    system("chcp 65001 > nul"); // 콘솔을 UTF-8로 변경
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    const int PORT = 9999;

    ChatServer server(PORT);

    if (!server.init_winsock() || !server.create_listen_socket()) {
        cout << "서버 초기화 실패. 엔터를 누르면 종료." << endl;
        cin.get();
        return 1;
    }

    // 리스닝 스레드 시작
    thread listen_thread(&ChatServer::start_listen, &server);

    // 관리자 콘솔 실행 (여기서 프로그램 종료 방지)
    server.process_admin_console();

    if (listen_thread.joinable()) listen_thread.join();
    return 0;
}