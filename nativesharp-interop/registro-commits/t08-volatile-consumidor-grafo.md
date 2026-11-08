# T08-010/015: conservar modo consumidor al reconstruir el grafo

El control de generación HIR aceptó por error un proyecto con selección
explícita. El cargador la conserva, pero las cinco reconstrucciones de `Project`
en `ProjectGraphLoader.cpp` y `ProjectGraph.cpp` omitían el campo y lo dejaban
ausente. Se transporta ahora junto con las referencias y demás propiedades.

La corrección mantiene la protección temporal HIR; todavía no implementa los
accesos volatile. Se debe construir incrementalmente y repetir tanto el control
del grafo como la comprobación de que la generación rechaza la selección hasta
que esté conectada al lowering.
