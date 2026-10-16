#include <gtest/gtest.h>
#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "parser/expr.h"
#include "core/exceptions.h"

static ASTNodePtr parse(const std::string& sql) {
    Lexer lexer(sql);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

// парсит и забирает владение узлом, скастовав к конкретному типу
// (возвращаем unique_ptr, а не сырой указатель - иначе узел умрёт сразу после parse)
template <class T>
static std::unique_ptr<T> parseAs(const std::string& sql) {
    ASTNodePtr node = parse(sql);
    T* casted = dynamic_cast<T*>(node.get());
    if (casted) node.release();
    return std::unique_ptr<T>(casted);
}

static int asInt(const Value& v) {
    EXPECT_TRUE(std::holds_alternative<int>(v));
    return std::get<int>(v);
}

static std::string asStr(const Value& v) {
    EXPECT_TRUE(std::holds_alternative<std::string_view>(v));
    return std::string(std::get<std::string_view>(v));
}

// --- DDL уровня БД ---

TEST(Parser, CreateDatabase) {
    auto s = parseAs<CreateDatabaseStmt>("CREATE DATABASE shop;");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name, "shop");
}

TEST(Parser, DropDatabase) {
    auto s = parseAs<DropDatabaseStmt>("DROP DATABASE shop;");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name, "shop");
}

TEST(Parser, Use) {
    auto s = parseAs<UseStmt>("USE shop;");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name, "shop");
}

// --- CREATE TABLE ---

TEST(Parser, CreateTableColumnsAndConstraints) {
    auto s = parseAs<CreateTableStmt>(
        "CREATE TABLE t (id INT INDEXED, name STRING NOT_NULL, note STRING);");
    ASSERT_NE(s, nullptr);
    EXPECT_FALSE(s->table.db.has_value());
    EXPECT_EQ(s->table.table, "t");
    ASSERT_EQ(s->columns.size(), 3u);

    EXPECT_EQ(s->columns[0].name, "id");
    EXPECT_EQ(s->columns[0].type, ColumnType::INT);
    EXPECT_EQ(s->columns[0].constraint, Constraint::INDEXED);

    EXPECT_EQ(s->columns[1].type, ColumnType::STRING);
    EXPECT_EQ(s->columns[1].constraint, Constraint::NOT_NULL);

    EXPECT_EQ(s->columns[2].constraint, Constraint::NONE);
}

TEST(Parser, CreateTableDefaultValues) {
    auto s = parseAs<CreateTableStmt>(
        "CREATE TABLE t (qty INT DEFAULT 5, status STRING DEFAULT \"new\");");
    ASSERT_NE(s, nullptr);
    ASSERT_TRUE(s->columns[0].default_value.has_value());
    EXPECT_EQ(asInt(*s->columns[0].default_value), 5);
    ASSERT_TRUE(s->columns[1].default_value.has_value());
    EXPECT_EQ(asStr(*s->columns[1].default_value), "new");
}

TEST(Parser, CreateTableNoDefaultIsNullopt) {
    auto s = parseAs<CreateTableStmt>("CREATE TABLE t (id INT);");
    ASSERT_NE(s, nullptr);
    EXPECT_FALSE(s->columns[0].default_value.has_value());
}

TEST(Parser, CreateTableDefaultTypeMismatchThrows) {
    EXPECT_THROW(parse("CREATE TABLE t (id INT DEFAULT \"oops\");"), SyntaxError);
}

TEST(Parser, DropTableWithDbNotation) {
    auto s = parseAs<DropTableStmt>("DROP TABLE shop.users;");
    ASSERT_NE(s, nullptr);
    ASSERT_TRUE(s->table.db.has_value());
    EXPECT_EQ(*s->table.db, "shop");
    EXPECT_EQ(s->table.table, "users");
}

// --- INSERT ---

TEST(Parser, InsertMultipleRows) {
    auto s = parseAs<InsertStmt>("INSERT INTO t (a, b) VALUE (1, \"x\"), (2, \"y\");");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->table.table, "t");
    ASSERT_EQ(s->columns.size(), 2u);
    EXPECT_EQ(s->columns[0], "a");
    EXPECT_EQ(s->columns[1], "b");
    ASSERT_EQ(s->rows.size(), 2u);
    EXPECT_EQ(asInt(s->rows[0][0]), 1);
    EXPECT_EQ(asStr(s->rows[0][1]), "x");
    EXPECT_EQ(asInt(s->rows[1][0]), 2);
    EXPECT_EQ(asStr(s->rows[1][1]), "y");
}

TEST(Parser, InsertNegativeNumber) {
    auto s = parseAs<InsertStmt>("INSERT INTO t (a) VALUE (-5);");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(asInt(s->rows[0][0]), -5);
}

TEST(Parser, InsertNull) {
    auto s = parseAs<InsertStmt>("INSERT INTO t (a) VALUE (NULL);");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(s->rows[0][0]));
}

// --- SELECT ---

TEST(Parser, SelectStarHasNoColumns) {
    auto s = parseAs<SelectStmt>("SELECT * FROM t;");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->columns.empty());
}

TEST(Parser, SelectColumnsWithAlias) {
    auto s = parseAs<SelectStmt>("SELECT name AS n, age FROM t;");
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->columns.size(), 2u);
    EXPECT_EQ(s->columns[0].name, "name");
    ASSERT_TRUE(s->columns[0].alias.has_value());
    EXPECT_EQ(*s->columns[0].alias, "n");
    EXPECT_EQ(s->columns[0].agg, AggFunc::NONE);
    EXPECT_EQ(s->columns[1].name, "age");
    EXPECT_FALSE(s->columns[1].alias.has_value());
}

TEST(Parser, SelectAggregates) {
    auto s = parseAs<SelectStmt>("SELECT SUM(price), COUNT(id), AVG(price) FROM t;");
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->columns.size(), 3u);
    EXPECT_EQ(s->columns[0].agg, AggFunc::SUM);
    EXPECT_EQ(s->columns[0].name, "price");
    EXPECT_EQ(s->columns[1].agg, AggFunc::COUNT);
    EXPECT_EQ(s->columns[2].agg, AggFunc::AVG);
}

TEST(Parser, SelectTableRefWithDb) {
    auto s = parseAs<SelectStmt>("SELECT * FROM shop.users;");
    ASSERT_NE(s, nullptr);
    ASSERT_TRUE(s->table.db.has_value());
    EXPECT_EQ(*s->table.db, "shop");
    EXPECT_EQ(s->table.table, "users");
}

// --- WHERE ---

TEST(Parser, WhereEqualityIsBinaryExpr) {
    auto s = parseAs<SelectStmt>("SELECT * FROM t WHERE id == 1;");
    ASSERT_NE(s, nullptr);
    auto* b = dynamic_cast<BinaryExpr*>(s->where.get());
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->oper, BinaryOper::EQ);
}

TEST(Parser, WherePrecedenceOrWeakerThanAnd) {
    // a == 1 OR b == 2 AND c == 3  ->  OR( a==1 , AND(b==2, c==3) )
    auto s = parseAs<SelectStmt>("SELECT * FROM t WHERE a == 1 OR b == 2 AND c == 3;");
    ASSERT_NE(s, nullptr);
    auto* top = dynamic_cast<LogicalExpr*>(s->where.get());
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->oper, LogicalOper::OR);
    auto* right = dynamic_cast<LogicalExpr*>(top->right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->oper, LogicalOper::AND);
}

TEST(Parser, WhereParensOverridePrecedence) {
    // (a==1 OR b==2) AND c==3  ->  AND( OR(...), c==3 )
    auto s = parseAs<SelectStmt>("SELECT * FROM t WHERE (a == 1 OR b == 2) AND c == 3;");
    ASSERT_NE(s, nullptr);
    auto* top = dynamic_cast<LogicalExpr*>(s->where.get());
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->oper, LogicalOper::AND);
    auto* left = dynamic_cast<LogicalExpr*>(top->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->oper, LogicalOper::OR);
}

TEST(Parser, WhereBetween) {
    auto s = parseAs<SelectStmt>("SELECT * FROM t WHERE x BETWEEN 1 AND 10;");
    ASSERT_NE(s, nullptr);
    EXPECT_NE(dynamic_cast<BetweenExpr*>(s->where.get()), nullptr);
}

TEST(Parser, WhereLike) {
    auto s = parseAs<SelectStmt>("SELECT * FROM t WHERE name LIKE \"a.*\";");
    ASSERT_NE(s, nullptr);
    auto* l = dynamic_cast<LikeExpr*>(s->where.get());
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->pattern, "a.*");
}

// --- UPDATE / DELETE ---

TEST(Parser, UpdateAssignments) {
    auto s = parseAs<UpdateStmt>("UPDATE t SET a = 1, b = \"x\" WHERE id == 2;");
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->assignments.size(), 2u);
    EXPECT_EQ(s->assignments[0].first, "a");
    EXPECT_EQ(asInt(s->assignments[0].second), 1);
    EXPECT_EQ(s->assignments[1].first, "b");
    EXPECT_EQ(asStr(s->assignments[1].second), "x");
    EXPECT_NE(s->where, nullptr);
}

TEST(Parser, DeleteWithWhere) {
    auto s = parseAs<DeleteStmt>("DELETE FROM t WHERE id == 1;");
    ASSERT_NE(s, nullptr);
    EXPECT_NE(s->where, nullptr);
}

TEST(Parser, DeleteWithoutWhereIsNull) {
    auto s = parseAs<DeleteStmt>("DELETE FROM t;");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->where, nullptr);
}

// --- ошибки синтаксиса ---

TEST(Parser, MissingSemicolonThrows) {
    EXPECT_THROW(parse("SELECT * FROM t"), SyntaxError);
}

TEST(Parser, SelectWithoutColumnsThrows) {
    EXPECT_THROW(parse("SELECT FROM t;"), SyntaxError);
}

TEST(Parser, TwoColumnsWithoutCommaThrows) {
    EXPECT_THROW(parse("SELECT col1 col2 FROM t;"), SyntaxError);
}

TEST(Parser, CreateTableEmptyThrows) {
    EXPECT_THROW(parse("CREATE TABLE;"), SyntaxError);
}

TEST(Parser, UnknownCommandThrows) {
    EXPECT_THROW(parse("FOO bar;"), SyntaxError);
}

TEST(Parser, WhereWithoutConditionThrows) {
    EXPECT_THROW(parse("SELECT * FROM t WHERE;"), SyntaxError);
}
