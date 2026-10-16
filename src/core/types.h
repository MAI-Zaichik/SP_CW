// Общие типы, структуры и т.п

#ifndef SP_CW_TYPES_H
#define SP_CW_TYPES_H

#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <vector>
#include <cstdint>

// строки хранятся как string_view без копий
using Value = std::variant<std::monostate, int, std::string_view>;
using Offset = std::size_t; // под вопросом. Для pager-lite не нужен, для full-pager нужен

// Тип данных колонки
enum class ColumnType {
    INT,
    STRING
};

// ограничение значений в колонках
enum class Constraint {
    NONE,           // обычная колонка, может быть null
    NOT_NULL,       // низя быть null
    INDEXED         // уникальное поле ну и индекс деревянный, null запрещен
};

struct Column{
    std::string name;
    ColumnType type;
    Constraint constraint = Constraint::NONE;
};

struct Schema{
    std::vector<Column> columns;
};

struct Row{
    std::vector<Value> values;
};

struct Pointer{
    uint32_t page_id;
    uint16_t slot_id;
};

using RowId = Pointer;
// поддерживаемые типы: null/int/string
// monostate = null

// ссылка на таблицу из запроса, по сути просто её название
// users    => db = пустое, table = users
// db.users => db = db, table = users

struct TableReference {
    std::optional<std::string> db; // optional - может и не быть этого значения, типа: "db"/{}
    std::string table;
};
#endif //SP_CW_TYPES_H