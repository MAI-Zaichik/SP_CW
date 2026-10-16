-- комментарии: -- до конца строки. ; внутри комментария не завершает запрос.

CREATE DATABASE cmt_test;   -- инлайн-комментарий после команды
USE cmt_test;

-- комментарий с кириллицей перед командой
CREATE TABLE t (id INT, note STRING);

-- ; внутри комментария игнорируется, запрос закрывает ; на следующей строке
INSERT INTO t (id, note)   -- тут точка с запятой; в комментарии не считается
    VALUE (1, "first");

SELECT * FROM t;   -- ожидаем одну строку

-- закомментированная команда ниже не выполняется:
-- INSERT INTO t (id, note) VALUE (99, "skipped");

SELECT * FROM t;   -- по-прежнему одна строка

DROP TABLE t;
DROP DATABASE cmt_test;
