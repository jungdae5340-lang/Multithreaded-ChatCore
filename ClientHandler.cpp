#include "ChatServer.h"
#include <sstream>

using namespace std;

// 클라이언트 핸들러
void handle_client_wrapper(ClientHandler* handler) {
    if (!g_server_instance) return;
    char buffer[1024];
    SOCKET sock = handler->get_socket();

    // 닉네임 받기
    int len = recv(sock, buffer, 1024, 0);
    if (len <= 0) { g_server_instance->remove_client(handler); return; }
    buffer[len] = '\0';
    string nick = buffer;
    // 줄바꿈 제거
    nick.erase(remove(nick.begin(), nick.end(), '\n'), nick.end());
    nick.erase(remove(nick.begin(), nick.end(), '\r'), nick.end());
    handler->setNickname(nick);

    cout << "[Connect] " << nick << " joined." << endl;
    logfile("[Connect] " + nick + " joined.");
    g_server_instance->broadcast_message(nick + " has joined the Lobby.");

    // 메시지 루프
    while (true) {
        len = recv(sock, buffer, 1024, 0);
        if (len <= 0) break;
        buffer[len] = '\0';
        string msg = buffer;
        logfile("[" + handler->getNickname() + "] " + msg);
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();

        if (msg.empty()) continue;

        if (msg[0] == '/') {
            stringstream ss(msg);
            string cmd; ss >> cmd;
            if (cmd == "/list") g_server_instance->send_user_list(handler);
            else if (cmd == "/rooms") g_server_instance->send_room_list(handler);
            else if (cmd == "/join") {
                string room; ss >> room;
                if (!room.empty()) g_server_instance->join_room(handler, room);
            }
            else if (cmd == "/whisper") {
                string target, content; ss >> target; getline(ss, content);
                if (!content.empty()) {
                    g_server_instance->send_private_message(target, content, handler);
                }
                else {
                    handler->send_message("Usage: /whisper <nickname> <message>");
                }
            }
            else if (cmd == "/notice") {
                string content;
                getline(ss, content); // 내용 받기
                // 앞에 공백 제거
                if (!content.empty() && content[0] == ' ') content.erase(0, 1);

                if (!content.empty()) {
                    // [NOTICE] 태그를 달아서 전체 방송
                    g_server_instance->broadcast_message("[NOTICE] " + content);
                }
                else {
                    handler->send_message("Usage: /notice <message>");
                }
            }
            else if (cmd == "/kick") {
                if (handler->getNickname() == "admin") {
                    string target;
                    ss >> target;
                    if (!target.empty()) {
                        cout << "[DEBUG] Admin kicking user: " << target << endl;
                        g_server_instance->kick_user(target, handler);
                    }
                    else {
                        handler->send_message("Usage: /kick <nickname>");
                    }
                }
                else {
                    cout << "[DEBUG] Kick attempt denied for: " << handler->getNickname() << endl;
                    handler->send_message("Error: You do not have permission to use this command.");
                }
            }

            else if (cmd == "/help") {
                handler->send_message(u8"Available commands:\n"
                    u8"/list - Show connected users(현재 서버에 들어와있는 유저를 확인할 수 있습니다!)\n"
                    u8"/whisper <nickname> <message> - Send private message(다른 유저에게 긴밀하게 연락을 보낼 수 있습니다!)\n"
                    u8"/rooms - List available chat rooms(어떤 방들이 있는지 확인할 수 있습니다!)\n"
                    u8"/join <room_name> - Join a chat room(새로운 방을 개설 및 참여할 수 있습니다!)\n"
                    u8"/notice <message> - Send server notice (admin only)(서버가 유저에게 공지합니다!)\n"
                    u8"/kick <nickname> - Kick user (admin only)(서버는 유저를 강퇴시킬 수 있습니다!)\n"
                    u8"/help - Show this help message(또 필요하시면 이 명령어를 입력해주세요!");
                //logfile(handler->getNickname() + " requested help.");
            }
            else {
                handler->send_message("Unknown command.");
            }
        }
        else {
            string full_msg = "[" + handler->getNickname() + "] " + msg;
            cout << full_msg << endl;
            g_server_instance->broadcast_message(full_msg, handler);
        }
    }
    g_server_instance->remove_client(handler);
}