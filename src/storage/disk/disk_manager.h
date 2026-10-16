#pragma once

#include <string>
#include <fstream>
#include <cstdint>

class DiskManager {
private:
    std::fstream file;
    std::string filename;

public:
    DiskManager(const std::string& filename);
    ~DiskManager();

    void read_page(uint32_t page_id, char* data);
    void write_page(uint32_t page_id, const char* data);

    // считает страницы по размеру файла
    uint32_t get_page_count();
};