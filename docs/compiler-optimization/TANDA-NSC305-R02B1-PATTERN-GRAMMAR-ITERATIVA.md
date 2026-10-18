# NSC 3.0.5 — R02B1 gramática y delimitadores de patterns iterativos

## Propósito

R02B1 elimina la recursión de pila nativa del reconocimiento sintáctico de patterns. La gramática
pública, el cursor, los rangos, el recovery, los diagnósticos y la topología de `Parser::Pattern`
permanecen invariantes. La tanda no modifica clasificación, expansión, construcción de condiciones,
lowering ni emisión de bindings: esas cuatro familias quedaron visibles como R02B2–R02B5.

## Causalidad del inventario

La casilla histórica `R02B · pattern parsing iterativo` ocultaba cinco call graphs independientes:

1. gramática y delimitadores;
2. clasificación, expansión y collectors;
3. construcción de condiciones;
4. lowering;
5. emisión de bindings y rutas match/conditional.

El tracking sustituyó el padre por cinco hojas absolutas, sin conservar un checkbox contable para el
padre. El ajuste mecánico fue 50→54. R02B1 cubre exclusivamente la primera hoja.

## Antes

El ciclo recursivo era:

```text
parsePattern
  -> parseAndPattern
    -> parsePatternPrimary
      -> parsePatternPrimary                 (!)
      -> parseListPattern -> parsePattern     ([...])
      -> parseParenthesizedPattern -> parsePattern
      -> parseStructPatternBody -> parsePattern
      -> parseNamedPattern -> parsePattern    (Variant(...) / Positional(...))
```

Una cadena válida de delimitadores trasladaba su profundidad directamente a la pila nativa. Los
operadores planos `&` y `|` ya usaban loops, pero sus operandos podían volver a entrar al ciclo.

## Después

`PatternIterationParser` mantiene dos estructuras heap-backed:

- una worklist LIFO de fases `ParseOr`, `ParseAnd`, `ParsePrimary` y continuaciones por delimitador;
- una pila de valores `Pattern` ya completados.

Las continuaciones materializan explícitamente:

- cierre de `!`;
- elementos y rest de listas;
- agrupación frente a tuple y trailing comma;
- campos explícitos/shorthand de structs y detección de duplicados;
- payloads Variant/Positional y su regla histórica distinta para trailing comma;
- asociación y precedencia `! > & > |`.

`parsePattern` conserva la fachada y sólo ejecuta la máquina. No quedan referencias a los antiguos
helpers recursivos `parseAndPattern`, `parsePatternPrimary`, `parseListPattern`,
`parseParenthesizedPattern`, `parseStructPatternBody` o `parseNamedPattern`.

## Contratos preservados

- `implicitBinding` cambia únicamente en las mismas fronteras históricas;
- un grupo simple devuelve el mismo hijo y expande su rango a los paréntesis exteriores;
- tuple, list y struct aceptan su trailing comma histórico;
- el payload nominal conserva el rechazo histórico de trailing comma;
- el primer `..` fija `restIndex`; los posteriores conservan diagnóstico y consumo;
- `knownTypeNames_` sigue distinguiendo Positional de Variant;
- `fieldNames` y `children` conservan alineación y orden fuente;
- tokens inesperados conservan diagnóstico, consumo salvo EOF y progreso del cursor;
- no cambian sintaxis, AST público, ABI, NSMETA, HIR, MIR ni artefactos.

## Umbral de funciones

El conteo estático local sitúa todas las funciones nuevas por debajo de 60 statements aproximados.
La mayor es `parsePrimary`, con 52 statements aproximados; las siguientes son `parseNamed` con 19,
`startStruct` y `continueList` con 18, y los finishers con 16 o menos. El gate definitivo se repetirá
con el validador establecido antes del commit.

## Contrafactual TP-PAR-542

El módulo `ParserPatternIterationCounterfactualModule.cpp` ejecuta en un proceso hijo siete entradas
válidas a profundidad 32,768:

- prefijos `!`;
- listas;
- tuples;
- structs de campo explícito;
- payloads Variant;
- payloads Positional;
- agrupación parentetizada.

El oráculo recorre el resultado iterativamente y exige cursor en EOF, cero diagnósticos, rango raíz
exacto, clase y aristas exactas, nombre nominal y campo struct exactos, y binding hoja exacto. El
valor profundo se deja deliberadamente al proceso aislado: destruir una cadena recursiva de
`vector<Pattern>` probaría el destructor del contenedor, no la recursión del parser.

## Infraestructura requerida

Antes del build focal TP-PAR-542 debe quedar registrado en la misma tanda en:

- `tests/compiler-suite/CMakeLists.txt`;
- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`;
- `tests/compiler-suite/host/src/main.cpp`;
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`;
- `docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv`;
- `tests/compiler-suite/manifest/cases.jsonl`;
- catálogo/Builder mediante los validadores canónicos.

El target sigue siendo `nsc_component_tests`. Producción conserva `PatternParser.cpp` una vez en
`nsc_frontend`; la suite lo consume mediante `PatternParserProductTestShim.cpp`, sin registrar la
fuente productiva directamente.

## Evidencia de cierre

- Build focal `nsc`: PASS; log `A:\temp\r02b1-build-nsc.log`.
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; log
  `A:\temp\r02b1-build-suite-three-targets.log`.
- TP-PAR-542 aislado a profundidad 32,768: PASS 1/1, exit 0, 20.450 s; log
  `A:\temp\r02b1-tp-par-542.log`.
- Regresión dirigida TP-PAR-349..410 + TP-PAR-525..540: PASS 78/78, exit 0; log
  `A:\temp\r02b1-host-parser-78.log`.
- Grep de los seis helpers recursivos retirados: cero referencias.
- `validate_source_registration.py`: PASS, exit 0.
- `CanonicalCatalog`: 5,997 casos, 30 componentes y 682 contratos; TP-PAR-542 descubierto como
  `Host.Parser/NIGHTLY/nsc_component_tests`.
- Auditoría estática de tamaño: máximo 52 statements aproximados en `parsePrimary`; todos los
  cuerpos nuevos quedan por debajo de 60.
- `git diff --check` sobre producción, contrafactual e infraestructura: PASS.

No se ejecutó baseline ni suite completa.
