#ifndef CW_PARSER_H
#define CW_PARSER_H
#include <vector>
#include "parser/ast.h"
#include "parser/token.h"


class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ASTNodePtr parse();
private:
    std::vector<Token> _vector;
    size_t _position = 0;
    // ограничение глубины вложенности выражений
    int _depth = 0;
    static constexpr int MAX_EXPR_DEPTH = 200;


    Token &current(); // возвращает текущий токен, без сдвига
    Token &peek(size_t offset = 1); // возвращает токен с определенным смещением, не двигаем позицию
    Token consume(TokenType expected, const char* what = nullptr); // what - человекочитаемое описание для ошибки
    bool match(TokenType type); // менее строгая проверка токена для необязательных частей, со сдвигом

    // общие обработчики, распределяют по уровням выражений
    ASTNodePtr parseStatement();
    ASTNodePtr parseCreate();
    ASTNodePtr parseDrop();

    ASTNodePtr parseCreateDatabase();
    ASTNodePtr parseDropDatabase();

    ASTNodePtr parseCreateTable();
    ASTNodePtr parseDropTable();

    ASTNodePtr parseUse();

    ASTNodePtr parseUpdate();
    ASTNodePtr parseInsert();
    ASTNodePtr parseDelete();
    ASTNodePtr parseSelect();

    bool check(TokenType type); // используется внутри match/consume для проверки типа токена
    TableReference parseTableRef(); // парсит название таблицы, т.к. таблица может быть передана 2 способами
    ColumnDef parseColumnDef(); // парсит колонку
    Value parseValue(); // парсит значение

    int parseIntFromToken(const Token& t, bool negate); // общий разбор LIT_INT с опциональным минусом

    // парсят операции по приоритетам: OR < AND < comparison < primary
    ExprPtr parseExpr();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseComparison();
    ExprPtr parsePrimary();
};


#endif //CW_PARSER_H