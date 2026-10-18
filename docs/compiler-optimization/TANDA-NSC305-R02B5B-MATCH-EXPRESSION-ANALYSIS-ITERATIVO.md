# Tanda NSC 3.0.5 · R02B5B · Análisis iterativo de match expressions

## Objetivo

Eliminar la recursión y los rescans cuadráticos del análisis de patterns usado por las
expresiones `match`, sin cambiar sintaxis, recovery, AST, diagnósticos, rangos, orden de
diagnósticos, metadata de cobertura, ABI ni NSMETA.

## Alcance productivo

- `Parser::MatchExpressionPatternAnalyzer` recorre cada árbol mediante frames explícitos en
  postorden y conserva por separado el orden preorden requerido por los diagnósticos.
- Cada frame produce un resumen con bindings, catch-all, casos nominales, longitudes de lista,
  wildcard y partial.
- Los bindings se representan como un multiset exacto y canonizado por
  `(nombre, PatternBindingMode)`. La unión small-to-large evita copiar el inventario completo en
  cada ancestro y conserva duplicados.
- Cada nodo `or` compara los multisets exactos de sus alternativas una sola vez. Los fallos se
  publican después en el mismo orden preorden y con el mismo rango, id, severidad y texto.
- Los casos y longitudes de cobertura también se fusionan small-to-large. El formateador conserva
  la precedencia wildcard → longitudes → variantes, orden lexicográfico, deduplicación y marcas
  históricas `*`, `~`, `?`, `#N` y `#N+`.
- `parseMatchExpressionArm` consume un único análisis para validación de `or` e inventario de
  bindings. Cada alternativa expandida obtiene un resumen independiente para su metadata de
  cobertura, como exigía el contrato anterior.

## Complejidad

El recorrido del AST es O(N). Las uniones hash small-to-large tienen costo amortizado
O(N log U), donde U es el número de bindings distintos; la canonización ordenada final y el orden
observable de casos y
longitudes conserva su costo contractual. Ya no existe el patrón anterior de recolectar y ordenar
subárboles completos para cada `or` ancestro, que podía crecer O(N²), ni llamadas recursivas que
consumieran la pila nativa.

## Exclusiones

R02B5B no modifica emisión de bindings R02B5A, conditional patterns R02B5C, match statements
R02B5D, parser de patterns, lowering HIR/MIR, LLVM, ABI ni NSMETA.

## Contrafactual TP-PAR-547

El caso `TP-PAR-547` registra cuatro procesos aislados:

1. inventario de binding `ref` bajo 32,768 wrappers `struct`;
2. cadena de 32,768 nodos `or` con multiset exacto, catch-all y cero diagnósticos;
3. propagación catch-all y metadata `match-coverage:v1:*` a 32,768 niveles;
4. paridad superficial de diagnósticos `or` en preorden, bindings con modos distintos,
   cobertura parcial de variantes/listas y wildcard restringido por guard.

La cadena está declarada en CMake de `compiler-suite`, `TestRegistry`, `main`, diseño, asignación
y manifest. El caso pertenece al target y ruta default `nsc_component_tests`.

## Auditoría estática

- No permanecen llamadas a `matchPatternIsCatchAll`, `collectMatchPatternBindings`,
  `validateMatchOrBindings` ni `collectMatchCoverage` en la ruta de expresiones.
- Las funciones productivas y del contrafactual se mantienen por debajo de 60 líneas físicas.
- El registro estático y el catálogo canónico deben reconocer TP-PAR-547 antes de construir.
- La validación focal requerida es: build `nsc` y los tres targets de suite, TP-PAR-547 aislado,
  Host.Parser completo con énfasis TP-PAR-369…384, 499, 515 y 543…547, más los TL-LANG de
  patterns 264…267, 311…313, 315 y 322.

## Evidencia de validación

- Build `nsc`: PASS; `ExpressionParser.cpp` compiló y `nsc.exe` enlazó con contrato de runtime
  estático (`A:\temp\r02b5b-build-nsc.log`).
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS (`A:\temp\r02b5b-build-suite.log`).
- `TP-PAR-547`: PASS, cuatro probes aislados y profundidad 32,768
  (`A:\temp\r02b5b-tp-par-547.log`).
- Host.Parser: PASS 547/547, incluidos TP-PAR-369…384, 499, 515 y 543…547
  (`A:\temp\r02b5b-host-parser.log`).
- TL-LANG-264…267, 311…313, 315 y 322: PASS 9/9
  (`A:\temp\r02b5b-tl-lang.log`).
- `validate_source_registration.py`: PASS.
- `CanonicalCatalog.load()`: PASS, 6,002 casos, 30 componentes, 682 contratos y TP-PAR-547
  presente una vez.
- Auditoría final: cero símbolos recursivos sustituidos, funciones por debajo de 60 líneas,
  `git diff --check` PASS y ningún archivo MIR/LLVM/NSMETA modificado.

## Entorno focal

El PATH de cada proceso se reconstruyó sólo desde Machine + User, sin modificar los valores
persistentes. `where.exe nslink.exe` resolvió exactamente
`A:\nslink-devel\bin\release\nslink.exe`; los TL-LANG se ejecutaron con `nslink.exe` por nombre.
