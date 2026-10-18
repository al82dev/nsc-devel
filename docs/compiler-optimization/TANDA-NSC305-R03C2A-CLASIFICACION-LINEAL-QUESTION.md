# R03C2A — Clasificación indexada de `?` sin rescans

## Estado

IMPLEMENTADO Y VERIFICADO FOCALMENTE.

## SCOPE LOCK

- Objetivo: sustituir los rescans de sufijo de `questionBeginsConditional()` por una clasificación
  única O(T log T) y consultas O(1), conservando exactamente el recovery de delimitadores cruzados.
- Naturaleza: optimización interna no funcional del parser de NSC 3.0.5.
- Revisión de partida: `23a24c4c1`, cierre M02V sobre `development`.
- Archivos de producto autorizados: `Parser.h`, `ParserContext.cpp` y `ExpressionParser.cpp`.
- Infraestructura autorizada: un contrafactual Host.Parser TP-PAR-552 y todos sus registros.
- Contratos congelados: tokens consumidos, cursor, AST, rangos, diagnósticos, recovery, sintaxis,
  API, ABI y NSMETA.
- Protegido: R03C2B, R03C2C, R03C3–R03C5, R03A, R03B, MIR y trabajo ajeno.
- Prohibido: baseline, suite completa, cambios de fixtures previos y cambios funcionales.
- Cierre: build focal y TP-PAR-552/regresión Host.Parser aprobados, gates estáticos y commit aislado.

## Causa y presupuesto

El loop postfix consulta `questionBeginsConditional()` en cada token `?`. La implementación anterior
recorría desde ese token hasta su colon o delimitador. Una secuencia de N preguntas anidadas repetía
los mismos sufijos y consumía O(N²), aunque cada respuesta dependía sólo de tokens inmutables.

El presupuesto modifica únicamente:

- la construcción del parser para crear el índice;
- la consulta privada que consume el loop postfix;
- helpers locales de clasificación de delimitadores;
- TP-PAR-552 y la cadena canónica de descubrimiento.

## Algoritmo

`buildConditionalQuestionIndex()` construye índices inmutables en cuatro recorridos iterativos:

1. empareja cada familia de delimitadores independientemente, como los tres contadores anteriores;
2. calcula el cierre transitivo de intervalos que se cruzan mediante ordinals y jumps;
3. publica el siguiente evento visible a profundidad cero para cada frontera de token;
4. empareja `?` y `:` de derecha a izquierda, colapsando ternarios ya resueltos.

Esto conserva el ownership de colon de ternarios anidados. Un colon dentro de paréntesis, brackets
o braces no puede clasificar una pregunta exterior. El emparejamiento independiente y los cierres
transitivos preservan también secuencias inválidas con delimitadores incompatibles, cruzados o sin
cerrar, sin volver al rescan por pregunta.

## Complejidad e invariantes

- Construcción: O(T log T) tiempo y O(T) memoria auxiliar máxima, incluso durante recovery.
- Consulta por `?`: O(1) tiempo y O(1) memoria.
- Stack nativo: O(1); la profundidad se representa en heap.
- Cada token participa en un número acotado de recorridos y búsquedas binarias.
- Cada pregunta se clasifica una vez y cada ternario resuelto se colapsa una vez.
- El vector de roles tiene exactamente el tamaño del span inmutable de tokens.
- El índice no altera `index_`, tokens, arena ni diagnósticos.
- No existe recursión directa, indirecta ni mutua en la ruta nueva.

## TP-PAR-552

El módulo `ParserConditionalQuestionIndexCounterfactualModule.cpp` registra tres probes aislados:

- 32,768 preguntas anidadas con 32,768 colons, consultadas individualmente;
- la misma profundidad con delimitadores cruzados `([)]`, que ejercita recovery adversarial;
- mezcla de propagación postfix, ternarios, scopes delimitados y closers incompatibles.

La cadena de infraestructura incluye fuente física, CMake de compiler-suite, declaración de registro,
invocación default, design CSV, allocation CSV y manifest JSONL. El perfil es NIGHTLY y el executor
es `nsc_component_tests`.

## Evidencia de validación

La validación fue focal. No se ejecutaron baseline, deploy ni batería completa.

- build incremental de `nsc`: PASS; log `A:\temp\r03c2a-build-nsc.log`;
- build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS en la configuración canónica
  `build-compiler-suite-infra`; log `A:\temp\r03c2a-build-suite-retry.log`;
- TP-PAR-552 aislado: PASS, tres probes y profundidad 32,768 tanto válida como con
  delimitadores cruzados; 176,215 microsegundos; log `A:\temp\r03c2a-tp-par-552.log`;
- Host.Parser completo: PASS 552/552; log `A:\temp\r03c2a-host-parser.log`;
- regresiones TP-PAR-429, 439–443 y 492: PASS 7/7; log
  `A:\temp\r03c2a-parser-regressions.log`;
- `validate_source_registration.py`: PASS;
- `CanonicalCatalog.load()`: PASS, 6,007 casos, 30 componentes, 682 contratos y
  TP-PAR-552 presente;
- declaración, definición y llamada de construcción del índice: una de cada una; definición de
  consulta: una; registros CMake/header/main/design/allocation/manifest: uno de cada uno;
- funciones nuevas: entre 3 y 25 líneas ejecutables, todas por debajo del máximo de 60;
- líneas mayores a 120 columnas en los archivos R03C2A: cero;
- `git diff --check`: PASS; sólo avisos de conversión LF/CRLF propios del checkout.

El PATH de cada proceso se reconstruyó desde Machine+User y `where.exe nslink.exe` resolvió por
nombre exactamente `A:\nslink-devel\bin\release\nslink.exe`.

La auditoría diferencial estática previa comparó la clasificación nueva contra el scanner literal
retirado en 177,155 secuencias exhaustivas de hasta cinco tokens y 500,000 secuencias
pseudoaleatorias deterministas de 6 a 128 tokens: cero divergencias. Esta evidencia complementa,
pero no sustituye, las pruebas del producto anteriores.
