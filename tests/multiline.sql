-- многострочные запросы и строковые литералы.

CREATE DATABASE ml_test;
USE ml_test;

-- одна команда на нескольких строках
CREATE TABLE notes (
    id   INT NOT_NULL,
    body STRING
);

-- строковый литерал, разорванный переносом строки: перенос входит в значение
INSERT INTO notes (id, body) VALUE (1, "line one
line two");

SELECT * FROM notes;

-- многострочный WHERE
SELECT id
    FROM notes
    WHERE id == 1
       OR id == 2;

DROP TABLE notes;
DROP DATABASE ml_test;
