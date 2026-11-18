# Integración de identidades de vistas

Se integraron los commits `ce09d6a90` y `2c9281e8a` de la rama local de
vistas mediante `cherry-pick -x`, como `9f6575ee3` y `8c11b688b`.
El segundo corrige un caso de shadowing sobre el trabajo del primero. La
evidencia original usó un build que también contenía cambios FnOnce sin
registrar; esta validación se hizo en la rama integradora sin esos cambios.

El build incremental de `nsc` y
`nsc_project_callable_result_merger_ordinal_tests` terminó con salida 0.
El compilador `A:/nsc-fixes-evidencia/build/bin/nsc.exe` tuvo SHA-256
`4aedad5ddd323493d549492da5a649ec23bacae39096fc2e922c01731780211d`.
Los comandos se ejecutaron desde `A:/nsc-devel-fixes`, con temporales y
runtime aislados en `A:/nsc-fixes-evidencia`. Evidencia:

- `vistas-integradas-tupla-20261112.log`: el runner de tuplas terminó con
  `FAILURES []`, incluyendo pruebas positivas O0/O2 y diagnósticos negativos.
- `vistas-integradas-shadow-20261112.log` y el `results.json` allí indicado:
  cuatro positivos compilaron y devolvieron 42 en O0/O2; tres escapes se
  rechazaron en análisis y no se ejecutaron.
- `vistas-integradas-ordinal-20261112.log`: la prueba dirigida
  `nsc.project-callable-result-merger-ordinals` pasó 1/1.

Estos controles no acreditan toda la seguridad de vistas. Algunas rutas de
reemplazo y `ref/out` aún consultan nombres; los orígenes desconocidos tienen
una resolución conservada por nombre, y la captura de raíces no cubre todas
las expresiones ni `in`. Faltan controles de shadowing combinado con
reemplazo, alias y llamadas, así como cobertura binaria y genérica ampliada.
Estos límites son hallazgos de inspección, no escapes ya demostrados.
