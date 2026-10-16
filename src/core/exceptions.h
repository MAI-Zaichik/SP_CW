#ifndef SP_CW_EXCEPTIONS_H
#define SP_CW_EXCEPTIONS_H

#include <stdexcept>
#include <string>

struct SyntaxError : std::runtime_error {
    explicit SyntaxError(const std::string& m) : std::runtime_error(m) {}
};
struct IncompleteInput : std::runtime_error {
    explicit IncompleteInput(const std::string& m) : std::runtime_error(m) {}
};
struct SemanticError : std::runtime_error {
    explicit SemanticError(const std::string& m) : std::runtime_error(m) {}
};
struct TypeError : std::runtime_error {
    explicit TypeError(const std::string& m) : std::runtime_error(m) {}
};
struct ConstraintError : std::runtime_error {
    explicit ConstraintError(const std::string& m) : std::runtime_error(m) {}
};

#endif //SP_CW_EXCEPTIONS_H