#include "parser/lexer.h"
#include "core/exceptions.h"
#include <cctype>
#include <algorithm>
#include <unordered_map>


static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"CREATE",   TokenType::K_CREATE},
    {"DROP",     TokenType::K_DROP},
    {"USE",      TokenType::K_USE},
    {"DATABASE", TokenType::K_DATABASE},
    {"TABLE",    TokenType::K_TABLE},
    {"INSERT",   TokenType::K_INSERT},
    {"INTO",     TokenType::K_INTO},
    {"VALUE",    TokenType::K_VALUE},
    {"UPDATE",   TokenType::K_UPDATE},
    {"SET",      TokenType::K_SET},
    {"DELETE",   TokenType::K_DELETE},
    {"FROM",     TokenType::K_FROM},
    {"SELECT",   TokenType::K_SELECT},
    {"WHERE",    TokenType::K_WHERE},
    {"AND",      TokenType::K_AND},
    {"OR",       TokenType::K_OR},
    {"NOT_NULL", TokenType::K_NOT_NULL},
    {"INDEXED",  TokenType::K_INDEXED},
    {"AS",       TokenType::K_AS},
    {"BETWEEN",  TokenType::K_BETWEEN},
    {"LIKE",     TokenType::K_LIKE},
    {"NULL",     TokenType::K_NULL},
    {"INT",      TokenType::K_INT},
    {"STRING",   TokenType::K_STRING},
    {"SUM",      TokenType::K_SUM},
    {"COUNT",    TokenType::K_COUNT},
    {"AVG",      TokenType::K_AVG},
    {"DEFAULT",  TokenType::K_DEFAULT},
};

Lexer::Lexer(const std::string &input)
    : _input(input), _position(0), _line(1){}

char Lexer::current() const {
    if (_position >= _input.size()) return '\0';
    return _input[_position];
}

void Lexer::moveNextPos() {
    if (_position < _input.size()) {
        if (current() == '\n') _line++;
        _position++;
    }
}

void Lexer::skipSpaceAndComments() {
    while (_position < _input.size()) {
        char c = current();
        if (std::isspace(static_cast<unsigned char>(c))) {
            moveNextPos();
            continue;
        }
        // -- комментарий до конца строки
        if (c == '-' && _position + 1 < _input.size() && _input[_position + 1] == '-') {
            while (current() != '\n' && current() != '\0') moveNextPos();
            continue;
        }
        break;
    }
}

Token Lexer::readIdent() {
    int start_line = _line;
    std::string word;
    while (std::isalnum(static_cast<unsigned char>(current())) || current()=='_') {
        word += current();
        moveNextPos();
    }
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    auto lit = KEYWORDS.find(upper);
    if (lit != KEYWORDS.end()) {
        // ключевые слова в одном регистре только: SELECT & select
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (word != upper && word != lower)
            throw SyntaxError("строка " + std::to_string(start_line) +
                              ": смешивание регистров в ключевом слове '" + word + "'");
        return {lit->second, word, start_line};
    }
    return {TokenType::IDENT, word, start_line};
}


Token Lexer::readNumber() {
    int start_line = _line;
    std::string num;
    while (std::isdigit((unsigned char)current())) {
        num += current();
        moveNextPos();
    }
    return {TokenType::LIT_INT, num, start_line};
}


Token Lexer::readString() {
    int start_line = _line;
    std::string str;
    moveNextPos(); // пропскаем "
    while (current() != '"' && current() != '\0') {
        str += current();
        moveNextPos();
    }
    if (current() == '\0') {
        // возможно продолжение на следующей строке
        throw IncompleteInput("незакрытая строка, строка " + std::to_string(start_line));
    }
    moveNextPos();
    return {TokenType::LIT_STRING, str, start_line};
}

Token Lexer::readOperator() {
    int start_line = _line;
    char c = current();
    moveNextPos();
    switch (c) {
        case '(': return {TokenType::SYM_LPAREN, "(", start_line};
        case ')': return {TokenType::SYM_RPAREN, ")", start_line};
        case ',': return {TokenType::SYM_COMMA, ",", start_line};
        case ';': return {TokenType::SYM_SEMICOLON, ";", start_line};
        case '.': return {TokenType::SYM_DOT, ".", start_line};
        case '*': return {TokenType::SYM_STAR, "*", start_line};
        case '-': return {TokenType::SYM_MINUS, "-", start_line};
        case '=':
            if (current() == '=' ) {
                moveNextPos();
                return {TokenType::OP_EQ, "==", start_line};
            }
            return {TokenType::SYM_EQ, "=", start_line};
        case '!':
            if (current() == '=') {
                moveNextPos();
                return {TokenType::OP_NEQ, "!=", start_line};
            }
            throw SyntaxError(std::string("неизвестный символ '") + c + "' на строке " + std::to_string(start_line));
        case '<':
            if (current() == '=') {
                moveNextPos();
                return {TokenType::OP_LTE,"<=", start_line};
            }
            return {TokenType::OP_LT, "<", start_line};
        case '>':
            if (current() == '=') {
                moveNextPos();
                return {TokenType::OP_GTE, ">=", start_line};
            }
            return {TokenType::OP_GT, ">", start_line};
        default:
            throw SyntaxError(std::string("неизвестный символ '") + c + "' на строке " + std::to_string(start_line));
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipSpaceAndComments();
        if (current() == '\0') {
            tokens.push_back({TokenType::END_OF_FILE, "", _line});
            break;
        }
        if      (std::isalpha(static_cast<unsigned char>(current())) || current() == '_') tokens.push_back(readIdent());
        else if (std::isdigit(static_cast<unsigned char>(current()))) tokens.push_back(readNumber());
        else if ( current() == '"') tokens.push_back(readString());
        else    tokens.push_back(readOperator());
    }
    return tokens;
}
