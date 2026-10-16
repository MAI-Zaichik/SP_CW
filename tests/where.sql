-- WHERE-операторы: ==, !=, <, >, <=, >=, AND, OR, BETWEEN, LIKE, скобки.

CREATE DATABASE test_where;
USE test_where;

CREATE TABLE products (
    id    INT    INDEXED,
    name  STRING NOT_NULL,
    price INT
);

INSERT INTO products (id, name, price) VALUE
    (1, "apple",   100),
    (2, "banana",  50),
    (3, "cherry",  200),
    (4, "date",    300),
    (5, "elder",   150),
    (6, "Acorn",   75);

-- равенство и неравенство
SELECT name FROM products WHERE price == 100;
SELECT name FROM products WHERE price != 100;

-- <, >, <=, >=
SELECT name FROM products WHERE price < 100;
SELECT name FROM products WHERE price > 150;
SELECT name FROM products WHERE price <= 100;
SELECT name FROM products WHERE price >= 200;

-- AND слабее сравнений
SELECT name FROM products WHERE price > 50 AND price < 200;

-- OR слабее AND: price < 100 OR (name == "cherry" AND price > 100)
SELECT name FROM products WHERE price < 100 OR name == "cherry" AND price > 100;

-- скобки переопределяют приоритет
SELECT name FROM products WHERE (price < 100 OR price > 250) AND id < 5;

-- BETWEEN: наш executor реализует [low, high) - правая граница НЕ включается
-- ожидаем: 100, 150 (200 НЕ войдёт)
SELECT name FROM products WHERE price BETWEEN 100 AND 200;

-- LIKE - regex. ".*" = любая строка
SELECT name FROM products WHERE name LIKE "a.*";

-- LIKE - точное соответствие
SELECT name FROM products WHERE name LIKE "cherry";

-- LIKE с альтернативой через regex
SELECT name FROM products WHERE name LIKE "(apple|banana)";

-- отрицательные числа
INSERT INTO products (id, name, price) VALUE (7, "discount", -10);
SELECT name FROM products WHERE price < 0;

-- NULL сравнения - в нашем executor любое сравнение с NULL даёт false
INSERT INTO products (id, name, price) VALUE (8, "unknown", NULL);
SELECT name FROM products WHERE price == NULL;
SELECT name FROM products WHERE price != NULL;

-- чистим за собой
DROP TABLE products;
DROP DATABASE test_where;
