#include "index/btree.h"
#include "core/string_pool.h"
#include <stdexcept>
#include <fstream>

// NodeStore
uint32_t BTree::NodeStore::alloc() {
    if (!free_ids.empty()) {
        uint32_t id = free_ids.back();
        free_ids.pop_back();
        nodes[id] = BTreeNode{};
        return id;
    }
    nodes.push_back(BTreeNode{});
    return static_cast<uint32_t>(nodes.size() - 1);
}

void BTree::NodeStore::free(uint32_t id) {
    if (id >= nodes.size()) return;
    nodes[id] = BTreeNode{};
    free_ids.push_back(id);
}

BTreeNode BTree::NodeStore::load(uint32_t id) const {
    if (id >= nodes.size())
        throw std::runtime_error("BTree: node id out of range");
    return nodes[id];
}

void BTree::NodeStore::save(uint32_t id, const BTreeNode& node) {
    if (id >= nodes.size())
        throw std::runtime_error("BTree: save to invalid id");
    nodes[id] = node;
}

// Конструктор + деструктор
BTree::BTree(const std::string& path) : _path(path) {
    loadFromDisk();

    if (_store.root_id == NULL_PAGE) {
        uint32_t root_id = _store.alloc();
        BTreeNode root;
        root.is_leaf   = true;
        root.parent_id = NULL_PAGE;
        _store.save(root_id, root);
        _store.root_id = root_id;
    }
}

BTree::~BTree() {
    try { flush(); } catch (...) {}
}

void BTree::flush() {
    saveToDisk();
}

// Сравнение
int BTree::compareValues(const Value& a, const Value& b) {
    // Оба int
    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
        int ia = std::get<int>(a);
        int ib = std::get<int>(b);
        return (ia < ib) ? -1 : (ia > ib) ? 1 : 0;
    }
    // Оба string
    if (std::holds_alternative<std::string_view>(a) &&
        std::holds_alternative<std::string_view>(b)) {
        auto sa = std::get<std::string_view>(a);
        auto sb = std::get<std::string_view>(b);
        if (sa < sb) return -1;
        if (sa > sb) return  1;
        return 0;
    }
    // Оба NULL
    if (std::holds_alternative<std::monostate>(a) &&
        std::holds_alternative<std::monostate>(b)) {
        return 0;
    }
    // Разные типы
    auto rank = [](const Value& v) -> int {
        if (std::holds_alternative<std::monostate>(v)) return 0;
        if (std::holds_alternative<int>(v))            return 1;
        return 2;
    };
    return rank(a) - rank(b);
}

// Поиск
// возвращает id листа, куда должен попасть key
uint32_t BTree::findLeaf(const Value& key) const {
    uint32_t cur = _store.root_id;
    if (cur == NULL_PAGE) return NULL_PAGE;

    while (true) {
        BTreeNode node = _store.load(cur);
        if (node.is_leaf) return cur;

        // ищем первого потомка, где keys[i] > key
        uint16_t i = 0;
        while (i < node.key_count && compareValues(key, node.keys[i]) >= 0) {
            ++i;
        }
        cur = node.children[i];
    }
}

// возвращает id узла, где лежит key
uint32_t BTree::findNodeWithKey(const Value& key) const {
    uint32_t cur = _store.root_id;
    if (cur == NULL_PAGE) return NULL_PAGE;

    while (cur != NULL_PAGE) {
        BTreeNode node = _store.load(cur);

        uint16_t i = 0;
        while (i < node.key_count && compareValues(key, node.keys[i]) > 0) {
            ++i;
        }

        if (i < node.key_count && compareValues(key, node.keys[i]) == 0) {
            return cur;
        }

        if (node.is_leaf) return NULL_PAGE;

        cur = node.children[i];
    }
    return NULL_PAGE;
}

std::optional<RowId> BTree::find(const Value& key) {
    uint32_t node_id = findNodeWithKey(key);
    if (node_id == NULL_PAGE) return std::nullopt;

    BTreeNode node = _store.load(node_id);
    for (uint16_t i = 0; i < node.key_count; ++i) {
        if (compareValues(key, node.keys[i]) == 0)
            return node.values[i];
    }
    return std::nullopt;
}

//  Вставка
void BTree::insert(const Value& key, RowId rid) {
    if (find(key).has_value())
        throw std::runtime_error("BTree::insert: duplicate key");

    uint32_t leaf_id = findLeaf(key);
    BTreeNode leaf = _store.load(leaf_id);

    // ищем позицию вставки
    uint16_t pos = 0;
    while (pos < leaf.key_count && compareValues(key, leaf.keys[pos]) > 0) {
        ++pos;
    }

    // вставляем
    leaf.keys.insert(leaf.keys.begin() + pos, key);
    leaf.values.insert(leaf.values.begin() + pos, rid);
    ++leaf.key_count;
    _store.save(leaf_id, leaf);

    // Если переполнен — split
    if (leaf.key_count > BTREE_MAX_KEYS) {
        splitNode(leaf_id);
    }
}

// Split
void BTree::splitNode(uint32_t node_id) {
    BTreeNode node = _store.load(node_id);

    uint16_t mid = node.key_count / 2;
    Value median_key = node.keys[mid];
    RowId median_val = node.values[mid];

    // правый узел - ключи после медианы
    uint32_t right_id = _store.alloc();
    BTreeNode right;
    right.is_leaf   = node.is_leaf;
    right.parent_id = node.parent_id;

    right.keys.assign(node.keys.begin() + mid + 1, node.keys.end());
    right.values.assign(node.values.begin() + mid + 1, node.values.end());
    right.key_count = static_cast<uint16_t>(right.keys.size());

    if (!node.is_leaf) {
        right.children.assign(node.children.begin() + mid + 1, node.children.end());
        // обновляем parent_id у детей
        for (uint32_t ch : right.children) {
            BTreeNode child = _store.load(ch);
            child.parent_id = right_id;
            _store.save(ch, child);
        }
    }

    // левый узел: ключи до медианы
    node.keys.resize(mid);
    node.values.resize(mid);
    node.key_count = mid;
    if (!node.is_leaf) {
        node.children.resize(mid + 1);
    }

    _store.save(node_id, node);
    _store.save(right_id, right);

    // медиана уходит в родителя
    if (node.parent_id == NULL_PAGE) {
        // новый корень
        uint32_t root_id = _store.alloc();
        BTreeNode root;
        root.is_leaf   = false;
        root.parent_id = NULL_PAGE;
        root.keys      = { median_key };
        root.values    = { median_val };
        root.key_count = 1;
        root.children  = { node_id, right_id };

        node.parent_id  = root_id;
        right.parent_id = root_id;

        _store.save(node_id, node);
        _store.save(right_id, right);
        _store.save(root_id, root);
        _store.root_id = root_id;
        return;
    }

    BTreeNode parent = _store.load(node.parent_id);

    // Позиция node_id в children родителя
    uint16_t pos = 0;
    while (pos < parent.children.size() && parent.children[pos] != node_id) {
        ++pos;
    }

    parent.keys.insert(parent.keys.begin() + pos, median_key);
    parent.values.insert(parent.values.begin() + pos, median_val);
    parent.children.insert(parent.children.begin() + pos + 1, right_id);
    ++parent.key_count;
    _store.save(node.parent_id, parent);

    // рекурсивно: если родитель переполнен
    if (parent.key_count > BTREE_MAX_KEYS) {
        splitNode(node.parent_id);
    }
}

// Удаление
bool BTree::erase(const Value& key) {
    if (_store.root_id == NULL_PAGE) return false;

    uint32_t node_id = findNodeWithKey(key);
    if (node_id == NULL_PAGE) return false;

    BTreeNode node = _store.load(node_id);

    // позиция ключа в узле
    uint16_t pos = 0;
    while (pos < node.key_count && compareValues(key, node.keys[pos]) != 0) {
        ++pos;
    }

    if (node.is_leaf) {
        // просто удаляем
        node.keys.erase(node.keys.begin() + pos);
        node.values.erase(node.values.begin() + pos);
        --node.key_count;
        _store.save(node_id, node);
    } else {
        // ключ в internal — заменяем на predecessor
        uint32_t pred_id = node.children[pos];  // левое поддерево
        BTreeNode pred = _store.load(pred_id);

        // идем в самый правый лист
        while (!pred.is_leaf) {
            pred_id = pred.children[pred.key_count];
            pred = _store.load(pred_id);
        }

        Value pred_key = pred.keys.back();
        RowId pred_val = pred.values.back();

        // заменяем в internal
        node.keys[pos] = pred_key;
        node.values[pos] = pred_val;
        _store.save(node_id, node);

        // удаляем из листа
        pred.keys.pop_back();
        pred.values.pop_back();
        --pred.key_count;
        _store.save(pred_id, pred);

        node_id = pred_id;
        node = pred;
    }

    // underflow: key_count < MIN_KEYS
    if (node_id != _store.root_id && node.key_count < BTREE_MIN_KEYS) {
        fixUnderflow(node_id);
    }

    // если корень пуст и не лист — его единственный потомок становится корнем
    BTreeNode root = _store.load(_store.root_id);
    if (!root.is_leaf && root.key_count == 0) {
        uint32_t new_root_id = root.children[0];
        BTreeNode new_root = _store.load(new_root_id);
        new_root.parent_id = NULL_PAGE;
        _store.save(new_root_id, new_root);
        _store.free(_store.root_id);
        _store.root_id = new_root_id;
    }

    return true;
}

// устраняем underflow
void BTree::fixUnderflow(uint32_t node_id) {
    BTreeNode node = _store.load(node_id);
    if (node.parent_id == NULL_PAGE) return;

    BTreeNode parent = _store.load(node.parent_id);
    int child_idx = findChildIndex(parent, node_id);
    uint16_t cidx = static_cast<uint16_t>(child_idx);

    // пытаемся занять у соседей
    if (cidx > 0 && tryBorrowLeft(node_id, node.parent_id, cidx)) return;
    if (cidx < parent.key_count && tryBorrowRight(node_id, node.parent_id, cidx)) return;

    // иначе merge
    if (cidx > 0) {
        mergeWithLeft(node_id, node.parent_id, cidx);
    } else {
        mergeWithRight(node_id, node.parent_id, cidx);
    }
}

bool BTree::tryBorrowLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent  = _store.load(parent_id);
    uint32_t  left_id = parent.children[child_idx - 1];
    BTreeNode left    = _store.load(left_id);

    if (left.key_count <= BTREE_MIN_KEYS) return false;

    BTreeNode node = _store.load(node_id);

    if (node.is_leaf) {
        // Переносим последний ключ left в начало node
        node.keys.insert(node.keys.begin(), left.keys.back());
        node.values.insert(node.values.begin(), left.values.back());
        ++node.key_count;

        left.keys.pop_back();
        left.values.pop_back();
        --left.key_count;

        parent.keys[child_idx - 1] = node.keys[0];
    } else {
        // разделитель родителя идет вниз, крайний правый ключ left идет вверх
        node.keys.insert(node.keys.begin(), parent.keys[child_idx - 1]);
        node.values.insert(node.values.begin(), parent.values[child_idx - 1]);
        ++node.key_count;

        uint32_t moved_child = left.children.back();
        node.children.insert(node.children.begin(), moved_child);
        left.children.pop_back();

        BTreeNode moved = _store.load(moved_child);
        moved.parent_id = node_id;
        _store.save(moved_child, moved);

        parent.keys[child_idx - 1]   = left.keys.back();
        parent.values[child_idx - 1] = left.values.back();
        left.keys.pop_back();
        left.values.pop_back();
        --left.key_count;
    }

    _store.save(left_id, left);
    _store.save(node_id, node);
    _store.save(parent_id, parent);
    return true;
}

bool BTree::tryBorrowRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent   = _store.load(parent_id);
    uint32_t  right_id = parent.children[child_idx + 1];
    BTreeNode right    = _store.load(right_id);

    if (right.key_count <= BTREE_MIN_KEYS) return false;

    BTreeNode node = _store.load(node_id);

    if (node.is_leaf) {
        node.keys.push_back(right.keys.front());
        node.values.push_back(right.values.front());
        ++node.key_count;

        right.keys.erase(right.keys.begin());
        right.values.erase(right.values.begin());
        --right.key_count;

        parent.keys[child_idx] = right.keys.front();
    }
    else {
        node.keys.push_back(parent.keys[child_idx]);
        node.values.push_back(parent.values[child_idx]);
        ++node.key_count;

        uint32_t moved_child = right.children.front();
        node.children.push_back(moved_child);
        right.children.erase(right.children.begin());

        BTreeNode moved = _store.load(moved_child);
        moved.parent_id = node_id;
        _store.save(moved_child, moved);

        parent.keys[child_idx]   = right.keys.front();
        parent.values[child_idx] = right.values.front();
        right.keys.erase(right.keys.begin());
        right.values.erase(right.values.begin());
        --right.key_count;
    }

    _store.save(right_id, right);
    _store.save(node_id, node);
    _store.save(parent_id, parent);
    return true;
}

void BTree::mergeWithLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent  = _store.load(parent_id);
    uint32_t  left_id = parent.children[child_idx - 1];
    BTreeNode left    = _store.load(left_id);
    BTreeNode node    = _store.load(node_id);

    if (node.is_leaf) {
        for (uint16_t i = 0; i < node.key_count; ++i) {
            left.keys.push_back(node.keys[i]);
            left.values.push_back(node.values[i]);
        }
        left.key_count += node.key_count;
    } else {
        // спускаем разделитель
        left.keys.push_back(parent.keys[child_idx - 1]);
        left.values.push_back(parent.values[child_idx - 1]);
        ++left.key_count;

        for (uint16_t i = 0; i < node.key_count; ++i) {
            left.keys.push_back(node.keys[i]);
            left.values.push_back(node.values[i]);
        }
        for (uint16_t i = 0; i <= node.key_count; ++i) {
            left.children.push_back(node.children[i]);
            BTreeNode ch = _store.load(node.children[i]);
            ch.parent_id = left_id;
            _store.save(node.children[i], ch);
        }
        left.key_count += node.key_count;
    }

    // убираем из родителя
    parent.keys.erase(parent.keys.begin() + (child_idx - 1));
    parent.values.erase(parent.values.begin() + (child_idx - 1));
    parent.children.erase(parent.children.begin() + child_idx);
    --parent.key_count;

    _store.save(left_id, left);
    _store.save(parent_id, parent);
    _store.free(node_id);

    if (parent.parent_id == NULL_PAGE) {
        if (!parent.is_leaf && parent.key_count == 0) {
            BTreeNode new_root = _store.load(left_id);
            new_root.parent_id = NULL_PAGE;
            _store.save(left_id, new_root);
            _store.root_id = left_id;
            _store.free(parent_id);
        }
        return;
    }

    if (parent.key_count < BTREE_MIN_KEYS) {
        fixUnderflow(parent_id);
    }
}

void BTree::mergeWithRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent   = _store.load(parent_id);
    uint32_t  right_id = parent.children[child_idx + 1];
    BTreeNode right    = _store.load(right_id);
    BTreeNode node     = _store.load(node_id);

    if (node.is_leaf) {
        for (uint16_t i = 0; i < right.key_count; ++i) {
            node.keys.push_back(right.keys[i]);
            node.values.push_back(right.values[i]);
        }
        node.key_count += right.key_count;
    } else {
        node.keys.push_back(parent.keys[child_idx]);
        node.values.push_back(parent.values[child_idx]);
        ++node.key_count;

        for (uint16_t i = 0; i < right.key_count; ++i) {
            node.keys.push_back(right.keys[i]);
            node.values.push_back(right.values[i]);
        }
        for (uint16_t i = 0; i <= right.key_count; ++i) {
            node.children.push_back(right.children[i]);
            BTreeNode ch = _store.load(right.children[i]);
            ch.parent_id = node_id;
            _store.save(right.children[i], ch);
        }
        node.key_count += right.key_count;
    }

    parent.keys.erase(parent.keys.begin() + child_idx);
    parent.values.erase(parent.values.begin() + child_idx);
    parent.children.erase(parent.children.begin() + child_idx + 1);
    --parent.key_count;

    _store.save(node_id, node);
    _store.save(parent_id, parent);
    _store.free(right_id);

    if (parent.parent_id == NULL_PAGE) {
        if (!parent.is_leaf && parent.key_count == 0) {
            BTreeNode new_root = _store.load(node_id);
            new_root.parent_id = NULL_PAGE;
            _store.save(node_id, new_root);
            _store.root_id = node_id;
            _store.free(parent_id);
        }
        return;
    }

    if (parent.key_count < BTREE_MIN_KEYS) {
        fixUnderflow(parent_id);
    }
}

// диапазонный поиск (обход)
void BTree::scanRangeHelper(uint32_t node_id, const Value& lo, const Value& hi,
                            std::vector<RowId>& out) const {
    if (node_id == NULL_PAGE) return;

    BTreeNode node = _store.load(node_id);

    for (uint16_t i = 0; i < node.key_count; ++i) {
        // сначала левое поддерево
        if (!node.is_leaf) {
            scanRangeHelper(node.children[i], lo, hi, out);
        }

        int c_lo = compareValues(node.keys[i], lo);
        int c_hi = compareValues(node.keys[i], hi);

        // если ключ в [lo, hi) — добавляем
        if (c_lo >= 0 && c_hi < 0) {
            out.push_back(node.values[i]);
        }

        // если ключ >= hi — дальше нет смысла
        if (c_hi >= 0) return;
    }

    if (!node.is_leaf) {
        scanRangeHelper(node.children[node.key_count], lo, hi, out);
    }
}

std::vector<RowId> BTree::scanRange(const Value& lo, const Value& hi) {
    std::vector<RowId> out;
    scanRangeHelper(_store.root_id, lo, hi, out);
    return out;
}

int BTree::findChildIndex(const BTreeNode& parent, uint32_t child_id) const {
    for (int i = 0; i <= static_cast<int>(parent.key_count); ++i) {
        if (parent.children[i] == child_id) return i;
    }
    throw std::runtime_error("BTree::findChildIndex: child not found");
}


//  Сохранение на диск
/*  
    Формат файла:
    [root_id: 4 байта]
    [node_count: 4 байта]
    Для каждого узла:
    [id: 4 байта]
    [is_leaf: 1 байт]
    [key_count: 2 байта]
    [parent_id: 4 байта]
    [keys: key_count значений]
    [values: key_count RowId]
    [children: key_count+1 id, если не лист]
*/

namespace {

// записывает одно value в файл
void writeValue(std::ofstream& f, const Value& v) {
    if (std::holds_alternative<std::monostate>(v)) {
        uint8_t tag = 0;
        f.write(reinterpret_cast<const char*>(&tag), 1);
    } else if (std::holds_alternative<int>(v)) {
        uint8_t tag = 1;
        f.write(reinterpret_cast<const char*>(&tag), 1);
        int32_t x = static_cast<int32_t>(std::get<int>(v));
        f.write(reinterpret_cast<const char*>(&x), 4);
    } else {
        uint8_t tag = 2;
        f.write(reinterpret_cast<const char*>(&tag), 1);
        auto sv = std::get<std::string_view>(v);
        uint32_t len = static_cast<uint32_t>(sv.size());
        f.write(reinterpret_cast<const char*>(&len), 4);
        f.write(sv.data(), static_cast<std::streamsize>(len));
    }
}

// читает одно value из файла
Value readValue(std::ifstream& f) {
    uint8_t tag = 0;
    if (!f.read(reinterpret_cast<char*>(&tag), 1))
        throw std::runtime_error("BTree::readValue: EOF reading tag");

    if (tag == 0) {
        return std::monostate{};
    }
    if (tag == 1) {
        int32_t x = 0;
        if (!f.read(reinterpret_cast<char*>(&x), 4))
            throw std::runtime_error("BTree::readValue: EOF reading int");
        return static_cast<int>(x);
    }
    if (tag == 2) {
        uint32_t len = 0;
        if (!f.read(reinterpret_cast<char*>(&len), 4))
            throw std::runtime_error("BTree::readValue: EOF reading string length");
        std::string s(len, '\0');
        if (!f.read(s.data(), static_cast<std::streamsize>(len)))
            throw std::runtime_error("BTree::readValue: EOF reading string data");
        
        return StringPool::instance().intern(std::move(s));
    }

    throw std::runtime_error("BTree::readValue: unknown tag " + std::to_string(tag));
}

}

// cохраняем дерево в файл
void BTree::saveToDisk() const {
    std::ofstream f(_path, std::ios::binary | std::ios::trunc);
    if (!f)
        throw std::runtime_error("BTree::saveToDisk: cannot open " + _path);

    f.write(reinterpret_cast<const char*>(&_store.root_id), 4);
    uint32_t count = static_cast<uint32_t>(_store.nodes.size());
    f.write(reinterpret_cast<const char*>(&count), 4);

    for (uint32_t id = 0; id < count; ++id) {
        const BTreeNode& node = _store.nodes[id];

        // пропускаем освобожденные узлы кроме корня
        if (id != _store.root_id && node.key_count == 0 && node.children.empty()) {
            continue;
        }

        f.write(reinterpret_cast<const char*>(&id), 4);

        uint8_t is_leaf = node.is_leaf ? 1 : 0;
        f.write(reinterpret_cast<const char*>(&is_leaf), 1);
        f.write(reinterpret_cast<const char*>(&node.key_count), 2);
        f.write(reinterpret_cast<const char*>(&node.parent_id), 4);

        for (uint16_t i = 0; i < node.key_count; ++i) {
            writeValue(f, node.keys[i]);
        }

        for (uint16_t i = 0; i < node.key_count; ++i) {
            f.write(reinterpret_cast<const char*>(&node.values[i].page_id), 4);
            f.write(reinterpret_cast<const char*>(&node.values[i].slot_id), 2);
        }

        if (!node.is_leaf) {
            for (uint32_t ch : node.children) {
                f.write(reinterpret_cast<const char*>(&ch), 4);
            }
        }
    }

    if (!f)
        throw std::runtime_error("BTree::saveToDisk: write failed");

    f.flush();
}

// загружаем дерево из файла
void BTree::loadFromDisk() {
    std::ifstream f(_path, std::ios::binary);
    if (!f) return;  // файла нет

    if (!f.read(reinterpret_cast<char*>(&_store.root_id), 4)) return;
    uint32_t count = 0;
    if (!f.read(reinterpret_cast<char*>(&count), 4)) return;

    // выделяем место
    _store.nodes.assign(count, BTreeNode{});
    _store.free_ids.clear();

    // читаем
    uint32_t loaded = 0;
    while (loaded < count) {
        uint32_t id = 0;
        if (!f.read(reinterpret_cast<char*>(&id), 4)) break;
        if (id >= count)
            throw std::runtime_error("BTree::loadFromDisk: bad node id");

        BTreeNode node;

        uint8_t is_leaf = 0;
        if (!f.read(reinterpret_cast<char*>(&is_leaf), 1))
            throw std::runtime_error("BTree::loadFromDisk: EOF is_leaf");
        node.is_leaf = (is_leaf == 1);

        if (!f.read(reinterpret_cast<char*>(&node.key_count), 2))
            throw std::runtime_error("BTree::loadFromDisk: EOF key_count");

        if (!f.read(reinterpret_cast<char*>(&node.parent_id), 4))
            throw std::runtime_error("BTree::loadFromDisk: EOF parent_id");

        // keys
        node.keys.resize(node.key_count);
        for (uint16_t i = 0; i < node.key_count; ++i) {
            node.keys[i] = readValue(f);
        }

        // values
        node.values.resize(node.key_count);
        for (uint16_t i = 0; i < node.key_count; ++i) {
            if (!f.read(reinterpret_cast<char*>(&node.values[i].page_id), 4))
                throw std::runtime_error("BTree::loadFromDisk: EOF page_id");
            if (!f.read(reinterpret_cast<char*>(&node.values[i].slot_id), 2))
                throw std::runtime_error("BTree::loadFromDisk: EOF slot_id");
        }

        // children
        if (!node.is_leaf) {
            node.children.resize(node.key_count + 1);
            for (uint16_t i = 0; i <= node.key_count; ++i) {
                if (!f.read(reinterpret_cast<char*>(&node.children[i]), 4))
                    throw std::runtime_error("BTree::loadFromDisk: EOF child");
            }
        }

        _store.nodes[id] = std::move(node);
        ++loaded;
    }

    // восстанавливаем список свободных id
    for (uint32_t id = 0; id < count; ++id) {
        if (id == _store.root_id) continue;
        const auto& n = _store.nodes[id];
        if (n.key_count == 0 && n.children.empty()) {
            _store.free_ids.push_back(id);
        }
    }
}
