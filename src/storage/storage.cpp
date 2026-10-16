#include "storage/storage.h"
#include "storage/pager/pager.h"
#include "storage/page/page.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

Storage::~Storage() = default;

// возвращает путь к файлу таблицы
std::string Storage::table_file(const std::string& db, const std::string& table) const {
    return root + db + "/" + table + ".db";
}

// возвращает pager для таблицы, создаёт если не существует
Pager& Storage::getPager(const std::string& db, const std::string& table) {
    std::string path = table_file(db, table);
    if (!pagers.count(path)) {
        pagers[path] = std::make_unique<Pager>(path);
    }
    return *pagers[path];
}

// создаёт директорию базы данных
void Storage::createDatabase(const std::string& name) {
    fs::create_directory(root + name);
}

// удаляет директорию базы данных со всем содержимым
void Storage::dropDatabase(const std::string& name) {
    // pager держит файлы открытыми - закрываем все по этой бд до удаления
    std::string prefix = root + name + "/";
    for (auto it = pagers.begin(); it != pagers.end();) {
        if (it->first.rfind(prefix, 0) == 0)
            it = pagers.erase(it);
        else
            ++it;
    }
    fs::remove_all(root + name);
}

// создаёт пустой файл таблицы
void Storage::createTable(const std::string& db, const std::string& table) {
    std::string path = table_file(db, table);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create table file");
    }
}

// удаляет файл таблицы
void Storage::dropTable(const std::string& db, const std::string& table) {
    std::string path = table_file(db, table);
    // pager держит файл открытым - закрываем дескриптор до удаления
    pagers.erase(path);
    fs::remove(path);
}

// записывает строку на последнюю страницу, при нехватке места выделяет новую
RowId Storage::write(const std::string& db, const std::string& table, const std::vector<uint8_t>& bytes) {
    Pager& pager = getPager(db, table);

    uint32_t page_count = pager.get_page_count();
    Page* page = nullptr;
    uint32_t target_page;

    if (page_count == 0) {
        page = pager.new_page();
        target_page = page->page_id;
    } else {
        // пробуем последнюю страницу перед тем как выделять новую
        target_page = page_count - 1;
        page = pager.fetch_page(target_page);
    }

    uint16_t slot_id;

    if (!page->add_row(bytes.data(), static_cast<uint16_t>(bytes.size()), slot_id)) {
        // текущая страница заполнена - выделяем новую
        page = pager.new_page();
        target_page = page->page_id;
        if (!page->add_row(bytes.data(), static_cast<uint16_t>(bytes.size()), slot_id)) {
            // строка не влезает даже в пустую страницу
            throw std::runtime_error("Row too large");
        }
    }

    pager.flush_page(target_page);
    return RowId{target_page, slot_id};
}

// возвращает все живые строки таблицы с их RowId
std::vector<std::pair<RowId, std::vector<uint8_t>>>
Storage::scan(const std::string& db, const std::string& table) {
    Pager& pager = getPager(db, table);

    std::vector<std::pair<RowId, std::vector<uint8_t>>> result;

    uint32_t page_count = pager.get_page_count();

    for (uint32_t p = 0; p < page_count; ++p) {
        Page* page = pager.fetch_page(p);
        uint16_t row_count = page->get_row_count();

        for (uint16_t s = 0; s < row_count; ++s) {
            uint16_t size;
            const uint8_t* data = page->get_row(s, size);
            // get_row возвращает nullptr для удалённых и невалидных слотов
            if (!data) continue;

            std::vector<uint8_t> row(data, data + size);
            result.push_back({RowId{p, s}, row});
        }
    }

    return result;
}

// читает одну строку по RowId (пусто если слот удалён или вне диапазона)
std::vector<uint8_t> Storage::read(const std::string& db, const std::string& table, const RowId& rid) {
    Pager& pager = getPager(db, table);
    if (rid.page_id >= pager.get_page_count()) return {};

    Page* page = pager.fetch_page(rid.page_id);
    uint16_t size;
    const uint8_t* data = page->get_row(rid.slot_id, size);
    if (!data) return {};
    return std::vector<uint8_t>(data, data + size);
}

// помечает старый слот удалённым и записывает новые данные, возвращает новый RowId
RowId Storage::update(const std::string& db, const std::string& table, const RowId& rid, const std::vector<uint8_t>& bytes) {
    remove(db, table, rid);
    return write(db, table, bytes);
}

// помечает слот как удалённый
void Storage::remove(const std::string& db, const std::string& table, const RowId& rid) {
    Pager& pager = getPager(db, table);
    Page* page = pager.fetch_page(rid.page_id);

    if (rid.slot_id >= page->get_row_count()) return;

    page->set_slot(rid.slot_id, Page::DELETED_SLOT);
    page->is_dirty = true;
    pager.flush_page(rid.page_id);
}