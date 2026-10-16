#pragma once
#include <string>
#include <nlohmann/json.hpp>

struct ExecuteResult {
    bool ok = true;
    std::string message;
    nlohmann::json data = nullptr;
};