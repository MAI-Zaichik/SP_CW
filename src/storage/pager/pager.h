#pragma once

#include <unordered_map>
#include <cstdint>
#include "storage/page/page.h"
#include "storage/disk/disk_manager.h"

// кеш страниц в памяти поверх DiskManager
class Pager {
private:
    // страницы индексируются по page_id, живут до flush или деструктора
    std::unordered_map<uint32_t, Page*> pages;
    DiskManager disk;
    uint32_t next_page_id = 0;

public:
    Pager(const std::string& filename);

    // возвращает страницу из кеша или загружает с диска
    Page* fetch_page(uint32_t page_id);

    // выделяет новую страницу, сразу помечает dirty
    Page* new_page();

    // сбрасывает страницу на диск если dirty
    void flush_page(uint32_t page_id);

    uint32_t get_page_count();

    // сбрасывает все dirty страницы
    void flush_all();

    // flush_all + освобождение памяти всех страниц
    ~Pager();
};