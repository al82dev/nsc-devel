# R02B5D2 — índice ordenado y coalescido de intervalos de `match` statement

## SCOPE LOCK

- Objetivo autorizado: sustituir la comparación cuadrática de intervalos enteros entre brazos de
  `match` statement por un índice disjunto ordenado y coalescido.
- Naturaleza: optimización y refactor interno; no agrega sintaxis ni corrige otro subsistema.
- Revisión correcta de partida: `956cd1a92`, cierre de R02B5D1.
- Archivos productivos autorizados: `src/Parser/StatementParser.cpp`.
- Símbolos autorizados: estado de intervalos de `MatchParseState`, normalización, consulta e
  inserción usadas únicamente por `validateMatchIntegerIntervals`.
- Comportamiento protegido: gramática, cursor, recovery, AST, rangos y texto diagnóstico, orden de
  diagnósticos, evaluación, temporales, scopes, coverage, ABI, NSMETA, HIR y MIR.
- Archivos protegidos: HIR, MIR, Driver, otros parsers y
  `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Acciones prohibidas: modificar contratos, aceptar nuevos dominios comparables, cambiar la
  semántica de guards, ejecutar baseline o batería completa, invadir R03.
- Pruebas inmutables: TP-PAR-376, TP-PAR-549, TL-LANG-311/312/313/315/322 y los CTests T03 de
  patterns/match.
- Criterio de terminación: costo `O(I log I)`, TP-PAR-550 e infraestructura completa, funciones
  bajo 60 líneas y gates focales PASS antes del commit.

## Causa y complejidad anterior

Cada brazo comparable producía un vector de intervalos half-open. Para cada intervalo nuevo,
`matchIntervalsOverlap` recorría todos los intervalos publicados por brazos anteriores y todos los
anteriores del mismo brazo. Además, el estado conservaba duplicados y solapamientos sin
normalización. Para `I` alternativas distribuidas en brazos, la comparación acumulada era
`O(I²)` y el estado podía crecer con cada intervalo redundante.

Los dominios comparables siguen siendo exclusivamente:

- literal integral distinto de `INT64_MAX`, representado como `[value, value + 1)`;
- rango integral con ambos límites presentes y `begin < end`, representado como `[begin, end)`;
- árboles `Or` compuestos íntegramente por los dos casos anteriores.

Si aparece cualquier otra forma, el brazo completo permanece fuera del índice, igual que antes.

## Diseño aplicado

`MatchParseState` conserva un `std::map<int64_t, int64_t>` cuyas entradas son intervalos disjuntos
y ordenados por inicio. La ruta por brazo ejecuta:

1. recolección iterativa histórica del dominio comparable;
2. ordenación por `(inicio, fin)`;
3. normalización lineal del brazo, detectando overlap estricto antes de coalescer overlap o
   adyacencia;
4. consulta de cada intervalo normalizado mediante `lower_bound`, su sucesor y su predecesor;
5. publicación sólo para guards incondicionales, fusionando vecinos solapados o adyacentes.

La adyacencia se coalesce para compactar estado, pero no se diagnostica: la consulta conserva la
comparación half-open estricta `begin < otherEnd && otherBegin < end`. Cada entrada se inserta o se
elimina del mapa un número acotado de veces; el costo agregado es `O(I log I)` y el espacio es
`O(U)`, donde `U` es el número de componentes disjuntos de la unión publicada.

## Invariantes preservados

- Un overlap dentro de un mismo `Or` produce un único diagnóstico en el rango del brazo.
- Un overlap con cualquier brazo incondicional previo produce el mismo diagnóstico.
- Un brazo guarded consulta el índice anterior, pero nunca publica su dominio.
- Un brazo `when true` se trata como incondicional; `when false` y guards no constantes no
  publican.
- Rangos adyacentes no se solapan.
- Los dominios mixtos no consultan ni publican intervalos parciales.
- `INT64_MAX` no se convierte en `[max,max+1)` y conserva el comportamiento histórico sin
  overflow.
- El orden de diagnósticos respecto a guard falso y catch-all permanece inalterado.

## Contrafactual TP-PAR-550

El caso ejecuta cuatro procesos aislados:

1. 32,768 brazos literales disjuntos y un duplicado final: demuestra consulta indexada y un único
   diagnóstico sin el costo cuadrático histórico.
2. Dos rangos solapados dentro de un mismo `Or`: preserva overlap intra-arm.
3. Rangos adyacentes y brazos guarded: preserva no-overlap por adyacencia, consulta guarded y
   publicación exclusiva de brazos incondicionales.
4. Un dominio mixto y literales `INT64_MAX`: demuestra que no se publica un subconjunto parcial ni
   se desborda el límite superior.

La infraestructura de la misma tanda incluye fuente Host.Parser, CMake suite, declaración,
registro default, diseño, asignación y manifest canónico.

## Evidencia de cierre

La validación se ejecutó de forma focal, sin baseline ni batería completa:

- build de `nsc`: PASS; log `A:\temp\r02b5d2-build-nsc.log`;
- build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; log `A:\temp\r02b5d2-build-suite.log`;
- `TP-PAR-550` aislado: PASS, cuatro procesos y 32,768 brazos en el contrafactual indexado;
  duración reportada 522,150 microsegundos; log `A:\temp\r02b5d2-tp-par-550.log`;
- Host.Parser completo: PASS 550/550; log `A:\temp\r02b5d2-host-parser.log`;
- TL-LANG-311/312/313/315/322: PASS 5/5; log `A:\temp\r02b5d2-tl-lang.log`;
- build de los cuatro targets T03 focales: PASS; log `A:\temp\r02b5d2-build-t03.log`;
- CTests `fundamental-patterns`, `exhaustive-match`, `conditional-patterns` y
  `pattern-guards`: PASS 4/4; log `A:\temp\r02b5d2-ctest-t03.log`.

El PATH de cada proceso se reconstruyó desde Machine+User y `nslink.exe` se resolvió por nombre
exactamente a `A:\nslink-devel\bin\release\nslink.exe`. Los CTests utilizaron el runtime generado
en `A:\nsc-devel\build-test-infrastructure`.

Segunda pasada estática:

- `validate_source_registration.py`: PASS;
- `CanonicalCatalog.load()`: PASS, 6,005 casos, 30 componentes, 682 contratos y TP-PAR-550
  presente;
- registro único del módulo en CMake, `TestRegistry` y ruta default;
- estado anterior vectorial retirado; consulta e inserción pasan exclusivamente por el índice
  ordenado;
- funciones nuevas o modificadas por debajo de 60 líneas;
- `diff --check`: PASS; los avisos de fin de línea del worktree no son errores de whitespace.
