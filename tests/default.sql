-- DEFAULT в CREATE TABLE (доп 10)

CREATE DATABASE def_test;
USE def_test;

CREATE TABLE orders (
    id     INT    INDEXED,
    item   STRING NOT_NULL,
    qty    INT    DEFAULT 1,
    status STRING DEFAULT "new"
);

-- все колонки заданы - DEFAULT не нужен
INSERT INTO orders (id, item, qty, status) VALUE (1, "book", 3, "shipped");

-- пропущенные колонки берут DEFAULT
INSERT INTO orders (id, item) VALUE (2, "pen");
INSERT INTO orders (id, item, qty) VALUE (3, "lamp", 5);

-- ждём: 1->(3,"shipped"), 2->(1,"new"), 3->(5,"new")
SELECT * FROM orders;

-- DEFAULT NULL: пропуск даёт NULL
CREATE TABLE notes (
    id   INT INDEXED,
    body STRING DEFAULT NULL
);
INSERT INTO notes (id) VALUE (10);
SELECT * FROM notes;

-- Error: тип DEFAULT не совпадает с типом колонки
CREATE TABLE bad (id INT DEFAULT "oops");

DROP TABLE orders;
DROP TABLE notes;
DROP DATABASE def_test;
