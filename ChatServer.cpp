#include "ChatServer.h"
#include <sstream>

using namespace std;

ChatServer* g_server_instance = nullptr;

ChatServer::ChatServer(int p) : port(p), listen_socket(INVALID_SOCKET), is_running(true) {
    g_server_instance = this;
}

ChatServer::~ChatServer() {
    is_running = false;
    if (listen_socket != INVALID_SOCKET) closesocket(listen_socket);
    for (auto* h : client_list) delete h;
    client_list.clear();
    rooms.clear();
    WSACleanup();
}

bool ChatServer::init_winsock() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

bool ChatServer::create_listen_socket() {
    struct addrinfo* result = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, to_string(port).c_str(), &hints, &result) != 0) return false;
    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) { freeaddrinfo(result); return false; }

    if (bind(listen_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result); closesocket(listen_socket); return false;
    }
    freeaddrinfo(result);

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_socket); return false;
    }
    cout << "Server listening on port " << port << "..." << endl;
    return true;
}

void ChatServer::start_listen() {
    while (is_running) {
        SOCKET client_sock = accept(listen_socket, NULL, NULL);
        if (!is_running) break;
        if (client_sock == INVALID_SOCKET) continue;

        ClientHandler* new_client = new ClientHandler(client_sock, "127.0.0.1");
        {
            lock_guard<mutex> lock(client_mutex);
            client_list.push_back(new_client);
            rooms["Lobby"].push_back(new_client);
        }
        thread(handle_client_wrapper, new_client).detach();
    }
}

// 절대 꺼지지 않는 콘솔 입력 처리
void ChatServer::process_admin_console() {
    system("cls");

    cout << "=== Server Started (Port: " << port << ") ===" << endl;
    cout << "Commands: /notice, /list, /kick <name>, /whisper <name>, /exit" << endl;

    // 입력 버퍼 꼬임 방지를 위해 cin은 최소화하고 단순 대기
    string line;
    while (is_running) {
        if (!getline(cin, line)) {
            cin.clear(); // 입력 에러 시 복구
            continue;
        }
        if (line == "/exit") {
            is_running = false;
            closesocket(listen_socket);
            break;
        }
        if (line.rfind("/notice", 0) == 0) {
            string msg = line.substr(7);
            if (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
            if (!msg.empty()) {
                // 서버 이름으로 공지 전송
                broadcast_message("[NOTICE] [SERVER] " + msg);
                cout << "Notice sent: " + msg << endl;
            }
        }
        else if (line == "/list") {
            lock_guard<mutex> lock(client_mutex);
            cout << "--- Connected Users (" << client_list.size() << ") ---" << endl;
            for (auto* h : client_list) cout << h->getNickname() << " [" << h->getRoom() << "]" << endl;
        }
        else if (line.rfind("/kick", 0) == 0) {
            string target = line.substr(6);
            if (!target.empty() && target[0] == ' ') target.erase(0, 1); // 앞 공백 제거
            if (!target.empty()) {
                broadcast_message("Server:" + target + " kick!"); // 서버가 유저 강퇴
            }
        }
        else if (line.rfind("/whisper", 0) == 0) {
            stringstream ss(line);
            string cmd, target, msg;
            ss >> cmd >> target; // "/whisper"와 "닉네임" 분리
            getline(ss, msg); // 나머지 전체 메시지로 받음
            if (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
            if (!target.empty() && !msg.empty()) {
                send_private_message(target, msg, nullptr);
                cout << "[Server -> " << target << "] Whisper sent: " << msg << endl;
            }
            else {
                cout << "Usage: /whisper <nickname> <message>" << endl;
            }
        }
    }
}

void ChatServer::join_room(ClientHandler* client, string room_name) {
    lock_guard<mutex> lock(client_mutex);
    string old_room = client->getRoom();

    // 이전 방에서 제거
    auto& old_users = rooms[old_room];
    old_users.erase(remove(old_users.begin(), old_users.end(), client), old_users.end());
    if (old_users.empty() && old_room != "Lobby") rooms.erase(old_room);

    // 새 방 추가
    rooms[room_name].push_back(client);
    client->setRoom(room_name);

    client->send_message("Moved to room: " + room_name);
    cout << "[Room] " << client->getNickname() << " moved to " << room_name << endl;
}

void ChatServer::broadcast_message(const string& message, ClientHandler* sender) {
    lock_guard<mutex> lock(client_mutex);
    if (sender) {
        string room = sender->getRoom();
        if (rooms.find(room) != rooms.end()) {
            for (auto* h : rooms[room]) h->send_message(message);
        }
    }
    else {
        // 서버 공지는 전체 전송
        for (auto* h : client_list) h->send_message(message);
    }
}

void ChatServer::remove_client(ClientHandler* handler) {
    lock_guard<mutex> lock(client_mutex);
    string room = handler->getRoom();
    if (rooms.find(room) != rooms.end()) {
        auto& v = rooms[room];
        v.erase(remove(v.begin(), v.end(), handler), v.end());
    }
    client_list.erase(remove(client_list.begin(), client_list.end(), handler), client_list.end());
    cout << "[Disconnect] Client removed." << endl;
    delete handler;
}

void ChatServer::send_user_list(ClientHandler* req) {
    lock_guard<mutex> lock(client_mutex);
    string msg = "Users:\n";
    for (auto* h : client_list) msg += "- " + h->getNickname() + " [" + h->getRoom() + "]\n";
    req->send_message(msg);
}

void ChatServer::send_server_notice(const string& message) {
    lock_guard<mutex> lock(client_mutex);
    string notice_msg = "[NOTICE] " + message;
    for (ClientHandler* handler : client_list) {
        handler->send_message(notice_msg);
    }
}

void ChatServer::send_room_list(ClientHandler* req) {
    lock_guard<mutex> lock(client_mutex);
    string msg = "Rooms:\n";
    for (auto const& [name, list] : rooms) msg += "- " + name + " (" + to_string(list.size()) + ")\n";
    req->send_message(msg);
}
void ChatServer::send_private_message(const string& target, const string& msg, ClientHandler* sender) {
    lock_guard<mutex> lock(client_mutex);
    // 받는 사람 찾기
    for (auto* h : client_list) {
        if (h->getNickname() == target) {
            string sender_name = (sender) ? sender->getNickname() : "[Server]";
            h->send_message("[Whisper] " + sender_name + ": " + msg); // 받는 사람에게 전송
            if (sender) {
                sender->send_message("[Whisper to " + target + "] " + msg);
            }
            return;
        }
    }
    if (sender) {
        sender->send_message("User not found.");
    }
    else {
        cout << "User '" << target << "' not found." << endl;
    }
}
void ChatServer::kick_user(const string& target, ClientHandler* sender) {
    ClientHandler* target_handler = nullptr;

    {
        lock_guard<mutex> lock(client_mutex);
        for (ClientHandler* handler : client_list) {
            if (handler->getNickname() == target) {
                target_handler = handler;
                break;
            }
        }
    }

    if (target_handler) {
        string kick_msg = "You have been kicked from the server.\n";
        // 1. 전송할 데이터 버퍼 (const char*)
        const char* buffer = kick_msg.c_str();
        // 2. 전송할 데이터 길이 (null 문자 포함하지 않은 길이)
        int length = kick_msg.length();

        send(target_handler->get_socket(), buffer, length, 0);
        // [Linux] MSG_NOSIGNAL
        //send(target_handler->getSocket(), kick_msg.c_str(), (int)kick_msg.length(), MSG_NOSIGNAL);
        closesocket(target_handler->get_socket()); // [Linux] close

        string log_msg = "Server: User '" + target + "' has been kicked.";
        if (sender) {
            sender->send_message(log_msg);
            cout << "[Server] " << target << " kicked by " << sender->getNickname() << endl;
        }
        else {
            cout << "[Server Console] Kicked user: " << target << endl;
        }
    }
    else {
        string err_msg = "Server: User '" + target + "' not found.";
        if (sender) {
            sender->send_message(err_msg);
        }
        else {
            cout << "User '" << target << "' not found." << endl;
        }
    }
}
/*lock_guard<mutex> lock(client_mutex);
for (auto* h : client_list) {
    if (h->getNickname() == target) {
        h->send_message("You have been kicked.");
        closesocket(h->get_socket()); // 강제 종료
        cout << "Kicked " << target << endl;
        return;
    }
}
}*/
