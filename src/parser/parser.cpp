#include "parser/parser.h"
#include "core/exceptions.h"
#include "core/string_pool.h"
#include <charconv>

Parser::Parser(std::vector<Token> tokens)
    : _vector(std::move(tokens)), _position(0) {}

// возвращает текущий токен без сдвига
Token& Parser::current() {
    if (_position >= _vector.size())
        throw SyntaxError("неожиданный конец ввода");
    return _vector[_position];
}

// возвращает токен со смещением, позицию не двигает
Token& Parser::peek(size_t offset) {
    size_t new_pos = _position + offset;
    if (new_pos >= _vector.size()) return _vector.back();
    return _vector[new_pos];
}

// проверяет тип текущего токена без сдвига
bool Parser::check(TokenType type) {
    return current().type == type;
}

// проверяет тип, сдвигается вперёд, при несовпадении - ошибка
Token Parser::consume(TokenType expected, const char* what) {
    if (!check(expected)) {
        // у END_OF_FILE и прочих служебных value пустой - показываем имя типа
        std::string found = current().value.empty() ? tokenName(current().type) : current().value;
        std::string exp = what ? what : tokenName(expected);
        throw SyntaxError(
            "Строка " + std::to_string(current().line) +
            ": ожидается " + exp +
            ", найдено '" + found + "'"
        );
    }
    Token token = current();
    _position++;
    return token;
}

// мягкая проверка - если совпало, сдвигается и возвращает true
bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    _position++;
    return true;
}

// точка входа - парсит одну команду и ожидает ;
ASTNodePtr Parser::parse() {
    ASTNodePtr node = parseStatement();
    consume(TokenType::SYM_SEMICOLON);
    return node;
}

// распределяет по типу команды
ASTNodePtr Parser::parseStatement() {
    switch (current().type) {
        case TokenType::K_CREATE: return parseCreate();
        case TokenType::K_DROP:   return parseDrop();
        case TokenType::K_USE:    return parseUse();
        case TokenType::K_INSERT: return parseInsert();
        case TokenType::K_UPDATE: return parseUpdate();
        case TokenType::K_DELETE: return parseDelete();
        case TokenType::K_SELECT: return parseSelect();
        default:
            throw SyntaxError(
                "Строка " + std::to_string(current().line) +
                ": неизвестная команда '" + current().value + "'"
            );
    }
}

// проваливается в -> CREATE DATABASE или CREATE TABLE
ASTNodePtr Parser::parseCreate() {
    consume(TokenType::K_CREATE);
    if (check(TokenType::K_DATABASE)) return parseCreateDatabase();
    if (check(TokenType::K_TABLE))    return parseCreateTable();
    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось DATABASE или TABLE после CREATE"
    );
}

// проваливается в -> DROP DATABASE или DROP TABLE
ASTNodePtr Parser::parseDrop() {
    consume(TokenType::K_DROP);
    if (check(TokenType::K_DATABASE)) return parseDropDatabase();
    if (check(TokenType::K_TABLE))    return parseDropTable();
    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось DATABASE или TABLE после DROP"
    );
}

ASTNodePtr Parser::parseCreateDatabase() {
    consume(TokenType::K_DATABASE);
    auto node  = std::make_unique<CreateDatabaseStmt>();
    node->name = consume(TokenType::IDENT, "имя базы данных").value;
    return node;
}

ASTNodePtr Parser::parseDropDatabase() {
    consume(TokenType::K_DATABASE);
    auto node  = std::make_unique<DropDatabaseStmt>();
    node->name = consume(TokenType::IDENT, "имя базы данных").value;
    return node;
}

ASTNodePtr Parser::parseUse() {
    consume(TokenType::K_USE);
    auto node  = std::make_unique<UseStmt>();
    node->name = consume(TokenType::IDENT, "имя базы данных").value;
    return node;
}

// CREATE TABLE name (col TYPE CONSTRAINT, ...)
ASTNodePtr Parser::parseCreateTable() {
    consume(TokenType::K_TABLE);
    auto node   = std::make_unique<CreateTableStmt>();
    node->table = parseTableRef();
    consume(TokenType::SYM_LPAREN);
    node->columns.push_back(parseColumnDef());
    while (match(TokenType::SYM_COMMA))
        node->columns.push_back(parseColumnDef());
    consume(TokenType::SYM_RPAREN);
    return node;
}

ASTNodePtr Parser::parseDropTable() {
    consume(TokenType::K_TABLE);
    auto node   = std::make_unique<DropTableStmt>();
    node->table = parseTableRef();
    return node;
}

// INSERT INTO table (cols) VALUE (vals), ...
ASTNodePtr Parser::parseInsert() {
    consume(TokenType::K_INSERT);
    consume(TokenType::K_INTO);
    auto node   = std::make_unique<InsertStmt>();
    node->table = parseTableRef();

    consume(TokenType::SYM_LPAREN);
    node->columns.push_back(consume(TokenType::IDENT, "имя колонки").value);
    while (match(TokenType::SYM_COMMA))
        node->columns.push_back(consume(TokenType::IDENT, "имя колонки").value);
    consume(TokenType::SYM_RPAREN);

    consume(TokenType::K_VALUE);

    // парсим одну строку значений ( val, val, ... )
    auto parseOneRow = [&]() {
        std::vector<Value> row;
        consume(TokenType::SYM_LPAREN);
        row.push_back(parseValue());
        while (match(TokenType::SYM_COMMA))
            row.push_back(parseValue());
        consume(TokenType::SYM_RPAREN);
        return row;
    };

    node->rows.push_back(parseOneRow());
    while (match(TokenType::SYM_COMMA))
        node->rows.push_back(parseOneRow());

    return node;
}

// UPDATE table SET col=val, ... WHERE condition
ASTNodePtr Parser::parseUpdate() {
    consume(TokenType::K_UPDATE);
    auto node   = std::make_unique<UpdateStmt>();
    node->table = parseTableRef();
    consume(TokenType::K_SET);

    auto parseAssignment = [&]() {
        std::string col = consume(TokenType::IDENT, "имя колонки").value;
        consume(TokenType::SYM_EQ); // одиночный = для присваивания, не ==
        return std::make_pair(col, parseValue());
    };

    node->assignments.push_back(parseAssignment());
    while (match(TokenType::SYM_COMMA))
        node->assignments.push_back(parseAssignment());

    node->where = match(TokenType::K_WHERE) ? parseExpr() : nullptr;
    return node;
}

// DELETE FROM table WHERE condition
ASTNodePtr Parser::parseDelete() {
    consume(TokenType::K_DELETE);
    consume(TokenType::K_FROM);
    auto node   = std::make_unique<DeleteStmt>();
    node->table = parseTableRef();
    node->where = match(TokenType::K_WHERE) ? parseExpr() : nullptr;
    return node;
}

// SELECT * → columns в ноде пустой (сигнал для executor'а взять все)
// SELECT col1, col2 → columns заполнен; SELECT без * и без колонок — SyntaxError
ASTNodePtr Parser::parseSelect() {
    consume(TokenType::K_SELECT);
    auto node = std::make_unique<SelectStmt>();

    if (!match(TokenType::SYM_STAR)) {
        // SELECT col [AS alias], ...
        auto parseCol = [&]() {
            SelectColumn sc;
            // агрегатные: SUM(col) / COUNT(col) / AVG(col)
            if      (match(TokenType::K_SUM))   sc.agg = AggFunc::SUM;
            else if (match(TokenType::K_COUNT)) sc.agg = AggFunc::COUNT;
            else if (match(TokenType::K_AVG))   sc.agg = AggFunc::AVG;

            if (sc.agg != AggFunc::NONE) {
                consume(TokenType::SYM_LPAREN);
                sc.name = consume(TokenType::IDENT, "имя колонки").value;
                consume(TokenType::SYM_RPAREN);
            } else {
                sc.name = consume(TokenType::IDENT, "имя колонки").value;
            }

            sc.alias = std::nullopt;
            if (match(TokenType::K_AS))
                sc.alias = consume(TokenType::IDENT, "имя алиаса").value;
            return sc;
        };
        node->columns.push_back(parseCol());
        while (match(TokenType::SYM_COMMA))
            node->columns.push_back(parseCol());
    }

    consume(TokenType::K_FROM);
    node->table = parseTableRef();
    node->where = match(TokenType::K_WHERE) ? parseExpr() : nullptr;
    return node;
}

// парсит table или db.table
TableReference Parser::parseTableRef() {
    Token first = consume(TokenType::IDENT, "имя таблицы");
    if (match(TokenType::SYM_DOT)) {
        Token second = consume(TokenType::IDENT, "имя таблицы");
        return TableReference{ first.value, second.value };
    }
    return TableReference{ std::nullopt, first.value };
}

// имя колонки + тип (INT/STRING) + опциональный constraint
ColumnDef Parser::parseColumnDef() {
    ColumnDef def;
    def.name = consume(TokenType::IDENT, "имя колонки").value;

    if      (match(TokenType::K_INT))    def.type = ColumnType::INT;
    else if (match(TokenType::K_STRING)) def.type = ColumnType::STRING;
    else throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидался тип колонки INT или STRING"
    );

    def.constraint = Constraint::NONE;
    if      (match(TokenType::K_NOT_NULL)) def.constraint = Constraint::NOT_NULL;
    else if (match(TokenType::K_INDEXED))  def.constraint = Constraint::INDEXED;

    if (match(TokenType::K_DEFAULT)) {
        Value dv = parseValue(); // int/string/NULL
        // тип DEFAULT должен совпадать с типом колонки (NULL допустим)
        bool ok = std::holds_alternative<std::monostate>(dv)
               || (std::holds_alternative<int>(dv) && def.type == ColumnType::INT)
               || (std::holds_alternative<std::string_view>(dv) && def.type == ColumnType::STRING);
        if (!ok)
            throw SyntaxError("Строка " + std::to_string(current().line) +
                              ": DEFAULT-значение не совпадает с типом колонки");
        def.default_value = dv;
    }

    return def;
}

// LIT_INT в int, при negative=true собирает "-N" и парсит как одно число
int Parser::parseIntFromToken(const Token& t, bool negative) {
    std::string s = negative ? ("-" + t.value) : t.value;
    int result;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    if (ec != std::errc{})
        throw SyntaxError(
            "Строка " + std::to_string(t.line) +
            ": число '" + s + "' выходит за пределы int"
        );
    return result;
}

// число (со знаком), строка или NULL
Value Parser::parseValue() {
    bool negative = match(TokenType::SYM_MINUS);
    if (check(TokenType::LIT_INT)) {
        Token t = consume(TokenType::LIT_INT);
        return parseIntFromToken(t, negative);
    }
    if (negative) throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось число после '-'"
    );
    if (check(TokenType::LIT_STRING))
        return StringPool::instance().intern(consume(TokenType::LIT_STRING).value);
    if (match(TokenType::K_NULL))
        return std::monostate{};
    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось значение (число, строка или NULL)"
    );
}

// счетчик глубины: ++ на входе в parseExpr и -- при выходе
namespace {
struct DepthGuard {
    int& d;
    explicit DepthGuard(int& depth) : d(depth) { ++d; }
    ~DepthGuard() { --d; }
};
}

// SQL-приоритет: OR слабее AND
// a OR b AND c   ==   a OR (b AND c)
ExprPtr Parser::parseExpr() {
    // каждый уровень скобок заходит сюда заново через parsePrimary - ловим переполнение стека
    DepthGuard guard(_depth);
    if (_depth > MAX_EXPR_DEPTH)
        throw SyntaxError("слишком глубоко вложенное выражение");
    return parseOr();
}

ExprPtr Parser::parseOr() {
    ExprPtr left = parseAnd();
    while (match(TokenType::K_OR)) {
        ExprPtr right = parseAnd();
        auto node     = std::make_unique<LogicalExpr>();
        node->left    = std::move(left);
        node->oper    = LogicalOper::OR;
        node->right   = std::move(right);
        left          = std::move(node);
    }
    return left;
}

ExprPtr Parser::parseAnd() {
    ExprPtr left = parseComparison();
    while (match(TokenType::K_AND)) {
        ExprPtr right = parseComparison();
        auto node     = std::make_unique<LogicalExpr>();
        node->left    = std::move(left);
        node->oper    = LogicalOper::AND;
        node->right   = std::move(right);
        left          = std::move(node);
    }
    return left;
}

// операторы сравнения, BETWEEN, LIKE
ExprPtr Parser::parseComparison() {
    ExprPtr left = parsePrimary();

    if (match(TokenType::K_BETWEEN)) {
        ExprPtr lo = parsePrimary();
        consume(TokenType::K_AND);
        ExprPtr hi  = parsePrimary();
        auto node   = std::make_unique<BetweenExpr>();
        node->value = std::move(left);
        node->low   = std::move(lo);
        node->high  = std::move(hi);
        return node;
    }

    if (match(TokenType::K_LIKE)) {
        auto node     = std::make_unique<LikeExpr>();
        node->value   = std::move(left);
        node->pattern = consume(TokenType::LIT_STRING).value;
        return node;
    }

    BinaryOper op = BinaryOper::EQ;
    bool found = true;
    if      (match(TokenType::OP_EQ))  op = BinaryOper::EQ;
    else if (match(TokenType::OP_NEQ)) op = BinaryOper::NEQ;
    else if (match(TokenType::OP_LT))  op = BinaryOper::LT;
    else if (match(TokenType::OP_GT))  op = BinaryOper::GT;
    else if (match(TokenType::OP_LTE)) op = BinaryOper::LTE;
    else if (match(TokenType::OP_GTE)) op = BinaryOper::GTE;
    else found = false;

    if (found) {
        ExprPtr right = parsePrimary();
        auto node     = std::make_unique<BinaryExpr>();
        node->left    = std::move(left);
        node->oper    = op;
        node->right   = std::move(right);
        return node;
    }

    return left;
}

// нижний уровень выражения: конкретное значение, имя колонки или (выражение в скобках)
ExprPtr Parser::parsePrimary() {
    if (match(TokenType::SYM_LPAREN)) {
        ExprPtr inner = parseExpr();
        consume(TokenType::SYM_RPAREN);
        return inner;
    }

    bool negative = match(TokenType::SYM_MINUS);

    if (check(TokenType::LIT_INT)) {
        Token t     = consume(TokenType::LIT_INT);
        auto node   = std::make_unique<LiteralExpr>();
        node->value = parseIntFromToken(t, negative);
        return node;
    }

    if (negative) throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось число после '-'"
    );

    if (check(TokenType::LIT_STRING)) {
        auto node   = std::make_unique<LiteralExpr>();
        node->value = StringPool::instance().intern(consume(TokenType::LIT_STRING).value);
        return node;
    }

    if (match(TokenType::K_NULL)) {
        auto node   = std::make_unique<LiteralExpr>();
        node->value = std::monostate{};
        return node;
    }

    if (check(TokenType::IDENT)) {
        auto node  = std::make_unique<ColumnRefExpr>();
        node->name = consume(TokenType::IDENT).value;
        return node;
    }

    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось выражение (имя колонки, число, строка, NULL или '(')"
    );
}