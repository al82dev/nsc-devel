# T08 H3: inicializadores y parámetros de lambda

Antes de la corrección `b41b36020`, el control de lambda reprodujo la captura
indebida de `shadow_value` en O0–O3, aunque el consumidor ejecutaba la lambda.

Después de la construcción incremental del compilador y de las pruebas de
identidad, con build `f2be0335c46cb515`, `diagnosticar.cmd` terminó con código
0 en O0–O3: el consumidor obtuvo 23 de la lambda y no se publicó la global C
homónima. El control incluye también los casos previos de parámetros y `for`.

`validar-ambito.cmd` terminó con código 0 en O0–O3 y comprobó el resultado 264:
incluye llamadas C en inicializadores tanto de variable local como de `for`,
además de las llamadas antes y después de los ámbitos sombreados.

Esto no verifica todos los tipos de ámbitos ni sustituye la validación completa
de la tanda 8, que sigue abierta.
