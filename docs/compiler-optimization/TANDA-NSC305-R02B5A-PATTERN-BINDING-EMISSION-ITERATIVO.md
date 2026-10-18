# Tanda NSC 3.0.5 · R02B5A · Emisión iterativa de bindings de patterns

## Objetivo

Eliminar la recursión directa e indirecta de la emisión de bindings de patterns sin cambiar
gramática, AST observable, orden de declaraciones, nombres temporales, modos de binding,
diagnósticos, recovery, ABI ni NSMETA.

## Alcance productivo

- `Parser::emitPatternBindings` conserva su contrato y queda como fachada.
- `Parser::PatternBindingEmitter` ejecuta una pila LIFO explícita para bindings simples,
  tuplas, patterns posicionales, estructuras, listas, variantes y alternativas `or`.
- Los accesos anidados se representan como `SourcePlan`; su evaluación usa otra pila
  explícita y conserva el número y el orden histórico de invocaciones del factory.
- Los accesos `list` anteriores al rest evalúan una sola vez su origen; rest y accesos
  desde el final lo evalúan dos veces, igual que el código sustituido.
- Las alternativas `or` conservan expansión, orden de arms, selección por binding,
  condición, auxiliares de referencia e incremento monotónico de temporales.
- Los patterns posicionales conservan la construcción de declaraciones antes del guard y
  la composición izquierda-a-derecha de las condiciones de sus hijos.

## Exclusiones

R02B5A no modifica el análisis de match expressions (R02B5B), conditional patterns
(R02B5C), match statements (R02B5D), sintaxis, recovery, HIR, MIR, ABI ni NSMETA.

## Contrafactual TP-PAR-546

La prueba `TP-PAR-546` registra cuatro procesos aislados:

1. source plan de estructura a 32,768 niveles;
2. emisión de deconstrucciones tuple a 32,768 niveles;
3. emisión de payloads variant a 32,768 niveles;
4. paridad de list rest/from-end y arms `or`, incluido el número exacto de source calls.

La cadena queda declarada en CMake de `compiler-suite`, `TestRegistry`, `main`, diseño,
asignación y manifest. El caso pertenece al target y ruta default `nsc_component_tests`.

## Evidencia de validación

- Build `nsc`: PASS (`A:\temp\r02b5a-build-nsc.log`).
- Build `nsc_component_tests`: PASS (`A:\temp\r02b5a-build-suite.log`).
- Build `nsc_component_tests_destructive` y `nsc_component_tests_source_extended`: PASS
  (`A:\temp\r02b5a-build-suite-auxiliary.log`).
- `TP-PAR-546`: PASS, cuatro probes aislados (`A:\temp\r02b5a-tp-par-546.log`).
- `Host.Parser`: PASS 546/546; incluye TP-PAR-543, 544, 545 y 546
  (`A:\temp\r02b5a-host-parser.log`).
- TL-LANG-264…269: PASS 6/6 (logs `A:\temp\r02b5a-tl-lang-*.log`).
- TL-LANG-308…313, 315, 322 y 357…366: PASS 18/18; cubren list bindings,
  rest/from-end, modos copy/move/ref/ref-mut, positional bindings y vistas prestadas.
- Registro estático: PASS; TP-PAR-546 aparece una vez en diseño, asignación y manifest, y su
  módulo está conectado a CMake, `TestRegistry` y `main`.

## Renovación del entorno focal

El proceso inicial había heredado un PATH anterior a la instalación de NSLink. Se reconstruyó
solamente el PATH del proceso con los valores persistentes Machine + User, sin modificarlos.
`where.exe nslink.exe` resolvió exactamente `A:\nslink-devel\bin\release\nslink.exe`; los gates
se ejecutaron después por nombre (`NSC_NSLINK_EXECUTABLE=nslink.exe`), nunca mediante una ruta
inyectada en la configuración. El TL-LANG-308 inicial documentó el bloqueo ambiental y la
repetición posterior pasó junto con los otros 17 focales externos.
