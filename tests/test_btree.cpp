#include <gtest/gtest.h>
#include <filesystem>
#include <cstdio>
#include "index/btree.h"

namespace fs = std::filesystem;

class BTreeTest : public ::testing::Test {
protected:
    fs::path _path;

    void SetUp() override {
        _path = fs::temp_directory_path() / "cwt_btree_test.idx";
        std::error_code ec;
        fs::remove(_path, ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove(_path, ec);
    }
};

TEST_F(BTreeTest, InsertAndFind) {
    BTree tree(_path.string());

    for (int i = 0; i < 1000; ++i) {
        tree.insert(Value{i}, RowId{static_cast<uint32_t>(i), 0});
    }

    for (int i = 0; i < 1000; ++i) {
        auto r = tree.find(Value{i});
        ASSERT_TRUE(r.has_value()) << "key " << i;
        EXPECT_EQ(r->page_id, static_cast<uint32_t>(i));
    }
}

TEST_F(BTreeTest, DuplicateThrows) {
    BTree tree(_path.string());
    tree.insert(Value{42}, RowId{1, 0});
    EXPECT_THROW(tree.insert(Value{42}, RowId{2, 0}), std::runtime_error);
}

TEST_F(BTreeTest, FindMissingReturnsNullopt) {
    BTree tree(_path.string());
    tree.insert(Value{1}, RowId{1, 0});
    tree.insert(Value{3}, RowId{3, 0});

    EXPECT_FALSE(tree.find(Value{2}).has_value());
    EXPECT_FALSE(tree.find(Value{100}).has_value());
}

TEST_F(BTreeTest, ScanRange) {
    BTree tree(_path.string());
    for (int i = 0; i < 100; ++i) {
        tree.insert(Value{i}, RowId{static_cast<uint32_t>(i), 0});
    }

    // [10, 20) — ожидаем 10 элементов
    auto r = tree.scanRange(Value{10}, Value{20});
    ASSERT_EQ(r.size(), 10u);
    for (uint32_t i = 0; i < 10; ++i) {
        EXPECT_EQ(r[i].page_id, 10u + i);
    }
}

TEST_F(BTreeTest, Erase) {
    BTree tree(_path.string());
    for (int i = 0; i < 100; ++i) {
        tree.insert(Value{i}, RowId{static_cast<uint32_t>(i), 0});
    }

    // Удаляем чётные
    for (int i = 0; i < 100; i += 2) {
        EXPECT_TRUE(tree.erase(Value{i}));
    }

    // Чётные не найдены
    for (int i = 0; i < 100; i += 2) {
        EXPECT_FALSE(tree.find(Value{i}).has_value());
    }
    // Нечётные на месте
    for (int i = 1; i < 100; i += 2) {
        EXPECT_TRUE(tree.find(Value{i}).has_value());
    }
}

TEST_F(BTreeTest, EraseMissingReturnsFalse) {
    BTree tree(_path.string());
    tree.insert(Value{1}, RowId{1, 0});
    EXPECT_FALSE(tree.erase(Value{999}));
}

TEST_F(BTreeTest, StringKeys) {
    BTree tree(_path.string());

    auto intern = [](const std::string& s) {
        return StringPool::instance().intern(s);
    };

    tree.insert(intern("apple"),  RowId{1, 0});
    tree.insert(intern("banana"), RowId{2, 0});
    tree.insert(intern("cherry"), RowId{3, 0});

    EXPECT_TRUE(tree.find(intern("banana")).has_value());
    EXPECT_FALSE(tree.find(intern("date")).has_value());

    // Порядок лексикографический
    auto r = tree.scanRange(intern("a"), intern("c"));
    ASSERT_EQ(r.size(), 2u);  // apple, banana
}

TEST_F(BTreeTest, Persistence) {
    // Записываем
    {
        BTree tree(_path.string());
        for (int i = 0; i < 500; ++i) {
            tree.insert(Value{i}, RowId{static_cast<uint32_t>(i), 0});
        }
        tree.flush();
    }

    // Загружаем
    {
        BTree tree(_path.string());
        for (int i = 0; i < 500; ++i) {
            auto r = tree.find(Value{i});
            ASSERT_TRUE(r.has_value()) << "after reload: " << i;
            EXPECT_EQ(r->page_id, static_cast<uint32_t>(i));
        }
    }
}

TEST_F(BTreeTest, SplitAndMerge) {
    BTree tree(_path.string());

    for (int i = 0; i < 2000; ++i) {
        tree.insert(Value{i}, RowId{static_cast<uint32_t>(i), 0});
    }

    for (int i = 0; i < 2000; ++i) {
        ASSERT_TRUE(tree.erase(Value{i})) << "erase " << i;
    }

    for (int i = 0; i < 2000; ++i) {
        EXPECT_FALSE(tree.find(Value{i}).has_value());
    }
}