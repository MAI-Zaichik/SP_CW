-- проверка обработки ошибок. программа НЕ должна падать ни на одной строке -
-- каждая ошибка печатается как "Error: ..." и парсер/executor идут дальше.

-- syntax errors из парсера
SELECT FROM users;
CREATE TABLE;
INSERT INTO users VALUE (1);
UPDATE SET x = 1;
SELECT * FROM;
SELECT col1 col2 FROM t;
SELECT * FROM users WHERE;

-- незакрытая строка вынесена в tests/unterminated.sql:
-- открытая кавычка поглощает весь последующий ввод, поэтому в середине батча она ломает остальные тесты

-- semantic errors из executor (БД не выбрана)
CREATE TABLE orphan (id INT);

-- готовим БД для семантических тестов
CREATE DATABASE err_test;
USE err_test;

CREATE TABLE items (
    id   INT    NOT_NULL,
    name STRING NOT_NULL,
    qty  INT    INDEXED
);

-- использование несуществующей таблицы
SELECT * FROM no_such_table;
INSERT INTO no_such_table (x) VALUE (1);
UPDATE no_such_table SET x = 1;
DELETE FROM no_such_table;
DROP TABLE no_such_table;

-- использование несуществующей колонки
INSERT INTO items (id, no_such_col) VALUE (1, "a");
SELECT no_such_col FROM items;
UPDATE items SET no_such_col = 1;
SELECT id FROM items WHERE no_such_col == 1;

-- type mismatch (STRING в INT-колонку)
INSERT INTO items (id, name, qty) VALUE ("not_a_number", "x", 1);

-- type mismatch в WHERE (сравнение int с string)
INSERT INTO items (id, name, qty) VALUE (1, "x", 1);
SELECT * FROM items WHERE id == "1";

-- NOT_NULL violation - явный NULL в NOT_NULL колонке
INSERT INTO items (id, name, qty) VALUE (NULL, "x", 2);

-- NOT_NULL violation - пропущена колонка (станет NULL по умолчанию)
INSERT INTO items (id, qty) VALUE (2, 3);

-- mismatch количества колонок и значений
INSERT INTO items (id, name) VALUE (3, "a", "extra");
INSERT INTO items (id, name, qty) VALUE (4, "b");

-- USE на несуществующую БД
USE no_such_db;

-- CREATE DATABASE с существующим именем
CREATE DATABASE err_test;

-- DROP несуществующей БД
DROP DATABASE no_such_db;

-- невалидный regex в LIKE
SELECT * FROM items WHERE name LIKE "[invalid";

-- проверяем что executor выжил - последний запрос должен сработать
SELECT * FROM items;

-- очистка
DROP TABLE items;
DROP DATABASE err_test;
