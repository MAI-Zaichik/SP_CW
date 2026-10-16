#pragma once
#include <optional>
#include <vector>
#include <cstdint>
#include "core/types.h"
#include "index/btree_node.h"

// докинуты доп функции и структура. мб фикс, если надо будет
class BTree {
public:
    explicit BTree(const std::string& path);

    ~BTree();

    std::optional<RowId> find(const Value& key);
    std::vector<RowId> scanRange(const Value& lo, const Value& hi);

    void insert(const Value& key, RowId rid);

    bool erase(const Value& key);

    void flush();

    static int compareValues(const Value& a, const Value& b);

private:
    struct NodeStore {
        std::vector<BTreeNode> nodes;
        std::vector<uint32_t>  free_ids;
        uint32_t root_id = NULL_PAGE;

        uint32_t alloc();
        void free(uint32_t id);
        BTreeNode load(uint32_t id) const;
        void save(uint32_t id, const BTreeNode& node);
    };

    NodeStore _store;
    std::string _path;

    // поиск
    uint32_t findLeaf(const Value& key) const;
    uint32_t findNodeWithKey(const Value& key) const;

    // вставка
    void splitNode(uint32_t node_id);

    // удаление 
    void fixUnderflow(uint32_t node_id);

    bool tryBorrowLeft (uint32_t node_id, uint32_t parent_id, uint16_t child_idx);
    bool tryBorrowRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    void mergeWithLeft (uint32_t node_id, uint32_t parent_id, uint16_t child_idx);
    void mergeWithRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    // обход
    void scanRangeHelper(uint32_t node_id, const Value& lo, const Value& hi,
                         std::vector<RowId>& out) const;

    // Индекс child_id в parent.children
    int findChildIndex(const BTreeNode& parent, uint32_t child_id) const;

    // сохранение на диск
    void loadFromDisk();
    void saveToDisk() const;
};