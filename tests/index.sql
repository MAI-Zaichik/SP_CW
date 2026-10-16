-- INDEXED: уникальность, поиск по индексу, нотация db.table

CREATE DATABASE idx_test;
USE idx_test;

CREATE TABLE users (
    id    INT    INDEXED,
    name  STRING NOT_NULL,
    email STRING
);

INSERT INTO users (id, name, email) VALUE
    (1, "Alice", "alice@example.com"),
    (2, "Bob",   "bob@example.com"),
    (3, "Carol", "carol@example.com");

-- поиск по индексу (== на INDEXED-колонке)
SELECT * FROM users WHERE id == 2;

-- Error: дубликат в INDEXED-колонке
INSERT INTO users (id, name, email) VALUE (1, "Duplicate", "dup@example.com");

-- Error: UPDATE создаёт дубликат id
UPDATE users SET id = 3 WHERE name == "Alice";

-- UPDATE на свободное значение - ОК
UPDATE users SET id = 10 WHERE name == "Alice";
SELECT * FROM users WHERE id == 10;

-- DELETE освобождает значение, потом его можно вставить снова
DELETE FROM users WHERE id == 2;
INSERT INTO users (id, name, email) VALUE (2, "Bob2", "bob2@example.com");
SELECT * FROM users WHERE id == 2;

-- db.table без предварительного USE (текущая БД остаётся idx_test)
CREATE DATABASE other_db;
CREATE TABLE other_db.flags (id INT INDEXED, label STRING);
INSERT INTO other_db.flags (id, label) VALUE (1, "from_idx_test");
SELECT * FROM other_db.flags;

DROP TABLE other_db.flags;
DROP DATABASE other_db;
DROP TABLE users;
DROP DATABASE idx_test;
