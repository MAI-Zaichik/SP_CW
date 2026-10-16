#include "parser/ast.h"


void CreateDatabaseStmt::accept(Visitor& v) {v.visit(*this);}
void DropDatabaseStmt::accept(Visitor& v) {v.visit(*this);}
void UseStmt::accept(Visitor& v) {v.visit(*this);}
void CreateTableStmt::accept(Visitor& v) {v.visit(*this);}
void DropTableStmt::accept(Visitor& v) {v.visit(*this);}
void InsertStmt::accept(Visitor& v) {v.visit(*this);}
void UpdateStmt::accept(Visitor& v) {v.visit(*this);}
void DeleteStmt::accept(Visitor& v) {v.visit(*this);}
void SelectStmt::accept(Visitor& v) {v.visit(*this);}

