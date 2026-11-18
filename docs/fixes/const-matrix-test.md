# Corrección de la prueba de dimensiones constantes

El caso original de `nsc.ns3.t04.const-generics` declaraba
`Matrix<int, 2, 3>` con campo `T[Rows][Columns]` y accedía a `[2][1]`.
El primer índice queda fuera de las dos filas. En la base, la inspección de
miembros muestra `builtin:int[3][2]` tanto para esa especialización como para
el campo no genérico `int[2][3]`; `nsc.language.fixed-multidimensional-arrays`
documenta la misma representación de dimensiones. Evidencia en
`A:/nsc-fixes-evidencia/const-matrix-probe`.

La prueba ejecuta `[1][2]` y conserva el programa original `[2][1]` como
prueba negativa que exige el diagnóstico de límite. Junto con la corrección
del reconocimiento de argumentos enum, `nsc.ns3.t04.const-generics` pasa.
El registro es `A:/nsc-fixes-evidencia/probe-const-generics.log`.
