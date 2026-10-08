# Tanda 10 — Semántica, tipos y comptime

## Propósito y resultado

La tanda incorpora al único `nsc_unit_tests.exe` los contratos históricos de semántica, tipos y comptime que
pueden ejecutarse en O0 sin perder sus aserciones. También externaliza el dump semántico directo como un caso
del catálogo común. Los ejecutables históricos permanecen disponibles hasta el corte de la Tanda 18.

El resultado dirigido es:

- 91 casos del componente `sema`: 91 aprobados y 0 fallidos;
- 7 casos del componente `comptime`: 7 aprobados y 0 fallidos;
- 1 fixture externo `nsc.dump-sema`: 1 aprobado y 0 fallidos;
- 15 pruebas del harness Python: 15 aprobadas y 0 fallidas.

## SCOPE LOCK aplicado

- Repositorio autorizado: `A:\nsc-devel`.
- Naturaleza: infraestructura, refactor de pruebas y migración con paridad.
- Código de producción del compilador: protegido y sin cambios.
- `nscbuilder.py`, `_nscbuilder`, instalaciones y herramientas en `C:`: protegidos y sin cambios.
- Configuración funcional: Windows x64, O0 y NSLink explícito desde A:.
- Prohibido: baseline, promoción, certificación, instalación, O1, O2, O3 y remotos Git.
- Las entradas, expectativas e identificadores históricos permanecen como contratos.
- Los adaptadores son transitorios y no autorizan retirar todavía los ejecutables anteriores.

## Inventario cerrado

La matriz inicial asignó 107 identificadores primarios a semántica, tipos o comptime. La tanda los conserva
sin omisiones mediante esta partición:

| Destino | Cantidad | Evidencia de membresía |
|---|---:|---|
| Registro consolidado `sema` | 91 | `SemanticLegacyEntries.def` |
| Registro consolidado `comptime` | 7 | `SemanticLegacyEntries.def` |
| Catálogo externo `nsc.dump-sema` | 1 | `dump-sema.case.json` |
| Enrutados a tandas posteriores | 8 | Tabla de enrutamiento de este documento |
| Total | 107 | `test-migration-state.json` |

Los 98 identificadores consolidados aparecen individualmente en
`tests/unit/include/nsc_test/SemanticLegacyEntries.def`. El archivo es la fuente legible por máquina y evita
duplicar listas que puedan divergir. Contiene 44 entradas semánticas directas, 7 entradas comptime y 47
selectores O0 del contrato CC1.

## Enrutamiento explícito

Ocho casos no se absorbieron artificialmente en esta tanda porque su contrato dominante pertenece a otra
frontera del plan:

| Identificador | Tanda | Motivo |
|---|---:|---|
| `nsc.cc1.generic-constraints` | 14 | Proyecto `.nsproj` y resolución entre proyectos |
| `nsc.ns3.t41.ownership` | 16 | Flujo especializado de múltiples etapas |
| `nsc.stage44.generics` | 14 | Proyecto completo y metadata |
| `nsc.stage44.generics-monomorphization` | 12 | Monomorfización y artefactos intermedios |
| `nsc.stage50-11.ownership-execution` | 11 | Ejecución de ownership |
| `nsc.stage52-1.generic-collections-additions` | 12 | HIR/MIR de colecciones genéricas |
| `nsc.stage52.generic-collections-compiler-fixes` | 12 | Correcciones verificadas mediante IR |
| `nsc.stage56.c7-definite-assignment` | 11 | Análisis de flujo y asignación definida |

El enrutamiento queda persistido en `test-migration-state.json`; no elimina ni aprueba por anticipado esos
casos.

## Diseño implementado

### Registro consolidado

`SemanticUnitTests.cpp` registra cada entrada mediante `TestRegistry`. Las unidades históricas se incorporan
con adaptadores de transición que renombran su `main` y conservan todas sus comprobaciones. No se crea un
runner por componente ni un proceso por identificador.

El registro distingue dos formas:

- entrada histórica sin argumentos para semántica y comptime;
- selector CC1 que invoca el dueño histórico con exactamente un nombre de fixture.

Cada caso CC1 conserva el identificador histórico extendido con su selector. Esto permite ejecutar y reportar
una sola reproducción sin perder el dueño común.

### Selección estricta de O0

El ejecutable histórico `nsc_cc1_correctness_tests.exe` conserva su matriz O0/O2. El target consolidado define
`NSC_TEST_O0_ONLY`, por lo que esa misma fuente recorre únicamente O0 al formar parte de la infraestructura
nueva. La bifurcación es de build y no modifica expectativas, fixtures ni el target histórico.

Cinco dueños que ejercen O2 como parte inseparable de su contrato se dejaron fuera de esta tanda y se
enrutaron a las tandas 11 o 12. No se ejecutó O2 para aprobar la tanda.

### Directorio del compilador seleccionado

`NSC_TEST_COMPILER_BUILD_DIR` permite que el ejecutable consolidado use el árbol del compilador construido que
se seleccione al configurar CMake. La validación fijó `A:\nsc-devel\build-clang`; no consultó el NSC instalado.

### Dump semántico externo

`dump-sema.case.json` reutiliza `full_frontend.ns`, ejecuta `--dump-sema` con O0 y conserva la expectativa
histórica `conversion implicit-string-literal-to-char-pointer`. El fixture se ejecuta mediante
`run_nsc_tests.py` con rutas explícitas de NSC y NSLink en A:.

## Corrección local durante la validación

`nsc.ns3.t31.array-borrow` compilaba y enlazaba su consumidor, pero el adaptador fallaba al ejecutar un
`cmd.exe` anidado dentro de `std::system`. La reproducción aislada confirmó que no era contaminación entre
casos.

La corrección se limita a la compilación consolidada:

- el target histórico conserva `cmd.exe /d /s /c` sin cambios;
- el adaptador define `NSC_TEST_CONSOLIDATED_HOST`;
- bajo esa definición, `std::system` recibe directamente la ruta entre comillas del ejecutable;
- el shell que ya utiliza `std::system` sigue siendo el único responsable de iniciar el proceso.

La prueba aislada pasó 12/12 comprobaciones después del cambio y el componente semántico completo pasó 91/91.

## Archivos creados

- `tests/unit/modules/SemanticUnitTests.cpp`: registro común de semántica, tipos y comptime.
- `tests/unit/include/nsc_test/SemanticLegacyEntries.def`: membresía determinista de 98 casos.
- `tests/unit/semantic_adapters/*.cpp`: 52 adaptadores transitorios a dueños históricos.
- `tests/cases/lexer/nsc.dump-tokens/dump-sema.case.json`: fixture externo del dump semántico.
- este documento editorial.

## Archivos modificados

- `tests/unit/CMakeLists.txt`: integra el módulo, adaptadores, build seleccionado y perfil O0.
- `tests/unit/consolidated/main.cpp`: registra el módulo semántico.
- `tests/unit/include/nsc_test/Modules.h`: declara el punto de registro.
- `tests/unit/Cc1CorrectnessTests.cpp`: conserva O0/O2 por defecto y habilita O0 sólo para el consolidado.
- `tests/unit/NativeSharp3T31ArrayBorrowTests.cpp`: evita únicamente el shell anidado del adaptador.
- `test-migration-state.json`: conserva el fixture, los grupos y los ocho casos enrutados.
- `TRACKING.md`: marca la tanda terminada.

## Algoritmos, complejidad y ownership

El registro realiza una pasada lineal O(n) sobre 98 descriptores estáticos. Cada selección CC1 ejecuta sólo su
fixture. Los adaptadores no crean ownership adicional: invocan la función histórica de forma síncrona y
propagan su resultado al `TestContext`. No se añadió recursión.

## Validación y evidencia

Configuración efectiva:

- `nsc.exe`: `A:\nsc-devel\build-clang\bin\nsc.exe`;
- NSLink: `A:\nslink-devel\bin\release\nslink.exe`;
- optimización: O0;
- runtime de prueba: artefacto generado bajo `A:\nsc-devel\build-clang`;
- raíz de bibliotecas: staging existente bajo `A:\native-sharp-3.0-baseline\lib`.

Resultados:

| Gate dirigido | Seleccionadas | Aprobadas | Fallidas |
|---|---:|---:|---:|
| `nsc_unit_tests.exe --component sema` | 91 | 91 | 0 |
| `nsc_unit_tests.exe --component comptime` | 7 | 7 | 0 |
| `run_nsc_tests.py --case nsc.dump-sema` | 1 | 1 | 0 |
| `unittest` del harness | 15 | 15 | 0 |

La construcción dirigida de `nsc_unit_tests.exe` terminó con código cero. La inspección estática confirmó que
los dueños consolidados no ejecutan O1, O2 ni O3 bajo el perfil nuevo. `git diff --check`, validación JSON y el
gate de residuos se ejecutan antes del commit.

## Comportamiento preservado

- No cambió código de producción, semántica, diagnósticos, API ni ABI del compilador.
- No cambió ninguna fuente NativeSharp ni expectativa histórica.
- No se retiró ningún ejecutable o CTest anterior.
- No se modificó `nscbuilder.py` ni `_nscbuilder`.
- No se instaló ni desplegó nada.
- No se ejecutó baseline, promoción, certificación, O1, O2 ni O3.
- No se usaron remotos Git.

## Estado y trabajo posterior

La Tanda 10 queda `TERMINADA` con paridad dirigida. Los adaptadores y literales históricos sólo pueden
retirarse en la Tanda 18 después de demostrar paridad del inventario completo. Los ocho casos enrutados siguen
pendientes en sus tandas asignadas.
