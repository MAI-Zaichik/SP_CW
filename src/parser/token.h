#ifndef CW_TOKEN_H
#define CW_TOKEN_H

#include <string>
enum class TokenType {
    // ключевые слова
    K_CREATE,
    K_DROP,
    K_USE,
    K_DATABASE,
    K_TABLE,
    K_INSERT,
    K_INTO,
    K_VALUE,
    K_UPDATE,
    K_SET,
    K_DELETE,
    K_FROM,
    K_SELECT,
    K_WHERE,
    K_AND,
    K_OR,
    K_NOT_NULL,
    K_INDEXED,
    K_AS,
    K_BETWEEN,
    K_LIKE,
    K_NULL,

    // типы данных
    K_INT,
    K_STRING,

    // агрегатные функции
    K_SUM,
    K_COUNT,
    K_AVG,

    // значение по умолчанию
    K_DEFAULT,

    // операторы сравнения
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,

    // символы
    SYM_LPAREN,
    SYM_RPAREN,
    SYM_COMMA,
    SYM_SEMICOLON,
    SYM_DOT,
    SYM_STAR,
    SYM_EQ,
    SYM_MINUS,

    // значения
    LIT_INT,
    LIT_STRING,

    // идентификатор - имена: users, age....
    IDENT,

    // конец файла
    END_OF_FILE,
};

// читаемое имя типа токена для сообщений об ошибках
inline const char* tokenName(TokenType t) {
    switch (t) {
        case TokenType::K_CREATE:      return "CREATE";
        case TokenType::K_DROP:        return "DROP";
        case TokenType::K_USE:         return "USE";
        case TokenType::K_DATABASE:    return "DATABASE";
        case TokenType::K_TABLE:       return "TABLE";
        case TokenType::K_INSERT:      return "INSERT";
        case TokenType::K_INTO:        return "INTO";
        case TokenType::K_VALUE:       return "VALUE";
        case TokenType::K_UPDATE:      return "UPDATE";
        case TokenType::K_SET:         return "SET";
        case TokenType::K_DELETE:      return "DELETE";
        case TokenType::K_FROM:        return "FROM";
        case TokenType::K_SELECT:      return "SELECT";
        case TokenType::K_WHERE:       return "WHERE";
        case TokenType::K_AND:         return "AND";
        case TokenType::K_OR:          return "OR";
        case TokenType::K_NOT_NULL:    return "NOT_NULL";
        case TokenType::K_INDEXED:     return "INDEXED";
        case TokenType::K_AS:          return "AS";
        case TokenType::K_BETWEEN:     return "BETWEEN";
        case TokenType::K_LIKE:        return "LIKE";
        case TokenType::K_NULL:        return "NULL";
        case TokenType::K_INT:         return "INT";
        case TokenType::K_STRING:      return "STRING";
        case TokenType::K_SUM:         return "SUM";
        case TokenType::K_COUNT:       return "COUNT";
        case TokenType::K_AVG:         return "AVG";
        case TokenType::K_DEFAULT:     return "DEFAULT";
        case TokenType::OP_EQ:         return "==";
        case TokenType::OP_NEQ:        return "!=";
        case TokenType::OP_LT:         return "<";
        case TokenType::OP_GT:         return ">";
        case TokenType::OP_LTE:        return "<=";
        case TokenType::OP_GTE:        return ">=";
        case TokenType::SYM_LPAREN:    return "(";
        case TokenType::SYM_RPAREN:    return ")";
        case TokenType::SYM_COMMA:     return ",";
        case TokenType::SYM_SEMICOLON: return ";";
        case TokenType::SYM_DOT:       return ".";
        case TokenType::SYM_STAR:      return "*";
        case TokenType::SYM_EQ:        return "=";
        case TokenType::SYM_MINUS:     return "-";
        case TokenType::LIT_INT:       return "число";
        case TokenType::LIT_STRING:    return "строка";
        case TokenType::IDENT:         return "имя";
        case TokenType::END_OF_FILE:   return "конец ввода";
    }
    return "?";
}

struct Token {
    TokenType type;
    std::string value; // текст токена
    int line; // номер строки для логов
};

#endif //CW_TOKEN_H