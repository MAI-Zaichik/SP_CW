#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <cstdint>

class Logger {
public:
    Logger();
    void log(const std::string& query,
             std::chrono::system_clock::time_point start,
             std::chrono::system_clock::time_point end,
             int64_t client_id,
             int64_t handler_id,
             bool ok,
             const std::string& status);
private:
    std::ofstream _file;
    static std::string formatTime(std::chrono::system_clock::time_point tp);
};