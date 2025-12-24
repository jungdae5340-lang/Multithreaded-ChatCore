#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include <algorithm>
#include <map> 

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

class ClientHandler;
class ChatServer;

extern ChatServer* g_server_instance;
void handle_client_wrapper(ClientHandler* handler);

class ClientHandler {
private:
    SOCKET client_socket;
    std::string client_ip;
    std::string nickname;
    std::string current_room;

public:
    ClientHandler(SOCKET sock, const std::string& ip)
        : client_socket(sock), client_ip(ip), current_room("Lobby") {
    }

    SOCKET get_socket() const { return client_socket; }
    std::string get_ip() const { return client_ip; }
    std::string getNickname() const { return nickname; }
    void setNickname(const std::string& name) { nickname = name; }
    std::string getRoom() const { return current_room; }
    void setRoom(const std::string& room_name) { current_room = room_name; }

    void send_message(const std::string& message) {
        std::string msg_with_newline = message + "\n";
        send(client_socket, msg_with_newline.c_str(), (int)msg_with_newline.length(), 0);
    }

    ~ClientHandler() {
        if (client_socket != INVALID_SOCKET) closesocket(client_socket);
    }
};

class ChatServer {
private:
    SOCKET listen_socket;
    int port;
    std::vector<ClientHandler*> client_list;
    std::map<std::string, std::vector<ClientHandler*>> rooms;
    std::mutex client_mutex;
    bool is_running;

public:
    ChatServer(int p);
    ~ChatServer();
    bool init_winsock();
    bool create_listen_socket();
    void start_listen();
    void process_admin_console(); // 관리자 콘솔
    void broadcast_message(const std::string& message, ClientHandler* sender = nullptr);
    void send_private_message(const std::string& target_name, const std::string& message, ClientHandler* sender);
    void send_user_list(ClientHandler* requester);
    void send_server_notice(const std::string& message);
    void kick_user(const std::string& target_name, ClientHandler* sender = nullptr);
    void remove_client(ClientHandler* handler);
    void join_room(ClientHandler* client, std::string room_name);
    void send_room_list(ClientHandler* requester);
};
void createlogdir();
int logfile(std::string msg);
#endif

