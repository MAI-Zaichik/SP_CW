-- smoke-тест: проход по всем 9 SQL-командам по ТЗ.
-- запуск: ./prog tests/smoke.sql
-- ожидание: каждая команда печатает результат, программа выходит на EOF.

-- DDL уровня БД
CREATE DATABASE shop;
USE shop;

-- CREATE TABLE с тремя видами constraint (NONE / NOT_NULL / INDEXED)
CREATE TABLE users (
    id    INT    INDEXED,
    name  STRING NOT_NULL,
    email STRING
);

-- INSERT - одиночный и многострочный (несколько VALUE подряд через запятую)
INSERT INTO users (id, name, email) VALUE (1, "Alice", "alice@example.com");
INSERT INTO users (id, name, email)
    VALUE (2, "Bob",   "bob@example.com"),
          (3, "Carol", "carol@example.com"),
          (4, "Dave",  NULL);

-- SELECT * - все колонки, все строки
SELECT * FROM users;

-- SELECT с проекцией и AS-алиасом
SELECT name AS user_name, email AS contact FROM users;

-- SELECT с WHERE (равенство)
SELECT * FROM users WHERE id == 2;

-- UPDATE с WHERE
UPDATE users SET email = "alicia@example.com" WHERE name == "Alice";
SELECT * FROM users WHERE id == 1;

-- UPDATE сразу нескольких колонок
UPDATE users SET name = "Robert", email = "rob@example.com" WHERE id == 2;
SELECT * FROM users WHERE id == 2;

-- DELETE с WHERE
DELETE FROM users WHERE id == 4;
SELECT * FROM users;

-- DROP TABLE / DROP DATABASE
-- на Windows может упасть из-за бага в Storage (порядок pagers.erase vs fs::remove)
DROP TABLE users;
DROP DATABASE shop;
