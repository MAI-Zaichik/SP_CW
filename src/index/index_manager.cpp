#include "index/index_manager.h"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

std::string IndexManager::makeKey(const std::string& db,
                                  const std::string& table,
                                  const std::string& column) {
    return db + "." + table + "." + column;
}

std::string IndexManager::makePath(const std::string& key) const {
    if (_indexDir.empty()) {
        return key + ".idx";
    }
    return _indexDir + "/" + key + ".idx";
}

IndexManager::Entry* IndexManager::findEntry(const std::string& db,
                                             const std::string& table,
                                             const std::string& column) {
    auto it = _indexes.find(makeKey(db, table, column));
    if (it == _indexes.end()) return nullptr;
    return &it->second;
}

void IndexManager::create(const std::string& db,
                          const std::string& table,
                          const std::string& column) {
    std::string key = makeKey(db, table, column);

    if (_indexes.count(key)) return;

    if (!_indexDir.empty()) {
        std::error_code ec;
        fs::create_directories(_indexDir, ec);
    }

    std::string path = makePath(key);
    auto tree = std::make_unique<BTree>(path);

    _indexes[key] = Entry{ std::move(tree) };
}

void IndexManager::drop(const std::string& db,
                        const std::string& table,
                        const std::string& column) {
    std::string key = makeKey(db, table, column);
    auto it = _indexes.find(key);
    if (it == _indexes.end()) return;

    // сохраняем на диск перед выгрузкой.
    try {
        it->second.tree->flush();
    } catch (...) {}

    std::string path = makePath(key);
    std::error_code ec;
    fs::remove(path, ec);

    _indexes.erase(it);
}

void IndexManager::insert(const std::string& db,
                          const std::string& table,
                          const std::string& column,
                          const Value& key,
                          RowId rid) {
    Entry* e = findEntry(db, table, column);
    if (!e) return;  // индекса нет — молча игнорируем

    e->tree->insert(key, rid);
}

bool IndexManager::erase(const std::string& db,
                         const std::string& table,
                         const std::string& column,
                         const Value& key) {
    Entry* e = findEntry(db, table, column);
    if (!e) return false;

    return e->tree->erase(key);
}

std::optional<RowId> IndexManager::find(const std::string& db,
                                        const std::string& table,
                                        const std::string& column,
                                        const Value& key) {
    Entry* e = findEntry(db, table, column);
    if (!e) return std::nullopt;

    return e->tree->find(key);
}

std::vector<RowId> IndexManager::scanRange(const std::string& db,
                                           const std::string& table,
                                           const std::string& column,
                                           const Value& lo,
                                           const Value& hi) {
    Entry* e = findEntry(db, table, column);
    if (!e) return {};

    return e->tree->scanRange(lo, hi);
}

void IndexManager::flushAll() {
    for (auto& [key, entry] : _indexes) {
        try {
            entry.tree->flush();
        } catch (...) {
            // flushAll не должен падать
        }
    }
}

void IndexManager::dropDatabase(const std::string& db) {
    std::string prefix = db + ".";
    for (auto it = _indexes.begin(); it != _indexes.end();) {
        if (it->first.rfind(prefix, 0) == 0) {
            std::string path = makePath(it->first);
            std::error_code ec;
            fs::remove(path, ec);
            it = _indexes.erase(it);
        } 
        else {
            ++it;
        }
    }
}