#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

#include "core/types.h"

class Pager;

class Storage {
private:
    std::string root = "./data/";

    // ключ — путь к файлу таблицы
    std::unordered_map<std::string, std::unique_ptr<Pager>> pagers;

    std::string table_file(const std::string& db, const std::string& table) const;
    Pager& getPager(const std::string& db, const std::string& table);

public:
    void createDatabase(const std::string& name);
    void dropDatabase(const std::string& name);

    void createTable(const std::string& db, const std::string& table);
    void dropTable(const std::string& db, const std::string& table);

    RowId write(const std::string& db, const std::string& table, const std::vector<uint8_t>& bytes);

    std::vector<std::pair<RowId, std::vector<uint8_t>>>
    scan(const std::string& db, const std::string& table);

    // чтение одной строки по RowId, {} если слот удалён или невалиден
    std::vector<uint8_t> read(const std::string& db, const std::string& table, const RowId& rid);

    // возвращает новый RowId — после update старый невалиден
    RowId update(const std::string& db, const std::string& table, const RowId& rid, const std::vector<uint8_t>& bytes);

    void remove(const std::string& db, const std::string& table, const RowId& rid);

    ~Storage();
};