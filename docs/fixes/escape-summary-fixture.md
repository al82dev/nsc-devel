# Preparación del control de resúmenes de escape

El control existente fallaba antes de comprobar el resumen porque su copia de
metadata conservaba campos que no podían representarse en el escenario de
compatibilidad que preparaba. El fixture elimina esos campos únicamente de la
copia de prueba y mantiene las aserciones y el comportamiento del codec.

Se recompiló incrementalmente el objetivo de prueba; el control pasó sus 10
verificaciones. También pasaron los controles dirigidos de vida de vistas y
escape de préstamos. Los comandos y resultados están en
`A:/nsc-fixes-evidencia/escape-summary-fixture-scope.md`. El build incluye otros
cambios locales en curso y no certifica un árbol limpio.
