#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <string>

#include "parser/lexer.h"
#include "parser/parser.h"
#include "execution/executor.h"
#include "catalog/catalog.h"
#include "storage/storage.h"
#include "storage/pager/pager.h"
#include "core/result.h"

namespace fs = std::filesystem;

class ExecutorTest : public ::testing::Test {
protected:
    fs::path _tmp;
    fs::path _old_cwd;
    std::unique_ptr<Catalog> _catalog;
    std::unique_ptr<Storage> _storage;
    std::unique_ptr<Executor> _exec;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        _old_cwd = fs::current_path();
        _tmp = fs::temp_directory_path() /
               (std::string("cwt_") + info->test_suite_name() + "_" + info->name());
        std::error_code ec;
        fs::remove_all(_tmp, ec);
        fs::create_directories(_tmp);
        fs::current_path(_tmp);
        fs::create_directories("data");

        _catalog = std::make_unique<Catalog>("catalog.json");
        _storage = std::make_unique<Storage>();
        _exec    = std::make_unique<Executor>(*_catalog, *_storage);
    }

    void TearDown() override {
        _exec.reset();
        _storage.reset();
        _catalog.reset();
        fs::current_path(_old_cwd);
        std::error_code ec;
        fs::remove_all(_tmp, ec);
    }

    ExecuteResult run(const std::string& sql) {
        Lexer lexer(sql);
        Parser parser(lexer.tokenize());
        ASTNodePtr node = parser.parse();
        return _exec->execute(*node);
    }

    // users(id INDEXED, name NOT_NULL, age) с тремя строками
    void seedUsers() {
        ASSERT_TRUE(run("CREATE DATABASE db1;").ok);
        ASSERT_TRUE(run("USE db1;").ok);
        ASSERT_TRUE(run("CREATE TABLE users (id INT INDEXED, name STRING NOT_NULL, age INT);").ok);
        ASSERT_TRUE(run("INSERT INTO users (id, name, age) VALUE "
                        "(1, \"Alice\", 100), (2, \"Bob\", 200), (3, \"Carol\", 300);").ok);
    }
};

TEST_F(ExecutorTest, SelectAllReturnsAllRows) {
    seedUsers();
    auto r = run("SELECT * FROM users;");
    ASSERT_TRUE(r.ok) << r.message;
    ASSERT_TRUE(r.data.is_array());
    EXPECT_EQ(r.data.size(), 3u);

    nlohmann::json expected = nlohmann::json::array({
        {{"id", 1}, {"name", "Alice"}, {"age", 100}},
        {{"id", 2}, {"name", "Bob"},   {"age", 200}},
        {{"id", 3}, {"name", "Carol"}, {"age", 300}},
    });
    EXPECT_EQ(r.data, expected);
}

TEST_F(ExecutorTest, SelectWhereEqualityOnIndex) {
    seedUsers();
    auto r = run("SELECT name FROM users WHERE id == 2;");
    ASSERT_TRUE(r.ok) << r.message;
    ASSERT_EQ(r.data.size(), 1u);
    EXPECT_EQ(r.data[0]["name"], "Bob");
}

TEST_F(ExecutorTest, SelectProjectionWithAlias) {
    seedUsers();
    auto r = run("SELECT name AS who FROM users WHERE id == 1;");
    ASSERT_TRUE(r.ok) << r.message;
    ASSERT_EQ(r.data.size(), 1u);
    EXPECT_EQ(r.data[0]["who"], "Alice");
}

TEST_F(ExecutorTest, Aggregates) {
    seedUsers();
    auto r = run("SELECT SUM(age), COUNT(id), AVG(age) FROM users;");
    ASSERT_TRUE(r.ok) << r.message;
    ASSERT_EQ(r.data.size(), 1u);
    EXPECT_EQ(r.data[0]["SUM(age)"], 600);
    EXPECT_EQ(r.data[0]["COUNT(id)"], 3);
    EXPECT_DOUBLE_EQ(r.data[0]["AVG(age)"].get<double>(), 200.0);
}

TEST_F(ExecutorTest, CountIgnoresNull) {
    ASSERT_TRUE(run("CREATE DATABASE db1;").ok);
    ASSERT_TRUE(run("USE db1;").ok);
    ASSERT_TRUE(run("CREATE TABLE t (id INT INDEXED, val INT);").ok);
    ASSERT_TRUE(run("INSERT INTO t (id, val) VALUE (1, 10), (2, NULL), (3, 30);").ok);
    auto r = run("SELECT COUNT(val) FROM t;");
    ASSERT_TRUE(r.ok) << r.message;
    EXPECT_EQ(r.data[0]["COUNT(val)"], 2);
}

TEST_F(ExecutorTest, DefaultValueAppliedWhenColumnOmitted) {
    ASSERT_TRUE(run("CREATE DATABASE db1;").ok);
    ASSERT_TRUE(run("USE db1;").ok);
    ASSERT_TRUE(run("CREATE TABLE orders (id INT INDEXED, qty INT DEFAULT 1, status STRING DEFAULT \"new\");").ok);
    ASSERT_TRUE(run("INSERT INTO orders (id) VALUE (1);").ok);
    auto r = run("SELECT * FROM orders WHERE id == 1;");
    ASSERT_TRUE(r.ok) << r.message;
    ASSERT_EQ(r.data.size(), 1u);
    EXPECT_EQ(r.data[0]["qty"], 1);
    EXPECT_EQ(r.data[0]["status"], "new");
}

TEST_F(ExecutorTest, UpdateChangesRow) {
    seedUsers();
    ASSERT_TRUE(run("UPDATE users SET name = \"Robert\" WHERE id == 2;").ok);
    auto r = run("SELECT name FROM users WHERE id == 2;");
    ASSERT_TRUE(r.ok) << r.message;
    EXPECT_EQ(r.data[0]["name"], "Robert");
}

TEST_F(ExecutorTest, DeleteRemovesRow) {
    seedUsers();
    ASSERT_TRUE(run("DELETE FROM users WHERE id == 1;").ok);
    auto r = run("SELECT * FROM users;");
    ASSERT_TRUE(r.ok) << r.message;
    EXPECT_EQ(r.data.size(), 2u);
}

// --- ошибки: execute не бросает наружу, возвращает ok=false ---

TEST_F(ExecutorTest, NoDatabaseSelectedFails) {
    auto r = run("SELECT * FROM users;");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, DuplicateIndexedValueFails) {
    seedUsers();
    auto r = run("INSERT INTO users (id, name, age) VALUE (1, \"Dup\", 1);");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, TypeMismatchFails) {
    seedUsers();
    auto r = run("INSERT INTO users (id, name, age) VALUE (\"notint\", \"x\", 1);");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, NotNullViolationFails) {
    seedUsers();
    auto r = run("INSERT INTO users (id, name, age) VALUE (9, NULL, 1);");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, UnknownColumnInSelectFails) {
    seedUsers();
    auto r = run("SELECT nope FROM users;");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, MixAggregateAndPlainFails) {
    seedUsers();
    auto r = run("SELECT name, SUM(age) FROM users;");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, SumOnStringColumnFails) {
    seedUsers();
    auto r = run("SELECT SUM(name) FROM users;");
    EXPECT_FALSE(r.ok);
}

TEST_F(ExecutorTest, DbTableNotationWithoutUse) {
    // текущая БД - db1, но к таблице из other обращаемся через db.table
    ASSERT_TRUE(run("CREATE DATABASE db1;").ok);
    ASSERT_TRUE(run("USE db1;").ok);
    ASSERT_TRUE(run("CREATE DATABASE other;").ok);
    ASSERT_TRUE(run("CREATE TABLE other.flags (id INT INDEXED, label STRING);").ok);
    ASSERT_TRUE(run("INSERT INTO other.flags (id, label) VALUE (1, \"hi\");").ok);
    auto r = run("SELECT label FROM other.flags WHERE id == 1;");
    ASSERT_TRUE(r.ok) << r.message;
    EXPECT_EQ(r.data[0]["label"], "hi");
}
