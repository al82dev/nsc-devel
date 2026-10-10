# SCOPE LOCK — Validaciones 151 a 155

## Objetivo autorizado

Cerrar, antes de una única validación integral final, los cinco frentes de
infraestructura u oráculo que permanecen en la corrida
`20261010-090333-82a4a13f`:

1. materialización contenida de cinco proyectos productores self-host;
2. descubrimiento portable del archivador requerido por Packaging;
3. presupuesto temporal correcto para once casos NIGHTLY de Dominance;
4. aislamiento explícito de dos defectos conocidos que terminan el proceso;
5. evidencia diagnóstica suficiente para separar defectos reales del
   compilador de defectos de la prueba.

Naturaleza: corrección de infraestructura de pruebas, oráculos y evidencia.

## Revisión base y rango evaluado

- rama: `development`;
- revisión base: `ef701c6b8e93e09c26fd371e830f252074e06d16`;
- corrida funcional de autoridad:
  `_nscbuilder3/history/20261010-090333-82a4a13f`;
- rango nuevo: commits posteriores a la revisión base, uno por hipótesis
  causal.

## Comportamiento que debe permanecer idéntico

- semántica, ABI, metadata, diagnósticos y generación de `nsc.exe`;
- expectativas contractuales de las pruebas;
- inventario de `9,587` casos y `5,695` variantes ejecutables;
- batería histórica y sus archivos;
- selección `Debug`, optimización NativeSharp `O0`, perfil `Todos`;
- resultados conocidos visibles: nunca convertir un fallo en `PASS` ni
  ocultarlo como omitido.

## Presupuesto de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componentes | Builder 3, controladores self-host, manifiesto de la suite, soporte de oráculos host, documentación de Fase 2 |
| Archivos self-host | `selfhost_inventory.py`, cinco `run_selfhost_*_gate.py` productores y pruebas dirigidas nuevas o existentes |
| Archivos Packaging | `nscbuilder3.py` y pruebas dirigidas de descubrimiento de rutas |
| Archivos Dominance | `tests/compiler-suite/manifest/cases.jsonl`, pruebas contractuales del manifiesto y documentación |
| Archivos defectos conocidos | `cases.jsonl`, `nscbuilder3.py` y pruebas del sobre host/proceso |
| Archivos evidencia | `FrontendContractSupport.h`, soporte mínimo de `TestContext` sólo si es indispensable, pruebas dirigidas y documentación |
| Estado persistente | `TRACKING.md`, `CURSOR.json` y un Markdown por validación |
| Configuración final | `Debug/O0/Todos` |

Los archivos candidatos que una auditoría causal demuestre innecesarios no se
modifican. Todo archivo no enumerado permanece protegido.

## Archivos protegidos expresamente

- todo `include/nativesharp/**`, `src/**`, `selfhost/nsc/**` y `lib/**`;
- `tests/compiler-optimization/test-infrastructure/**` salvo autorización
  posterior;
- los proyectos fuente `tests/compiler-suite/selfhost/modules/*.nsproj`;
- los seis archivos con trabajo previo del usuario:
  - `docs/compiler-optimization/test-infrastructure/test-execution-parity.csv`;
  - `docs/compiler-optimization/test-infrastructure/test-execution-parity.json`;
  - `docs/compiler-optimization/test-infrastructure/test-execution-parity.md`;
  - `tests/harness/nsc_test/suites.py`;
  - `tests/harness/run_nsc_tests.py`;
  - `tests/harness/tests/test_suite_adapter.py`.

## Acciones prohibidas

- modificar el producto para satisfacer una prueba;
- cambiar expectativas para que un fallo pase;
- retirar, omitir o saltar casos;
- reutilizar artefactos viejos como fallback;
- instalar o desplegar en `C:`;
- ejecutar validaciones funcionales intermedias;
- usar recursión;
- limpiar o incluir trabajo ajeno en commits;
- ejecutar baseline, promoción o minibaseline.

## Estrategia de validación

Durante las cinco correcciones sólo se permiten auditoría estática, validación
de sintaxis/estructura y revisión diferencial. No se ejecuta la suite entre
tandas. Al cerrar la quinta corrección se construye lo necesario y se ejecuta
una sola vez el conjunto completo `Debug/O0/Todos`.

## Criterio de terminación

- los cinco frentes poseen corrección mínima y evidencia documental;
- ningún caso fue eliminado ni convertido artificialmente en éxito;
- los seis archivos protegidos conservan sus hashes iniciales;
- cada hipótesis causal tiene commit propio;
- la corrida integral final termina y publica todos sus resultados;
- cualquier rojo residual contiene evidencia suficiente para atribuirlo a un
  contrato real del compilador o a un bloqueo explícito, sin residuo conocido
  de la infraestructura corregida.
