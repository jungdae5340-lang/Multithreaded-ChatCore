#define _CRT_SECURE_NO_WARNINGS
#include "ChatServer.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem> 
#include <fstream>
#include <ctime>
using namespace std;
namespace fs = std::filesystem;

// 메시지가 이미 UTF-8이므로 그대로 반환
static string ensure_utf8(const string& src) {
    return src;
}

void createlogdir() {
    // 로그 디렉토리 생성 (이미 존재하면 무시)
    fs::path log_dir = "logs";
    if (!fs::exists(log_dir)) {
        fs::create_directories(log_dir);
    }
}


int logfile(string msg) {
    // 현재 날짜와 시간 가져오기
    fs::path log_dir = "logs";
    auto t = time(nullptr);
    auto tm = *localtime(&t);
    char date_buffer[11]; // YYYY-MM-DD\0
    strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", &tm);
    string log_filename = log_dir.string() + "/log_" + string(date_buffer) + ".txt";

    // 새 파일이면 UTF-8을 추가해서 메모장 등에서 올바르게 보이게 함
    if (!fs::exists(log_filename)) {
        ofstream bomf(log_filename, ios::binary);
        if (bomf.is_open()) {
            const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            bomf.write((const char*)bom, sizeof(bom));
            bomf.close();
        }
    }

    ofstream log_file(log_filename, ios::app | ios::binary);
    if (!log_file.is_open()) {
        cerr << "Error opening log file: " << log_filename << endl;
        return -1;
    }
    // 현재 시간 형식화
    char time_buffer[9]; // HH:MM:SS\0
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &tm);

    string utf8msg = ensure_utf8(msg);
    log_file << "[" << time_buffer << "] ";
    log_file.write(utf8msg.c_str(), (streamsize)utf8msg.size());
    log_file << endl;
    return 0;
}