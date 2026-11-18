# Consumo de closures FnOnce por aliases locales

Estado: integración parcial para aliases locales, con límites detallados abajo. La rama aislada
parte de `65bf5b83e618e386126628fa0b780fb26a4ad547`; las fuentes compiladas y enlazadas
incluyen cambios WIP congelados de la copia principal, por lo que no constituyen una revisión
limpia de ese commit. El parche se compara con la instantánea de esos archivos WIP, no con HEAD.

## Reproducción y corrección

Una captura `[move box]` crea una closure FnOnce. Antes, `Read alias = action; action();
alias();` pasaba la comprobación porque el estado de invocación se guardaba por ordinal local.
El prototipo registra, para cada local, las posibles procedencias de lambda y el consumo de esas
procedencias, copia ese estado en las instantáneas de flujo, une alternativas entre ramas y
sustituye la procedencia del destino al reasignar. En retrocesos de bucles rechaza un entorno
consumido que permanece accesible mediante un local cuyo nombre aparece en el bucle.

La comprobación de bucle usa menciones sintácticas conservadoras. Puede rechazar usos seguros
si un alias mencionado no se invoca; no demuestra ausencia de escapes ni de otros usos indirectos.
El origen es un nodo sintáctico: varias creaciones dinámicas desde un mismo sitio pueden
confundirse. La prueba incluye una closure fresca por iteración y una reasignación con un alias
antiguo no usado, pero no certifica todas las combinaciones de ciclos o ramas.

El contrato documentado clasifica una captura move como FnOnce y transfiere el dueño al entorno.
Interpretar el consumo como propiedad de ese entorno compartido es la elección de esta
corrección. No se cambia el comportamiento de closures sin captura move de forma intencional.

## Evidencia

La compilación incremental integrada de 2026-11-11 terminó correctamente.
CTest ejecutó sin omisiones `nsc.compiler.fnonce-shared-environment`,
`nsc.compiler.borrowed-lambda-owned-view` y
`nsc.compiler.borrowed-lambda-delegate-ref-out`: 1/1 cada una. Los logs
`fnonce-integrated-20261111.log`, `fnonce-borrow-owned-20261111.log` y
`fnonce-borrow-ref-out-20261111.log` están en `A:/nsc-fixes-evidencia`.
El compilador contenía otros cambios locales; esta evidencia no certifica
una revisión limpia. El análisis de parámetros y campos sigue abierto.

- Ejecutable de referencia WIP, SHA-256
  `78aef06df829e03e23beb15a8c2f3d8d15dbb3d9e252e6231789d2f58ac5370d`:
  aceptó las dobles llamadas mediante aliases, ramas y ciclos. Los casos inseguros solo se
  analizaron, nunca se ejecutaron.
- La modificación de cabeceras exigió recompilar incrementalmente 191 unidades dependientes del
  ejecutable. El compilador aislado final tiene SHA-256
  `e22d49dc6c4192852ef00525c457bc4e7e879a952fc55ee789a23a766710fca7`.
  Las entradas se congelaron en `A:\nsc-fixes-agent1-temp\fnonce-source` y
  `A:\nsc-fixes-agent1-temp\fnonce-objects`; no se enlazaron objetos con disposición ABI antigua.
- `tests/integration/fnonce_shared_environment.py` pasó con diagnósticos NS3020 en los negativos;
  los controles seguros se compilaron y ejecutaron en O0/O2. Los casos de doble llamada y el
  control de reasignación con limpieza no definida se limitaron al análisis.
- También pasaron `borrowed_lambda_owned_view.py` con sus controles de ejecución habilitados y
  `borrowed_lambda_delegate_ref_out.py`, usando el Core aislado de la campaña.
- Comandos y resultados: `A:\nsc-fixes-agent1-temp\fnonce-baseline\baseline.log`,
  `candidate.log`, `A:\nsc-fixes-agent1-temp\fnonce-borrow-regressions`,
  `A:\nsc-fixes-agent1-temp\fnonce-scope` y scripts adyacentes. El parche independiente está
  en `A:\nsc-fixes-agent1-temp\fnonce-environment-prototype.patch`.

## Limitaciones confirmadas

Dos invocaciones mediante parámetros separados y la llamada por un campo estático después de la
llamada por un local siguen aceptándose en análisis: el seguimiento es intraprocedimental y
local. No se ejecutaron estas reproducciones potencialmente inseguras. La guarda tampoco cubre
automáticamente temporales, arreglos, campos de objetos ni delegados cuyo origen no sea trazable.
Las pruebas de un alias anidado, alias reasignado, cadena y rama dentro de un bucle se rechazan;
no equivalen a un análisis exhaustivo de todos los caminos de escape.

El parche toca `ProjectMethodBodyModel.h`, `OwnershipFlowSnapshot.h`, `CallAnalyzer.cpp`,
`AssignmentAnalyzer.cpp`, `StatementAnalyzer.cpp`, `BodyScopeAndFlow.cpp` y
`MethodBodyCallableLifecycle.cpp`. Comparte archivos con los cambios de préstamos y punteros;
debe revisarse por fragmentos contra el WIP vigente. No incluye los parches independientes de
`delete` ni de referencias repetidas a capturas move.
