# NSC 3.0.5 — R02B2 clasificación, expansión y collectors lineales de patterns

## SCOPE LOCK

- Objetivo: sustituir exclusivamente cuatro recorridos read-only de `Parser::Pattern` por
  máquinas iterativas y eliminar el lookup cuadrático de bindings únicos.
- Naturaleza: refactor algorítmico interno, sin cambios de lenguaje ni contratos.
- Símbolos autorizados: `patternHasBindings`, `patternIsConditionOnly`,
  `expandPatternAlternatives`, `collectUniquePatternBindings` y el helper privado necesario para
  la expansión.
- Comportamiento inmutable: clasificación, short-circuit, orden fuente, orden cartesiano,
  topología, campos escalares, rangos, punteros AST, `fieldNames`, `restIndex`, modos de binding y
  política first-seen/first-mode.
- Archivos productivos autorizados: `include/nativesharp/Parser/Parser.h` y
  `src/Parser/PatternParser.cpp`.
- Protegido: gramática R02B1; construcción de condiciones R02B3; lowering R02B4; emisión de
  bindings y rutas match/conditional R02B5; AST público; diagnósticos; recovery; ABI; NSMETA;
  HIR; MIR; Driver y publicación de artefactos.
- Validación requerida para cierre: TP-PAR-543, regresión de patterns aplicable, build focal,
  registro de fuente, catálogo y diferencial limpio. Esta primera preparación no ejecuta builds
  ni pruebas mientras D04/M02L conservan la ventana compartida.

## Causa y frontera

La auditoría separó cuatro recorridos independientes que permanecieron recursivos después de
R02B1:

1. `patternHasBindings` hacía DFS recursivo con short-circuit;
2. `patternIsConditionOnly` hacía DFS recursivo con short-circuit;
3. `expandPatternAlternatives` hacía postorden recursivo y copiaba el subárbol completo antes de
   borrar `children`, por lo que una cadena sin `Or` acumulaba costo cuadrático;
4. `collectUniquePatternBindings` hacía DFS recursivo y buscaba cada nombre con `none_of` sobre el
   vector acumulado, con costo `O(B²)`.

Las rutas de match y conditional poseen collectors distintos y permanecen deliberadamente fuera
de esta tanda. Se cerrarán en R02B5; moverlas aquí mezclaría dos hipótesis y ocultaría pendientes.

## Diseño iterativo

### Clasificadores

Los dos clasificadores usan una worklist LIFO de punteros `const Pattern*`. Los hijos se insertan
en orden inverso para visitar en el mismo orden fuente que la recursión histórica. La primera
coincidencia conserva el retorno inmediato:

- `patternHasBindings` retorna `true` ante el primer `Binding`;
- `patternIsConditionOnly` retorna `false` ante el primer `Binding`, `Tuple` o `List`.

Su complejidad es `O(N)` tiempo y `O(ancho/profundidad)` heap, sin consumir pila nativa por nivel.

### Expansión de alternativas

`PatternAlternativeExpander` ejecuta postorden mediante frames heap-backed. Cada frame conserva:

- el nodo fuente;
- el siguiente hijo pendiente;
- las alternativas parciales del nodo.

Un frame `Or` concatena resultados hijos en orden fuente. Un frame no-`Or` crea una copia
superficial explícita de todos los campos escalares y distribuye cada hijo sobre los prefijos ya
construidos. El caso común uno-por-uno mueve el hijo completo dentro del único prefijo, sin copiar
la cadena acumulada. Así, una cadena sin alternativas pasa de `O(N²)` a `O(N)`. Cuando existe
producto cartesiano, las copias restantes son proporcionales a la salida que el contrato exige.

El orden permanece exactamente:

```text
por cada prefijo en orden fuente
  por cada alternativa del hijo en orden fuente
```

La copia superficial preserva `kind`, `bindingMode`, `name`, `range`, `firstValue`, `secondValue`,
`firstInteger`, `secondInteger`, `restIndex` y `fieldNames`; sólo `children` se reconstruye.

### Collector de bindings

El collector inicializa un `unordered_set` con los nombres que el consumidor ya proporcionó y
recorre el árbol iterativamente en DFS fuente. Cada `Binding` se procesa como hoja, igual que el
retorno histórico. El primer nombre conserva su modo; las repeticiones posteriores se ignoran.
El costo esperado pasa de `O(N+B²)` a `O(N+B)`.

## Contrafactual TP-PAR-543

El módulo `ParserPatternTraversalCounterfactualModule.cpp` prepara una ejecución aislada con:

- clasificadores sobre cadenas de profundidad 32,768, tanto positivas como negativas;
- expansión uno-por-uno de una cadena de profundidad 32,768;
- producto cartesiano pequeño que exige el orden `AX, AY, BX, BY` y preservación de escalares;
- collector sobre una cadena de profundidad 32,768;
- collector ancho con 32,768 nombres únicos y una segunda aparición de cada nombre, incluyendo un
  binding preexistente cuyo modo no puede reemplazarse.

Los valores recursivos profundos se dejan deliberadamente al proceso hijo hasta su salida. Su
destrucción probaría la recursión interna de `std::vector<Pattern>`, no los cuatro algoritmos que
esta tanda modifica.

## Infraestructura completa de la misma tanda

TP-PAR-543 queda registrado, en un único cambio coherente, en:

- `tests/compiler-suite/CMakeLists.txt`;
- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`;
- `tests/compiler-suite/host/src/main.cpp`;
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`;
- `docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv`;
- `tests/compiler-suite/manifest/cases.jsonl`;
- catálogo/Builder mediante los validadores canónicos.

El target previsto es `nsc_component_tests`; `PatternParser.cpp` debe seguir entrando a la suite
únicamente mediante el shim productivo existente.

`validate_source_registration.py` terminó con exit `0`. `CanonicalCatalog` cargó 5,998 casos,
30 componentes y 682 contratos, y descubrió TP-PAR-543 como
`Host.Parser/NIGHTLY/nsc_component_tests`.

## Estado

- Producción: implementada estáticamente.
- TP-PAR-543: fuente, infraestructura y ejecución completas.
- Build focal `nsc`: PASS, log `A:\temp\r02b2-build-nsc.log`.
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, log
  `A:\temp\r02b2-build-suite-three-targets.log`.
- TP-PAR-543: PASS 1/1, exit `0`, 897,057 us, log
  `A:\temp\r02b2-tp-par-543.log`.
- Regresión TP-PAR-349..410 + TP-PAR-525..540: PASS 78/78, exit `0`, log
  `A:\temp\r02b2-host-parser-78.log`.
- `validate_source_registration.py`: PASS, exit `0`, log
  `A:\temp\r02b2-source-registration.log`.
- `CanonicalCatalog`: PASS, 5,998 casos, 30 componentes y 682 contratos; log
  `A:\temp\r02b2-canonical-catalog.log`.
- Gate conservador de tamaño: producción máximo 24 líneas físicas/14 statements aproximados en
  `appendChildAlternatives`; contrafactual máximo 29/26 en `testWideCollector`; todas las funciones
  quedan por debajo de 60.
- Tracking externo: 56→55 hojas pendientes; M02M permanece abierto y sin cambios.
- Índice: auditado con exactamente las diez rutas R02B2; M02M y la prueba ajena quedaron fuera.
- Cierre técnico: completo en un commit local aislado después de todos los gates.
