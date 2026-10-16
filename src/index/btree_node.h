#pragma once
#include <vector>
#include <cstdint>
#include "core/types.h"

static constexpr uint32_t NULL_PAGE = 0xFFFFFFFF;

static constexpr uint16_t BTREE_MAX_KEYS = 100;

static constexpr uint16_t BTREE_MIN_KEYS = BTREE_MAX_KEYS / 2;

struct BTreeNode {
    bool is_leaf = true;
    uint16_t key_count = 0;

    // NULL_PAGE означает нет родителя (узел — корень)
    uint32_t parent_id = NULL_PAGE;

    // key_count ключей, отсортированных по возрастанию
    std::vector<Value> keys;

    // значения для каждого узла
    std::vector<RowId> values;

    // только у internal: key_count + 1 потомков
    std::vector<uint32_t> children;

};