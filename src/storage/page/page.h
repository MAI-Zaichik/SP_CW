#pragma once

#include <vector>
#include <cstdint>
#include "core/types.h"
#include <cstring>

inline constexpr size_t PAGE_SIZE = 4096;

struct Page {
    static const uint16_t DELETED_SLOT = 0xFFFF;

    // индекс страницы в файле
    uint32_t page_id;
    // сырые байты страницы, fstream требует char*
    char data[PAGE_SIZE];
    // true если страница изменена и не сброшена на диск
    bool is_dirty = false;

    Page(uint32_t id = 0) : page_id(id) {
        init();
    }

    // инициализирует заголовок страницы, free_offset стартует с конца страницы
    void init() {
        set_row_count(0);
        // данные растут от конца к началу
        set_free_space_offset(PAGE_SIZE);
    }

    uint16_t get_row_count() const {
        return *reinterpret_cast<const uint16_t*>(data);
    }

    void set_row_count(uint16_t count) {
        *reinterpret_cast<uint16_t*>(data) = count;
    }

    uint16_t get_free_space_offset() const {
        return *reinterpret_cast<const uint16_t*>(data + sizeof(uint16_t));
    }

    void set_free_space_offset(uint16_t offset) {
        *reinterpret_cast<uint16_t*>(data + sizeof(uint16_t)) = offset;
    }

    uint16_t get_slot(uint16_t slot_id) const {
        return *reinterpret_cast<const uint16_t*>(data + sizeof(uint16_t) * 2 + slot_id * sizeof(uint16_t));
    }

    void set_slot(uint16_t slot_id, uint16_t offset) {
        *reinterpret_cast<uint16_t*>(data + sizeof(uint16_t) * 2 + slot_id * sizeof(uint16_t)) = offset;
    }

    // добавляет строку в страницу, slot array растёт от начала, данные от конца
    bool add_row(const uint8_t* row_data, uint16_t size, uint16_t& slot_id) {
        uint16_t row_count = get_row_count();
        uint16_t free_offset = get_free_space_offset();

        uint16_t header_size = sizeof(uint16_t) * 2;
        uint16_t slot_array_size = row_count * sizeof(uint16_t);

        uint16_t data_needed = sizeof(uint16_t) + size;
        uint16_t slot_needed = sizeof(uint16_t);

        uint16_t used_space = header_size + slot_array_size;
        uint16_t free_space = free_offset - used_space;

        if (free_space < data_needed + slot_needed) {
            return false;
        }

        free_offset -= data_needed;

        std::memcpy(data + free_offset, &size, sizeof(uint16_t));
        std::memcpy(data + free_offset + sizeof(uint16_t), reinterpret_cast<const char*>(row_data), size);

        set_slot(row_count, free_offset);
        set_row_count(row_count + 1);
        set_free_space_offset(free_offset);

        slot_id = row_count;
        is_dirty = true;

        return true;
    }

    // возвращает указатель на данные строки, nullptr если слот удалён или невалиден
    const uint8_t* get_row(uint16_t slot_id, uint16_t& size) const {
        if (slot_id >= get_row_count()) return nullptr;

        uint16_t offset = get_slot(slot_id);

        if (offset == DELETED_SLOT) return nullptr;
        // offset в зоне заголовка значит страница повреждена
        if (offset >= PAGE_SIZE) return nullptr;

        std::memcpy(&size, data + offset, sizeof(uint16_t));

        return reinterpret_cast<const uint8_t*>(data + offset + sizeof(uint16_t));
    }
};