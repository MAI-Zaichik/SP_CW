#pragma once
#include <string>
#include <string_view>
#include <unordered_set>

// intern() возвращает string_view на единственный экземпляр строки в пуле.
class StringPool {
public:
    static StringPool& instance() {
        static StringPool inst;
        return inst;
    }

    // если строка уже есть — возвращает view на неё;
    // иначе вставляет и возвращает view на новый экземпляр
    std::string_view intern(std::string s) {
        return *_pool.insert(std::move(s)).first;
    }

private:
    StringPool() = default;
    std::unordered_set<std::string> _pool;
};