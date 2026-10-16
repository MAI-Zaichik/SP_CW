#include "storage/disk/disk_manager.h"
#include "storage/page/page.h"
#include <stdexcept>

// открывает файл на r/w, создаёт если не существует
DiskManager::DiskManager(const std::string& fname)
    : filename(fname)
{
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        file.clear();
        file.open(filename, std::ios::out | std::ios::binary);
        file.close();
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open database file");
    }
}

DiskManager::~DiskManager() {
    file.close();
}

// читает страницу по id, при неполном чтении заполняет остаток нулями
void DiskManager::read_page(uint32_t page_id, char* data) {
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

    file.seekg(offset, std::ios::beg);
    file.read(data, PAGE_SIZE);

    // частичное чтение возможно на последней странице при незавершённой записи
    if (file.gcount() < static_cast<std::streamsize>(PAGE_SIZE)) {
        std::fill(data, data + PAGE_SIZE, 0);
    }
}

// записывает страницу по id и сбрасывает буфер на диск
void DiskManager::write_page(uint32_t page_id, const char* data) {
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

    file.seekp(offset, std::ios::beg);
    file.write(data, PAGE_SIZE);
    file.flush();
}

// возвращает количество полных страниц в файле
uint32_t DiskManager::get_page_count() {
    file.seekg(0, std::ios::end);
    uint64_t size = file.tellg();

    return static_cast<uint32_t>(size / PAGE_SIZE);
}