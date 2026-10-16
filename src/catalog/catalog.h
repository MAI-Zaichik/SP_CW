#pragma once
#include "../parser/ast.h"
#include "../core/types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <stdexcept>

struct TableSchema {
    std::string db;
    std::string table;
    std::vector<ColumnDef> columns;

    int indexOf(const std::string& name) const {
        for (int i = 0; i < (int)columns.size(); ++i)
            if (columns[i].name == name) return i;
        return -1;
    }
};

class Catalog {
public:
    // path to catalog.json
    explicit Catalog(const std::string& path) : _path(path) { load(); }
    void save() const {
        nlohmann::json j;

        for (const auto& [db, tables] : _dbs) {
            nlohmann::json db_json;
            for (const auto& [tname, schema] : tables) {
                nlohmann::json cols_json;
                for (const auto& col : schema.columns) {
                    nlohmann::json cj = {
                        {"name", col.name},
                        {"type", (int)col.type},
                        {"constraint", (int)col.constraint}
                    };
                    // DEFAULT ставим только если задан и не NULL; тип известен из колонки
                    if (col.default_value && !std::holds_alternative<std::monostate>(*col.default_value)) {
                        if (std::holds_alternative<int>(*col.default_value))
                            cj["default"] = std::get<int>(*col.default_value);
                        else
                            cj["default"] = std::string(std::get<std::string_view>(*col.default_value));
                    }
                    cols_json.push_back(cj);
                }
                db_json[tname] = cols_json;
            }
            j[db] = db_json;
        }

        // пишем во временный файл (атомарность)
        std::string tmp = _path + ".tmp";
        {
            std::ofstream f(tmp);
            if (!f) throw std::runtime_error("Catalog: cannot open " + tmp);
            f << j.dump(2);
            f.flush();
            if (!f) throw std::runtime_error("Catalog: write failed " + tmp);
        }
        std::filesystem::rename(tmp, _path);
    }

    void createDatabase(const std::string& db) {
        if (_dbs.count(db)) throw std::runtime_error("Database already exists: " + db);
        _dbs[db];
        save();
    }

    void dropDatabase(const std::string& db) {
        requireDb(db);
        _dbs.erase(db);
        save();
    }

    bool databaseExists(const std::string& db) const {
        return _dbs.count(db) > 0;
    }

    void createTable(const std::string& db, const std::string& table, const std::vector<ColumnDef>& columns)
    {
        requireDb(db);
        auto& tables = _dbs.at(db);
        if (tables.count(table)) throw std::runtime_error("Table already exists: " + table);
        tables[table] = TableSchema{db, table, columns};
        save();
    }

    void dropTable(const std::string& db, const std::string& table) {
        requireDb(db);
        requireTable(db, table);
        _dbs.at(db).erase(table);
        save();
    }

    bool tableExists(const std::string& db, const std::string& table) const {
        return _dbs.count(db) && _dbs.at(db).count(table);
    }

    const TableSchema& getSchema(const std::string& db, const std::string& table) const {
        requireDb(db);
        requireTable(db, table);
        return _dbs.at(db).at(table);
    }

    std::vector<std::string> getTableNames(const std::string& db) const {
        requireDb(db);
        std::vector<std::string> names;
        for (const auto& [tname, _] : _dbs.at(db))
            names.push_back(tname);
        return names;
    }

private:
    std::string _path;
    std::unordered_map<std::string,
        std::unordered_map<std::string, TableSchema>> _dbs;

    void requireDb(const std::string& db) const {
        if (!_dbs.count(db)) throw std::runtime_error("Database not found: " + db);
    }

    void requireTable(const std::string& db, const std::string& table) const {
        if (!_dbs.count(db) || !_dbs.at(db).count(table))
            throw std::runtime_error("Table not found: " + db + "." + table);
    }

    void load() {
        std::ifstream f(_path);
        if (!f) return;
        nlohmann::json j = nlohmann::json::parse(f);
        for (const auto& [db, tables] : j.items()) {
            for (const auto& [tname, cols_json] : tables.items()) {
                std::vector<ColumnDef> cols;
                for (const auto& c : cols_json) {
                    ColumnDef cd{
                        c["name"].get<std::string>(),
                        (ColumnType)c["type"].get<int>(),
                        (Constraint)c["constraint"].get<int>(),
                        std::nullopt
                    };
                    // старый catalog.json без "default" -> остаётся nullopt
                    if (c.contains("default")) {
                        if (cd.type == ColumnType::INT) cd.default_value = c["default"].get<int>();
                        else cd.default_value = c["default"].get<std::string>();
                    }
                    cols.push_back(cd);
                }
                _dbs[db][tname] = TableSchema{db, tname, cols};
            }
        }
    }
};