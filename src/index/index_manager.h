#ifndef CW_INDEX_MANAGER_H
#define CW_INDEX_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>
#include "core/types.h"
#include "index/btree.h"

class IndexManager {
public:
    explicit IndexManager(std::string indexDir = "")
        : _indexDir(std::move(indexDir)) {}

    // cоздаём индекс для колонки
    void create(const std::string& db, const std::string& table,
                const std::string& column);

    // удаляем
    void drop(const std::string& db, const std::string& table,
              const std::string& column);

    // вставляем ключ
    void insert(const std::string& db, const std::string& table,
                const std::string& column, const Value& key, RowId rid);

    // Удаляем
    bool erase(const std::string& db, const std::string& table,
               const std::string& column, const Value& key);

    // ищем rowid
    std::optional<RowId> find(const std::string& db, const std::string& table,
                              const std::string& column, const Value& key);

    // ищем в диапазоне
    std::vector<RowId> scanRange(const std::string& db, const std::string& table, const std::string& column, 
                                const Value& lo, const Value& hi);

    void flushAll();

    void dropDatabase(const std::string& db);

private:
    struct Entry {
        std::unique_ptr<BTree> tree;
    };

    std::unordered_map<std::string, Entry> _indexes;
    std::string _indexDir;

    // ключ в мапе: "db.table.column"
    static std::string makeKey(const std::string& db,
                               const std::string& table,
                               const std::string& column);

    // путь к файлу: <_indexDir>/db.table.column.idx
    std::string makePath(const std::string& key) const;

    // ищет Entry по ключу. Возвращает nullptr, если нет.
    Entry* findEntry(const std::string& db,
                     const std::string& table,
                     const std::string& column);
};

#endif // CW_INDEX_MANAGER_H