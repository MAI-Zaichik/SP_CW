-- агрегаты SUM / COUNT / AVG (доп 12)

CREATE DATABASE agg_test;
USE agg_test;

CREATE TABLE sales (
    id    INT    INDEXED,
    item  STRING NOT_NULL,
    price INT
);

INSERT INTO sales (id, item, price) VALUE
    (1, "apple",  100),
    (2, "banana", 50),
    (3, "cherry", 200),
    (4, "date",   150),
    (5, "free",   NULL);

-- COUNT считает только не-NULL: price -> 4, item -> 5
SELECT COUNT(price) FROM sales;
SELECT COUNT(item) FROM sales;

-- SUM = 500, AVG = 500/4 = 125 (NULL не учитывается)
SELECT SUM(price) FROM sales;
SELECT AVG(price) FROM sales;

-- несколько агрегатов и алиасы
SELECT COUNT(id), SUM(price), AVG(price) FROM sales;
SELECT SUM(price) AS total, AVG(price) AS mean FROM sales;

-- агрегат с WHERE
SELECT COUNT(id), SUM(price) FROM sales WHERE price >= 150;

-- Error: SUM/AVG только по INT
SELECT SUM(item) FROM sales;
SELECT AVG(item) FROM sales;

-- Error: агрегат вперемешку с обычной колонкой
SELECT item, SUM(price) FROM sales;

DROP TABLE sales;
DROP DATABASE agg_test;
