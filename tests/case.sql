-- регистр ключевых слов: слово целиком в одном регистре.
-- UPPER и lower допустимы, смешение внутри слова - ошибка.

CREATE DATABASE case_test;
USE case_test;

-- идентификатор со смешанным регистром - норма
CREATE TABLE MixedCase (id INT INDEXED, userName STRING);

-- ключевые слова в нижнем регистре - норма
insert into MixedCase (id, userName) value (1, "Alice");
select * from MixedCase;

-- ключевые слова в верхнем регистре - норма
SELECT * FROM MixedCase WHERE id == 1;

-- смешение регистров в ключевом слове - Error на каждой строке, программа идёт дальше
SeLeCt * FROM MixedCase;
Select * FROM MixedCase;
sELECT * FROM MixedCase;
CREATE dataBASE nope;

-- регистр идентификатора значим: mixedcase != MixedCase - Error, таблица не найдена
SELECT * FROM mixedcase;

DROP TABLE MixedCase;
DROP DATABASE case_test;
