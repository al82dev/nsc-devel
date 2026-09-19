# NativeSharp 2.0 self-hosting — tanda 07: símbolos, tipos y semántica

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-067` a `NS2-SH-083` |
| Gate | `SH06` |
| Rama | `development` |
| Commit de entrada | `fb43d9005` |
| Commit de cierre | este commit (`selfhost(tanda-07): port semantic model and binding`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-07-sh06-final-r2-20260919`, PASS, 617.004 s |

## 2. Objetivo y alcance

### Objetivo

Convertir el árbol canónico de SH05 en un modelo semántico autocontenido, estable y consultable sin
llamar al frontend C++. La entrega crea tablas de tipos, símbolos y scopes; liga declaraciones y
expresiones; calcula constantes y layout Win64; y publica dumps diferenciales que separan la forma
sintáctica del significado que consumirán HIR y los metadatos de proyecto.

### Incluido

- tipos primitivos Win64, named, type parameters, pointers, arreglos con extensión exacta,
  funciones y calificadores `owned`/`borrowed`;
- namespaces, tipos nominales, miembros, parámetros, variables y scopes léxicos;
- nombres calificados, relación padre/miembro y conjuntos de overloads conservados por orden;
- flags `const`, `static`, `virtual`, `override`, definición, `extern C`, genéricos y constraints;
- hechos por nodo: tipo, categoría de valor, modificabilidad, dirección, bounds check, símbolo,
  constante, operador y conversiones;
- promoción numérica, casts, asignaciones, acceso a miembro, llamadas, índices, memoria explícita,
  `sizeof`, `alignof`, condicionales y lambdas;
- clases, structs, interfaces, enums, unions, bitfields, eventos, delegates y lambdas;
- layout secuencial, union superpuesta, `pack`, `align`, `size` y ubicación explícita `at`;
- envolvente determinista de metadatos semánticos para tipos/símbolos públicos;
- dumps `--dump-symbols`, `--dump-sema` y `--dump-semantic-metadata`;
- igualdad exacta con `nsc0` en la matriz canónica y estabilidad O0/O2 sobre todo el corpus;
- gate CMake/CTest, contrato JSON y reporte estructurado promovible.

### Frontera deliberada

SH06 cierra la representación y el binding necesarios para comenzar HIR. No sustituye aún el
pipeline de proyecto de `nsc0`: la lectura/escritura binaria de `.nsmeta`, la materialización de
especializaciones entre módulos y los diagnósticos semánticos exhaustivos se conectarán cuando
existan el grafo de proyecto y el formato de metadata autocontenido. El modelo sí conserva desde
ahora identidad, constraints, layout, ownership, conversiones y símbolos suficientes para que esas
fases no tengan que reinterpretar texto sintáctico.

## 3. Diseño

### Arquitectura

```text
SyntaxTree congelado
  -> pase de declaraciones
       -> TypeTable interning
       -> Symbol table + árbol padre/miembros
       -> scopes globales y nominales
  -> pase de cuerpos
       -> scopes locales
       -> binding de nombres
       -> tipos/categorías/conversiones/constantes por nodo
  -> cálculo de layout ABI
  -> SemanticModel inmutable por consumo
       -> SemanticDumper
       -> SH07 lowering a HIR
       -> envolvente semantic-metadata
```

El analizador usa dos pasadas. La primera declara todos los símbolos, crea identidades nominales,
registra firmas y calcula layouts; así un cuerpo puede mencionar una declaración posterior. La
segunda recorre inicializadores y cuerpos con scopes anidados, liga cada uso y coloca un
`SemanticFact` en el mismo ID que el nodo sintáctico. No se añaden punteros cruzados al AST: los
enlaces son IDs estables dentro de `SemanticModel`.

`TypeTable` aplica interning por representación canónica. Los IDs son densos y deterministas para
un mismo archivo. `SemanticType` guarda forma, elemento/retorno/parámetros, extensión, tamaño,
alineación y ownership. `Symbol` mantiene identidad, nombre calificado, tipo, declaración, padre,
miembros y propiedades ABI. Los scopes almacenan nombres y símbolos en orden de declaración;
`LookupLocal` busca desde el último, lo que preserva shadowing y los conjuntos repetidos permiten
conservar overloads sin destruir candidatos.

### Layout Windows x64

Los primitivos se fijan con tamaño/alineación del target Win64. En structs y clases, cada field se
alinea con `min(alignment, pack)` y avanza el cursor; `at(n)` fija un offset explícito. En unions,
todos los fields empiezan en cero y el tamaño es el máximo. `align(n)` eleva la alineación final y
`size(n)` impone un mínimo. Los arreglos con extensión constante multiplican el tamaño del elemento;
los no acotados conservan extensión cero. Los bitfields registran ancho y desplazamiento de bit.

### Contratos e invariantes

- cada tipo textual canónico tiene un único ID dentro del modelo;
- los tipos built-in tienen IDs y layouts deterministas;
- todo símbolo tiene declaración válida, padre `-1` o símbolo existente y nombre calificado estable;
- el orden de globals, miembros, parámetros y conversiones es el orden fuente;
- cada nodo tiene exactamente un `SemanticFact`; `Present=false` significa ausencia observable;
- un lvalue modificable siempre es addressable; constantes y temporales son rvalues;
- las conversiones conservan origen, destino, papel y número de operando;
- una union superpone fields; structs/clases no reducen offsets salvo `at` explícito;
- O0 y O2 producen mismo exit code, stdout y stderr para todo el corpus;
- los casos canónicos producen stdout idéntico al de `nsc0`;
- el dumper de metadata no serializa direcciones ni estado dependiente del proceso;
- sólo CRLF/CR se normaliza a LF en el comparador.

### Decisiones y alternativas

| Decisión | Alternativa | Razón |
|---|---|---|
| IDs densos para tipos/símbolos/hechos | punteros C++ equivalentes | ownership claro y serialización determinista |
| dos pasadas | resolver mientras se parsea | referencias adelantadas y separación SH05/SH06 |
| interning por nombre canónico | duplicar tipos por uso | identidad estable y comparación barata |
| hechos paralelos al AST | decorar/mutar nodos congelados | mantiene el contrato de inmutabilidad SH05 |
| strings de constraints preservados | descartarlos hasta proyectos | permite validar y exportar sin reparsing futuro |
| metadata textual de contrato | emitir `.nsmeta` binario ya | congela contenido antes de congelar transporte |
| diferencial contra stdout real | golden files manuales | detecta deriva del port y del oráculo |
| estabilidad corpus completo | exigir que toda unidad aislada sea válida | muchas fuentes requieren su proyecto/referencias |

## 4. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/Types/TypeSystem.ns` | crear | kinds, layouts, ownership e interning de tipos |
| `selfhost/nsc/Symbols/Symbols.ns` | crear | símbolos, flags, nombres calificados, miembros y scopes |
| `selfhost/nsc/Sema/SemanticModel.ns` | crear | facts, conversiones y propietario del modelo |
| `selfhost/nsc/Sema/SemanticAnalyzer.ns` | crear | declaración, binding, expresiones, flow y layout |
| `selfhost/nsc/Sema/SemanticDumper.ns` | crear | dumps canónicos de símbolos, semántica y metadata |
| `selfhost/nsc/Syntax/SyntaxTree.ns` | modificar | texto semántico oculto y modifiers sin cambiar AST |
| `selfhost/nsc/Parser/Parser.ns` | modificar | extensiones exactas, lambdas, events, union/layout/bitfields |
| `selfhost/nsc/Driver/Program.ns` | modificar | identidad SH06 y nuevas opciones de dump |
| `selfhost/nsc/NativeSharp.Compiler.nsproj` | modificar | incluir Types/Symbols/Sema |
| `selfhost/contracts/semantic.contract.json` | crear | matriz diferencial, corpus e invariantes SH06 |
| `selfhost/bootstrap/verify_semantics.py` | crear | comparación nsc0/O0/O2 y reporte estructurado |
| `cmake/VerifyNativeSharpSemantics.cmake` | crear | build/PE/gate O0 y O2 |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.semantics` |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | congelar schema/matriz/newlines SH06 |
| roadmaps y READMEs | modificar | navegación y nueva frontera alcanzada |
| este expediente y evidencia SH06 | crear | diseño, resultados y hashes de cierre |

## 5. Tipos y datos

| Tipo | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|
| `SemanticTypeKind` | taxonomía de tipos | enum estable dentro de schema 1 | valor |
| `SemanticType` | forma y ABI de un tipo | display canónico; layout no negativo | propiedad de `TypeTable` |
| `TypeTable` | interning y consultas | un ID por display; built-ins iniciales | propiedad del modelo |
| `SymbolKind` | taxonomía de declaraciones | nombre observable por `SymbolFacts` | valor |
| `Symbol` | identidad y atributos ligados | padre/miembros por ID; orden fuente | propiedad del modelo |
| `Scope` | nombres visibles | listas nombre/símbolo sincronizadas | propiedad del modelo |
| `SemanticConversion` | ajuste implícito/explícito | origen/destino válidos | propiedad del fact |
| `SemanticFact` | significado de un nodo | índice igual al nodo de SH05 | propiedad del modelo |
| `SemanticModel` | agregado semántico | tablas y arrays densos coherentes | resultado del analyzer |
| contrato/reporte SH06 | norma y evidencia | schema/version 1, status passed | archivos JSON |

## 6. Métodos y funciones

| Firma/grupo | Responsabilidad | Postcondición/errores |
|---|---|---|
| `TypeTable.Intern` | internar pointer/array/ownership/named | devuelve ID existente o nuevo determinista |
| `TypeTable.Function` | crear firma `(args) -> result` | parámetros y retorno preservados |
| `TypeTable.StringLiteral` | tipo literal con longitud incluida | tamaño exacto, alineación 1 |
| `TypeTable.IsInteger/IsNumeric` | clasificación para promociones | no depende de texto del AST |
| `Scope.Declare/LookupLocal` | alta y lookup con shadowing | último símbolo homónimo gana localmente |
| `SemanticModel.BindNode` | enlazar declaración/uso a símbolo | ID consultable por lowering |
| `SemanticFact.SetType/SetStorage` | categoría y propiedades de valor | activa el fact de forma explícita |
| `SemanticFact.SetOperator` | tipo de operación y resultado | operador y ambos tipos quedan juntos |
| `SemanticFact.AddConversion` | registrar coerción | conserva papel/índice del operando |
| `SemanticAnalyzer.DeclareGlobalNode` | pase global/namespace/extern | todos los globals visibles antes de cuerpos |
| `SemanticAnalyzer.DeclareAggregate` | tipos, members y layout | árbol nominal y layout deterministas |
| `SemanticAnalyzer.DeclareFunction` | firma, parámetros y modifiers | símbolo callable completo |
| `SemanticAnalyzer.AnalyzeExpression` | despacho de expresiones | fact tipado o tipo `error` estable |
| `SemanticAnalyzer.AnalyzeStatement` | scopes y control de flujo | visita inicializadores/condiciones/cuerpos |
| `SemanticAnalyzer.AnalyzeFunction` | scope de parámetros y body | restaura scope/función al terminar |
| `SemanticAnalyzer.Analyze` | orquestar las dos pasadas | entrega `SemanticModel` autocontenido |
| `SemanticDumper.DumpSymbols` | árbol de símbolos compatible | preorder y formato idéntico a `nsc0` |
| `SemanticDumper.Dump` | hechos semánticos por AST | incluye operadores y conversiones |
| `SemanticDumper.DumpMetadata` | envolvente ABI determinista | schema 1, target y tablas completas |
| `verify_semantics.verify` | diferencial + estabilidad + invariantes | reporte o fallo con archivo/acción/optimización |

## 7. Trazabilidad de `NS2-SH-067` a `NS2-SH-083`

| ID | Implementación/contrato observable |
|---|---|
| 067 | `Scope`, namespaces, qualified names y binding léxico/global |
| 068 | símbolos nominales padre/miembro y tipos named/anidados |
| 069 | built-ins, pointer, array exacto, function y ownership-qualified |
| 070 | símbolos class/struct/interface/enum/union, layout y bitfields |
| 071 | members y flags de accesibilidad sintáctica, static/virtual/override |
| 072 | conjuntos homónimos preservados, binding de callables y conversiones ordenadas |
| 073 | parámetros/firma genéricos preservados para sustitución y especialización |
| 074 | constraints canónicos en tipos y métodos; invariantes de `c44` |
| 075 | promoción numérica, casts, constantes y folding escalar |
| 076 | scopes de statement, categorías, asignabilidad y recorrido de control |
| 077 | ownership en tipos, lvalue/addressable y operaciones new/delete/stackalloc |
| 078 | lambda facts/scopes, delegates y event symbols |
| 079 | recorrido semántico de try/catch/finally/throw y flujo no local |
| 080 | tamaños, alineación, pack/align/size/at, union y firma `extern C` |
| 081 | contenido exportable congelado por `--dump-semantic-metadata`; transporte binario posterior |
| 082 | tres dumps canónicos y comparador O0/O2/nsc0 |
| 083 | fixtures de constraints, operators/layout, asm, lambdas y unions sin divergencia |

La tabla distingue representación semántica cerrada de validación de programa completo. El ranking
exhaustivo de overloads genéricos, definite assignment interprocedural, préstamos/escape entre
módulos y lectura binaria de `.nsmeta` requieren el grafo de proyectos y se mantienen como trabajo
de integración de las fases SH12/SH13; no se fingirán como diagnósticos ya disponibles.

## 8. Superficie observable

- `nsc-next --dump-symbols <file.ns>`;
- `nsc-next --dump-sema <file.ns>`;
- `nsc-next --dump-semantic-metadata <file.ns>`;
- identidad `nsc-next 2.0.0-bootstrap.sh06`;
- exit 0 para unidad semántica aceptada y 1 si lexer/parser/sema registra errores;
- CTest `nsc.selfhost.semantics`;
- schemas `nativesharp-selfhost-semantic-contract` y `nativesharp-selfhost-sh06-report`, v1;
- cabecera de metadata `semantic-metadata schema=1 target=windows-x64`.

## 9. Implementación

El trabajo empezó con tipos y símbolos mínimos capaces de reproducir exactamente las doce fuentes
de integración de `nsc0`. Los dumps se usaron como especificación ejecutable: cada nodo ligado,
conversión y orden de símbolo tuvo que coincidir tanto con un candidato O0 como O2. Después se
extendió el parser sin cambiar su AST público: `SemanticTypeText` conserva extensiones de array
reales donde SH05 debe seguir mostrando `[expr]`, y `ModifiersText` conserva atributos necesarios
para semántica sin alterar el dumper sintáctico.

La segunda ampliación cubrió construcciones que la matriz antigua no fijaba: constraints y
overloads (`c44`), operators/layout de arrays (`c6`), firmas de ensamblador, lambdas y unions. Para
unions se añadió captura de `pack`, `align`, `size`, `at` y bitfields, seguida de una envolvente de
metadata que hace observable el layout. Esto encontró y corrigió una regresión real: al almacenar
el tipo base únicamente como dato semántico desaparecía de `--dump-ast`; el parser ahora mantiene
el display legado y el tipo semántico por separado.

El corpus completo no presupone que cada `.ns` pueda compilar aisladamente. El gate exige que O0 y
O2 tengan exactamente el mismo código de salida, stdout y stderr para cada acción; adicionalmente,
la matriz de oráculo exige exit 0, stderr vacío e igualdad byte a byte contra `nsc0`.

## 10. Pruebas específicas previas al baseline

| Prueba | Tipo | Propósito | Resultado |
|---|---|---|---|
| compilación O0/O2 | positiva | modelo válido en ambos niveles | PASS ×2 |
| frontend SH05 | regresión | lexer/parser/AST no cambian | PASS: 482 fuentes, 1,006 comparaciones |
| símbolos/sema integración | diferencial | igualdad exacta con `nsc0` | PASS: 48 comparaciones |
| corpus semántico | estabilidad | O0/O2 no divergen ni caen | PASS: 483 fuentes, 966 comparaciones |
| invariantes ampliadas | contractual | constraints/layout/asm/lambda/union | PASS: 28 checks |
| pruebas Python | infraestructura | schemas, matrices, hashes/newlines | PASS: 30/30 |

## 11. Baseline de cierre

El baseline canónico se ejecutó después del último cambio funcional y de este diseño:

```text
python -B nscbuilder.py --baseline \
  --config selfhost/bootstrap/bootstrap.config.example.json \
  --run-id tanda-07-sh06-final-r2-20260919 --jobs 16
```

| Dato | Resultado |
|---|---|
| Estado | PASS, 0 fallos; Gate 0 `LANDED` |
| Inicio / fin | 2026-09-19 16:10:59 / 16:21:16, UTC-06:00 |
| Duración total | 617.004 s |
| CTest `lld-link` | 443/443 PASS, 259.738 s |
| Fixtures `lld-link` | 148/148 PASS, 55.838 s |
| Triage `lld-link` | 18/18 PASS, 8.588 s |
| CTest NSLink | 443/443 PASS, 195.993 s |
| Fixtures NSLink | 148/148 PASS, 48.940 s |
| Triage NSLink | 18/18 PASS, 9.366 s |
| Gate 0 | LANDED, 9.828 s |
| Fuente auditada | 1,402 archivos; SHA-256 `a3e001fcc6c94722a2706dd33dc167da90b5979d33321fa98ddab3622bb611b1` |
| `nsc0` construido/instalado | SHA-256 `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561` |
| Reporte Markdown | SHA-256 `d043a183ac9aa7fcfde7889434182220e086c34884d78c351cc81b942f0c0f2b` |
| Reporte JSON | SHA-256 `86e119d12d6b687c3b8f5e95bffee3234de3cc936d6ccf9d6ebaf03bd523c3a6` |
| Gate G00 | SHA-256 `2b86479a3cf70beb3df361bf5c9535d995fef3e9639be1d0f62d32fc77e41f11` |
| Reporte SH06 promovido | SHA-256 `b7dbf4c3e06758750327f7196df955860eac76c36d85f334de8222d663727818` |
| Candidato O0 | SHA-256 `a661e38ccfe2f1dcdf0ca80a0418080fc9e1864f7501cefc32ff03facaf0cc57` |
| Candidato O2 | SHA-256 `5b002366fd2988c0a43b9d1986c62135f443595adbfc63dece6746dc83324021` |

Una primera corrida, `tanda-07-sh06-final-20260919`, registró correctamente `FAILED` porque el
proceso CTest de NSLink terminó de forma transitoria con status Windows `0xffffffff` después de 115
tests, sin fallo de caso. La matriz se reprodujo completa de forma aislada y el baseline se repitió
desde cero. Sólo la segunda corrida, íntegra y con Gate 0 `LANDED`, constituye evidencia de cierre.

## 12. Riesgos, limitaciones y deuda explícita

- La resolución actual liga el símbolo visible y preserva overloads, pero el ranking genérico
  exhaustivo y diagnósticos de ambigüedad se validarán con compilación de proyectos en SH12/SH13.
- El análisis recorre control y scopes; no emite todavía todos los diagnósticos de definite
  assignment, escape, borrow o flujo excepcional que ofrece `nsc0` en un build completo.
- La envolvente semantic-metadata congela contenido y determinismo, no el transporte `.nsmeta`
  binario existente; import/export real requiere project graph y loader.
- Calling conventions se conservan en símbolos/firma `extern C`; su lowering físico se verifica en
  HIR/MIR/LLVM y por las suites ABI posteriores.
- Los tipos named empiezan con layout desconocido hasta que su declaración se procesa; consumidores
  deben usar el layout final del modelo y no cachear el valor provisional.
- `nsc0` sigue siendo el oráculo temporal; SH15 elimina esa dependencia del build reproducible.

## 13. Contenido del commit

### Higiene

- [x] Artefactos O0/O2 y reportes temporales viven bajo `selfhost/out` ignorado.
- [x] No se versiona ningún `.obj`, `.exe`, `.dll`, `.lib`, PDB ni caché.
- [x] No se preparan los cambios locales ajenos enumerados al abrir la tanda.
- [x] Archivos, tipos, métodos, decisiones, fronteras y deuda están documentados.
- [x] Baseline completo posterior al último cambio funcional/documental.
- [x] Evidencia SH06 promovida con hashes finales.

### Commit de cierre

```text
selfhost(tanda-07): port semantic model and binding
```

## 14. Criterio de cierre

- [x] `NS2-SH-067` a `NS2-SH-083` tienen representación y contrato observable SH06.
- [x] Compilación O0/O2 y gates directos SH05/SH06 verdes.
- [x] Diferencial de símbolos/sema y estabilidad del corpus verdes.
- [x] Limitaciones de proyecto/metadata/diagnósticos se documentan sin ocultarlas.
- [x] Baseline completo verde en lld-link y NSLink.
- [x] Gate SH06 promovido y commit exacto creado.
