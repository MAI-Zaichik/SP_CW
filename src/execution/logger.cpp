#include "execution/logger.h"
#include <ctime>
#include <iomanip>
#include <sstream>

Logger::Logger() : _file("access.log", std::ios::app) {}

std::string Logger::formatTime(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::log(const std::string& query, std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end, 
    int64_t client_id, int64_t handler_id, bool ok, const std::string& status) {
        _file << formatTime(start) << " | " << formatTime(end) << " | client=" << client_id << " | handler=" << handler_id
        << " | " << (ok ? "OK" : "ERR") << " | " << query << " | " << status << '\n';
        _file.flush();
}