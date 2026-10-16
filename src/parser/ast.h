#ifndef CW_AST_H
#define CW_AST_H

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "core/types.h"
#include "parser/expr.h"

struct Visitor;

// базовый класс всех узлов
struct ASTNode {
    virtual void accept(Visitor&) = 0;
    virtual ~ASTNode() = default;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;


// описание колонки в CREATE TABLE
struct ColumnDef {
    std::string name;
    ColumnType type;
    Constraint constraint;
    std::optional<Value> default_value;   // nullopt = нет DEFAULT
};

// агрегатная функция над колонкой в SELECT
enum class AggFunc { NONE, SUM, COUNT, AVG };

// одна колонка в SELECT
// SELECT name, age .... - два SelectColumn
// SELECT *  - columns пустой вектор
// agg != NONE -> это SUM(name)/COUNT(name)/AVG(name)
struct SelectColumn {
    AggFunc agg = AggFunc::NONE;
    std::string name;
    std::optional<std::string> alias;
};


// CREATE DATABASE name;
struct CreateDatabaseStmt : ASTNode {
    std::string name;
    void accept(Visitor& v) override;
};

// DROP DATABASE name;
struct DropDatabaseStmt : ASTNode {
    std::string name;
    void accept(Visitor& v) override;
};

// USE name;
struct UseStmt : ASTNode {
    std::string name;
    void accept(Visitor& v) override;
};

// CREATE TABLE name (col1 TYPE CONSTRAINT, ...)
struct CreateTableStmt : ASTNode {
    TableReference table;
    std::vector<ColumnDef> columns;
    void accept(Visitor& v) override;
};

//DROP TABLE name;
struct DropTableStmt : ASTNode{
    TableReference table;
    void accept(Visitor& v) override;
};

// INSERT INTO table (col1, col2) VALUE (v1, v2), (v3, v4);
struct InsertStmt : ASTNode {
    TableReference table;
    std::vector<std::string> columns;
    std::vector<std::vector<Value>> rows;
    void accept(Visitor &) override;
};

// UPDATE table SET col1 = val1 WHERE condition;
struct  UpdateStmt : ASTNode {
    TableReference table;
    std::vector<std::pair<std::string, Value>> assignments;
    ExprPtr where;
    void accept(Visitor &) override;

};

// DELETE FROM table WHERE condition;
struct DeleteStmt : ASTNode {
    TableReference table; // название таблицы
    ExprPtr where;   //nullptr = нет WHERE
    void accept(Visitor &) override;
};

// SELECT col1, col2 AS alias FROM table WHERE condition
struct SelectStmt : ASTNode {
    TableReference table;
    std::vector<SelectColumn> columns; // пустой = SELECT *
    ExprPtr where;
    void accept(Visitor &) override;
};

// условный интерфейс для всех базовых SQL выражений, нужно реализовать в Executor
struct Visitor {
    virtual void visit(CreateDatabaseStmt&) = 0;
    virtual void visit(DropDatabaseStmt&)   = 0;
    virtual void visit(UseStmt&)            = 0;
    virtual void visit(CreateTableStmt&)    = 0;
    virtual void visit(DropTableStmt&)      = 0;
    virtual void visit(InsertStmt&)         = 0;
    virtual void visit(UpdateStmt&)         = 0;
    virtual void visit(DeleteStmt&)         = 0;
    virtual void visit(SelectStmt&)         = 0;
    virtual ~Visitor() = default;
};



#endif //CW_AST_H