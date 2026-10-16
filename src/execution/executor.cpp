#include "execution/executor.h"
#include "execution/serializer.h"
#include <regex>
#include <stdexcept>
#include <unordered_map>

// checker
static void checkType(const Value& v, ColumnType t, const std::string& col) {
    if (std::holds_alternative<std::monostate>(v)) return;
    if (t == ColumnType::INT && !std::holds_alternative<int>(v)) {
        throw std::runtime_error("Type mismatch: column '" + col + "' expects INT");
    }
    if (t == ColumnType::STRING && !std::holds_alternative<std::string_view>(v)) {
        throw std::runtime_error("Type mismatch: column '" + col + "' expects STRING");
    }
}

Executor::Executor(Catalog& catalog, Storage& storage)
    : _catalog(catalog), _storage(storage), _index("./data/indexes") {}

ExecuteResult Executor::execute(ASTNode& node) {
    _result = {};
    try {
        node.accept(*this);
    } catch (const std::exception& e) {
        _result.ok = false;
        _result.message = e.what();
    }
    return _result;
}

std::pair<std::string, std::string> Executor::resolve(const TableReference& ref) const {
    std::string db = ref.db.value_or(_current_db);
    if (db.empty()) throw std::runtime_error("No database selected. Use USE <db>.");
    return {db, ref.table};
}

// DDL db
void Executor::visit(CreateDatabaseStmt& s) {
    _catalog.createDatabase(s.name);
    try {
        _storage.createDatabase(s.name);
    } catch (...) {
        // каталог не должен ссылаться на несозданную бд
        _catalog.dropDatabase(s.name);
        throw;
    }
    _result.message = "Database '" + s.name + "' created.";
}

void Executor::visit(DropDatabaseStmt& s) {
    _index.dropDatabase(s.name);
    _catalog.dropDatabase(s.name);
    _storage.dropDatabase(s.name);
    _result.message = "Database '" + s.name + "' dropped.";
}

void Executor::visit(UseStmt& s) {
    if (!_catalog.databaseExists(s.name))
        throw std::runtime_error("Unknown database: " + s.name);
    _current_db = s.name;
    rebuildIndexes(s.name);
    _result.message = "Database changed to '" + s.name + "'.";
}

// DDL table
void Executor::visit(CreateTableStmt& s) {
    auto [db, table] = resolve(s.table);
    _catalog.createTable(db, table, s.columns);
    try {
        _storage.createTable(db, table);
        for (const auto& col : s.columns) {
            if (col.constraint == Constraint::INDEXED) {
                _index.create(db, table, col.name);
            }
        }
    } catch (...) {
        // каталог не должен ссылаться на несозданную таблицу
        _catalog.dropTable(db, table);
        throw;
    }

    _result.message = "Table '" + db + "." + table + "' created.";
}

void Executor::visit(DropTableStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);

    for (const auto& col : schema.columns) {
        if (col.constraint == Constraint::INDEXED) {
            _index.drop(db, table, col.name);
        }
    }

    _catalog.dropTable(db, table);
    _storage.dropTable(db, table);
    _result.message = "Table '" + db + "." + table + "' dropped.";
}

// INSERT
void Executor::visit(InsertStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);
    int inserted = 0;

    for (const auto& src_row : s.rows) {
        if (src_row.size() != s.columns.size()) {
            throw std::runtime_error("INSERT: column count mismatch");
        }

        std::vector<Value> full_row(schema.columns.size(), std::monostate{});

        // предзаполняем дефолтами; переданные значения перекроют их дальше
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].default_value)
                full_row[i] = *schema.columns[i].default_value;
        }

        for (size_t i = 0; i < s.columns.size(); ++i) {
            int idx = schema.indexOf(s.columns[i]);
            if (idx < 0) throw std::runtime_error("Unknown column: " + s.columns[i]);
            full_row[idx] = src_row[i];
        }

        for (size_t i = 0; i < schema.columns.size(); ++i) {
            const auto& col = schema.columns[i];
            bool is_null = std::holds_alternative<std::monostate>(full_row[i]);
            if (is_null && col.constraint != Constraint::NONE) {
                throw std::runtime_error("NOT_NULL/INDEXED violation: " + col.name);
            }
            checkType(full_row[i], col.type, col.name);
            // проверка уникальности до записи
            if (col.constraint == Constraint::INDEXED &&
                _index.find(db, table, col.name, full_row[i]).has_value()) {
                throw std::runtime_error("Unique violation: column '" + col.name + "'");
            }
        }
        RowId rid = _storage.write(db, table, Serializer::encodeRow(full_row));

        // upd indexes
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].constraint == Constraint::INDEXED) {
                _index.insert(db, table, schema.columns[i].name, full_row[i], rid);
            }
        }
        ++inserted;
    }

    _result.message = "Inserted " + std::to_string(inserted) + " row(s).";
}

// UPDATE
void Executor::visit(UpdateStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);

    for (const auto& [col, val] : s.assignments) {
        int idx = schema.indexOf(col);
        if (idx < 0) throw std::runtime_error("Unknown column: " + col);
        checkType(val, schema.columns[idx].type, col);
    }

    auto records = _storage.scan(db, table);
    int updated = 0;

    for (auto& [rid, bytes] : records) {
        auto row = Serializer::decodeRow(bytes);
        auto row_map = makeRowMap(row, schema);
        if (!matchRow(s.where.get(), row_map)) continue;

        auto new_row = row;
        for (const auto& [col, val] : s.assignments) {
            new_row[schema.indexOf(col)] = val;
        }

        // уникальность проверяем до изменения (старый ключ в индексе, поэтому self не совпадет)
        for (const auto& [col, val] : s.assignments) {
            int ci = schema.indexOf(col);
            if (schema.columns[ci].constraint == Constraint::INDEXED &&
                BTree::compareValues(val, row[ci]) != 0 &&
                _index.find(db, table, col, val).has_value()) {
                throw std::runtime_error("Unique violation: column '" + col + "'");
            }
        }

        // снимаем ключи из индексов перед удалением
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].constraint == Constraint::INDEXED) {
                _index.erase(db, table, schema.columns[i].name, row[i]);
            }
        }
        
        RowId new_rid = _storage.update(db, table, rid, Serializer::encodeRow(new_row));

        for (size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].constraint == Constraint::INDEXED) {
                _index.insert(db, table, schema.columns[i].name, new_row[i], new_rid);
            }
        }
        ++updated;
    }

    _result.message = "Updated " + std::to_string(updated) + " row(s).";
}

// DELETE
void Executor::visit(DeleteStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);
    auto records = _storage.scan(db, table);
    int deleted = 0;

    for (auto& [rid, bytes] : records) {
        auto row = Serializer::decodeRow(bytes);
        auto row_map = makeRowMap(row, schema);
        if (!matchRow(s.where.get(), row_map)) continue;

        // снимаем ключи из индексов перед удалением
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].constraint == Constraint::INDEXED) {
                _index.erase(db, table, schema.columns[i].name, row[i]);
            }
        }
        _storage.remove(db, table, rid);
        ++deleted;
    }

    _result.message = "Deleted " + std::to_string(deleted) + " row(s).";
}

// распознаёт WHERE вида (col == литерал) / (литерал == col) на INDEXED-колонке
static std::optional<std::pair<std::string, Value>>
indexedEquality(const ExprNode* where, const TableSchema& schema) {
    const auto* bin = dynamic_cast<const BinaryExpr*>(where);
    if (!bin || bin->oper != BinaryOper::EQ) return std::nullopt;

    const auto* col = dynamic_cast<const ColumnRefExpr*>(bin->left.get());
    const auto* lit = dynamic_cast<const LiteralExpr*>(bin->right.get());
    if (!col || !lit) {
        col = dynamic_cast<const ColumnRefExpr*>(bin->right.get());
        lit = dynamic_cast<const LiteralExpr*>(bin->left.get());
    }
    if (!col || !lit) return std::nullopt;

    int ci = schema.indexOf(col->name);
    if (ci < 0 || schema.columns[ci].constraint != Constraint::INDEXED) return std::nullopt;
    return std::make_pair(col->name, lit->value);
}

// имя агрегатной функции для ключа JSON по умолчанию
static const char* aggName(AggFunc f) {
    switch (f) {
        case AggFunc::SUM:   return "SUM";
        case AggFunc::COUNT: return "COUNT";
        case AggFunc::AVG:   return "AVG";
        default:             return "";
    }
}

// SELECT
void Executor::visit(SelectStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);

    std::vector<std::string> proj_cols;
    if (s.columns.empty()) {
        for (const auto& c : schema.columns) proj_cols.push_back(c.name);
    } else {
        for (const auto& sc : s.columns) proj_cols.push_back(sc.name);
    }

    std::unordered_map<std::string, std::string> aliases;
    for (const auto& sc : s.columns) {
        aliases[sc.name] = sc.alias.value_or(sc.name);
    }

    // путь по индексу для (INDEXED-колонка == литерал)
    // если индекс ничего не вернул - используется полный scan
    std::vector<std::pair<RowId, std::vector<uint8_t>>> records;
    bool used_index = false;
    if (auto hit = indexedEquality(s.where.get(), schema)) {
        if (auto rid = _index.find(db, table, hit->first, hit->second)) {
            auto bytes = _storage.read(db, table, *rid);
            if (!bytes.empty()) {
                records.push_back({*rid, bytes});
                used_index = true;
            }
        }
    }
    if (!used_index) {
        records = _storage.scan(db, table);
    }

    // агрегаты SUM/COUNT/AVG: либо все колонки агрегатные, либо ни одной
    bool has_agg = false, has_plain = false;
    for (const auto& sc : s.columns) {
        if (sc.agg != AggFunc::NONE) has_agg = true; else has_plain = true;
    }
    if (has_agg && has_plain)
        throw std::runtime_error("Cannot mix aggregate and plain columns in SELECT");

    nlohmann::json result = nlohmann::json::array();

    if (has_agg) {
        // индексы колонок резолвим один раз; заодно проверяем типы аргументов
        std::vector<int> col_idx(s.columns.size());
        for (size_t k = 0; k < s.columns.size(); ++k) {
            int ci = schema.indexOf(s.columns[k].name);
            if (ci < 0) throw std::runtime_error("Unknown column in SELECT: " + s.columns[k].name);
            if ((s.columns[k].agg == AggFunc::SUM || s.columns[k].agg == AggFunc::AVG) &&
                schema.columns[ci].type != ColumnType::INT)
                throw std::runtime_error("SUM/AVG requires INT column: " + s.columns[k].name);
            col_idx[k] = ci;
        }

        std::vector<long long> sums(s.columns.size(), 0);
        std::vector<long long> counts(s.columns.size(), 0);

        for (auto& [rid, bytes] : records) {
            auto row = Serializer::decodeRow(bytes);
            auto row_map = makeRowMap(row, schema);
            if (!matchRow(s.where.get(), row_map)) continue;
            for (size_t k = 0; k < s.columns.size(); ++k) {
                const Value& v = row[col_idx[k]];
                if (std::holds_alternative<std::monostate>(v)) continue; // NULL пропускаем
                ++counts[k];
                if (std::holds_alternative<int>(v)) sums[k] += std::get<int>(v);
            }
        }

        nlohmann::json obj;
        for (size_t k = 0; k < s.columns.size(); ++k) {
            const auto& sc = s.columns[k];
            std::string key = sc.alias.value_or(std::string(aggName(sc.agg)) + "(" + sc.name + ")");
            if (sc.agg == AggFunc::COUNT) {
                obj[key] = counts[k];
            } else if (sc.agg == AggFunc::SUM) {
                obj[key] = sums[k];
            } else { // AVG
                if (counts[k] == 0) obj[key] = nullptr;
                else obj[key] = static_cast<double>(sums[k]) / static_cast<double>(counts[k]);
            }
        }
        result.push_back(std::move(obj));

        _result.message = "OK";
        _result.data = std::move(result);
        return;
    }

    for (auto& [rid, bytes] : records) {
        auto row_map = makeRowMap(Serializer::decodeRow(bytes), schema);
        if (!matchRow(s.where.get(), row_map)) continue;

        nlohmann::json obj;
        for (const auto& col : proj_cols) {
            if (!row_map.count(col)) throw std::runtime_error("Unknown column in SELECT: " + col);
            std::string out_name = aliases.count(col) ? aliases.at(col) : col;
            obj[out_name] = valueToJson(row_map.at(col));
        }
        result.push_back(std::move(obj));
    }

    _result.message = "OK";
    _result.data = std::move(result);
}

void Executor::rebuildIndexes(const std::string& db) {
    for (const auto& tname : _catalog.getTableNames(db)) {
        const auto& schema = _catalog.getSchema(db, tname);
        for (const auto& col : schema.columns) {
            if (col.constraint == Constraint::INDEXED)
                _index.create(db, tname, col.name);
        }
    }
}

// helpers
std::unordered_map<std::string, Value> Executor::makeRowMap(const std::vector<Value>& row, const TableSchema& schema) const {
    std::unordered_map<std::string, Value> m;
    for (size_t i = 0; i < schema.columns.size() && i < row.size(); ++i) {
        m[schema.columns[i].name] = row[i];
    }
    return m;
}

nlohmann::json Executor::valueToJson(const Value& v) {
    if (std::holds_alternative<std::monostate>(v)) return nullptr;
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    return std::string(std::get<std::string_view>(v));
}

bool Executor::matchRow(const ExprNode* where, const std::unordered_map<std::string, Value>& row) const {
    if (!where) return true;
    Value result = evalExpr(where, row);
    if (std::holds_alternative<int>(result)) return std::get<int>(result) != 0;
    return false;
}

// WHERE recursive eval
Value Executor::evalExpr(const ExprNode* expr, const std::unordered_map<std::string, Value>& row) const {
    if (const auto* lit = dynamic_cast<const LiteralExpr*>(expr)) {
        return lit->value;
    }

    if (const auto* col = dynamic_cast<const ColumnRefExpr*>(expr)) {
        auto it = row.find(col->name);
        if (it == row.end()) throw std::runtime_error("Unknown column in WHERE: " + col->name);
        return it->second;
    }

    if (const auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        Value lv = evalExpr(bin->left.get(), row);
        Value rv = evalExpr(bin->right.get(), row);

        if (std::holds_alternative<std::monostate>(lv) || std::holds_alternative<std::monostate>(rv))
            return 0;

        auto cmp = [&]() -> int {
            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv))
                return std::get<int>(lv) - std::get<int>(rv);
            if (std::holds_alternative<std::string_view>(lv) && std::holds_alternative<std::string_view>(rv))
                return std::get<std::string_view>(lv).compare(std::get<std::string_view>(rv));
            throw std::runtime_error("Type mismatch in WHERE comparison");
        };

        switch (bin->oper) {
            case BinaryOper::EQ: return (cmp() == 0) ? 1 : 0;
            case BinaryOper::NEQ: return (cmp() != 0) ? 1 : 0;
            case BinaryOper::LT: return (cmp() < 0) ? 1 : 0;
            case BinaryOper::GT: return (cmp() > 0) ? 1 : 0;
            case BinaryOper::LTE: return (cmp() <= 0) ? 1 : 0;
            case BinaryOper::GTE: return (cmp() >= 0) ? 1 : 0;
            default: throw std::runtime_error("Unknown binary operator");
        }
    }

    if (const auto* bet = dynamic_cast<const BetweenExpr*>(expr)) {
        Value val = evalExpr(bet->value.get(), row);
        Value lo = evalExpr(bet->low.get(), row);
        Value hi = evalExpr(bet->high.get(), row);

        // NULL в любой из частей - условие неопределено, считаем ложным
        if (std::holds_alternative<std::monostate>(val) ||
            std::holds_alternative<std::monostate>(lo) ||
            std::holds_alternative<std::monostate>(hi))
            return 0;

        // интервал [lo, hi) по ТЗ - верхняя граница исключается
        if (std::holds_alternative<int>(val) &&
            std::holds_alternative<int>(lo) && std::holds_alternative<int>(hi)) {
            int v = std::get<int>(val), l = std::get<int>(lo), h = std::get<int>(hi);
            return (v >= l && v < h) ? 1 : 0;
        }
        if (std::holds_alternative<std::string_view>(val) &&
            std::holds_alternative<std::string_view>(lo) && std::holds_alternative<std::string_view>(hi)) {
            const auto v = std::get<std::string_view>(val);
            const auto l = std::get<std::string_view>(lo);
            const auto h = std::get<std::string_view>(hi);
            return (v >= l && v < h) ? 1 : 0;
        }
        throw std::runtime_error("Type mismatch in BETWEEN");
    }

    if (const auto* like = dynamic_cast<const LikeExpr*>(expr)) {
        Value val = evalExpr(like->value.get(), row);
        if (!std::holds_alternative<std::string_view>(val)) return 0;
        try {
            std::regex re(like->pattern);
            const auto sv = std::get<std::string_view>(val);
            return std::regex_match(sv.begin(), sv.end(), re) ? 1 : 0;
        } catch (const std::regex_error&) {
            throw std::runtime_error("Invalid LIKE pattern: " + like->pattern);
        }
    }

    if (const auto* log = dynamic_cast<const LogicalExpr*>(expr)) {
        Value lv = evalExpr(log->left.get(), row);
        bool lb = std::holds_alternative<int>(lv) && std::get<int>(lv) != 0;

        if (log->oper == LogicalOper::AND) {
            if (!lb) return 0;
            Value rv = evalExpr(log->right.get(), row);
            return (std::holds_alternative<int>(rv) && std::get<int>(rv) != 0) ? 1 : 0;
        } else {
            if (lb) return 1;
            Value rv = evalExpr(log->right.get(), row);
            return (std::holds_alternative<int>(rv) && std::get<int>(rv) != 0) ? 1 : 0;
        }
    }

    throw std::runtime_error("Executor: unknown ExprNode type in WHERE");
}