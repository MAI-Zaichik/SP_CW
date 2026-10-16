#ifndef CW_EXPR_H
#define CW_EXPR_H
#include <memory>
#include <string>
#include "core/types.h"


// базовая структура для всех узлов выражений
struct ExprNode {
    virtual ~ExprNode() = default;
};

using ExprPtr = std::unique_ptr<ExprNode>;

// Литерал: 67, "Nikitos", NULL
struct LiteralExpr :ExprNode {
    Value value; // из src/core/types.h
};


// ссылка на колонку: age, name, id или др
struct ColumnRefExpr : ExprNode {
    std::string name;
};


// бинарные операции
enum class BinaryOper {
    EQ,  // ==
    NEQ, // !=
    LT,  // <
    GT,  // >
    LTE, // <=
    GTE  // >=
};


// бинарное выражение
struct BinaryExpr : ExprNode {
    ExprPtr left;
    BinaryOper oper;
    ExprPtr right;
};


// промежутки [low, high) левая граница включительна, правая не включительно
struct BetweenExpr : ExprNode {
    ExprPtr value;
    ExprPtr low;
    ExprPtr high;
};


// LIKE: name LIKE "A.*"
struct LikeExpr : ExprNode {
    ExprPtr value;
    std::string pattern;
};


// логические операторы: left AND/OR right
enum class LogicalOper {AND, OR};

// age > 18 AND name LIKE "A.*"
struct LogicalExpr : ExprNode {
    ExprPtr left;
    LogicalOper oper;
    ExprPtr right;
};


#endif //CW_EXPR_H