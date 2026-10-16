#ifndef CW_LEXER_H
#define CW_LEXER_H

#include <string>
#include <vector>
#include "parser/token.h"

class Lexer {
public:
    // принимает весь текст запроса
    explicit Lexer(const std::string &input);

    // основной метод для возврата всех токенов сразу
    std::vector<Token> tokenize();

private:
    std::string _input; // исходная строка
    size_t _position; // текущая позиция в строке
    int _line; // текущая строка

    char current() const; // возвращает текущий символ
    void moveNextPos(); // двигаем position на +1;
    void skipSpaceAndComments(); // пропустить пробелы, переносы и комментарии -- до конца строки

    Token readIdent(); // чтение целого слова keyword/ident
    Token readNumber(); // чтение цифры - LIT_INT
    Token readString(); // чтение строки  - LIT_STRING
    Token readOperator(); // чтение операторов(= == != < <= > >=)

};



#endif //CW_LEXER_H