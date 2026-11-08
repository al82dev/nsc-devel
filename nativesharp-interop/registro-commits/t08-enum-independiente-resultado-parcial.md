# Resultado parcial de enum independiente y su alias

La construcción incremental con `69e5c61cc` terminó con código 0
(`9d527f3b87f9f837`). El control con header y metadata avanzó desde el fallo
de observación de `NMood` hasta la resolución de su alias, que informó
`conflicting C/C++ type alias 'NMood'`.

Tras `f15d6fd36`, la construcción incremental terminó con código 0
(`fd30dbe92a2a24ea`), pero el mismo control conservó el conflicto en O0.
Las nuevas rutas aún no están validadas y el caso negativo y O1–O3 no se
alcanzaron. Debe determinarse qué condición concreta impide la conciliación;
no debe ampliarse el guard sin evidencia. La tanda 8 sigue abierta.
