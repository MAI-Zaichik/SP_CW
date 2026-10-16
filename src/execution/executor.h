#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "parser/ast.h"
#include "core/result.h"
#include "catalog/catalog.h"
#include "storage/storage.h"
#include "index/index_manager.h"

class Executor : public Visitor {
public:
    Executor(Catalog& catalog, Storage& storage);

    ExecuteResult execute(ASTNode& node);

    void visit(CreateDatabaseStmt&) override;
    void visit(DropDatabaseStmt&) override;
    void visit(UseStmt&) override;
    void visit(CreateTableStmt&) override;
    void visit(DropTableStmt&) override;
    void visit(InsertStmt&) override;
    void visit(UpdateStmt&) override;
    void visit(DeleteStmt&) override;
    void visit(SelectStmt&) override;

private:
    Catalog& _catalog;
    Storage& _storage;
    std::string _current_db;
    IndexManager _index;
    ExecuteResult _result;

    std::pair<std::string, std::string> resolve(const TableReference& ref) const;

    Value evalExpr(const ExprNode* expr, const std::unordered_map<std::string, Value>& row) const;

    bool matchRow(const ExprNode* where, const std::unordered_map<std::string, Value>& row) const;

    std::unordered_map<std::string, Value> makeRowMap(const std::vector<Value>& row, const TableSchema& schema) const;

    void rebuildIndexes(const std::string& db);

    static nlohmann::json valueToJson(const Value& v);
};