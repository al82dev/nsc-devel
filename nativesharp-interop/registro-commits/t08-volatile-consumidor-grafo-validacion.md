# T08-010/015: validación del modo en el grafo

Antes de `694f67e2b`, `nsc --check-project-oop-hir` aceptó un proyecto con
`CppConsumerVolatileMode=Microsoft`: cinco reconstrucciones del grafo descartaban
la selección y la protección temporal no la veía.

Después de corregirlas se construyeron incrementalmente `nsc` y
`nsc_project_graph_tests`, con build `dd05c84ac56c8bef`. El llamador agrupado
terminó con código 0: modelo 78, cargador 96, grafo 40 y HIR dirigido 4
controles. Un proyecto independiente con la propiedad explícita confirmó por
`CALL` que la generación HIR se detiene con el diagnóstico temporal esperado.

Esto verifica conservación en los casos probados, no que el modo genere los
accesos correctos. La opción sigue protegida y la tanda 8 permanece abierta.
