# Test plan — Host.Parser

## Estado

- **Clasificación:** `DISEÑO PROPUESTO`.
- **Implementación/ejecución:** ninguna.
- **Unidad lógica:** cada fila puede implementarse como tabla parametrizada; no implica ejecutable separado.
- **Oráculo mínimo:** árbol/tipo completo, rangos, flags, textos auxiliares, diagnósticos, cursor/progreso y
  evaluación única cuando exista lowering.
- **Regla:** el éxito sintáctico nunca sustituye la comparación estructural del AST.

Los IDs `TP-PAR-NNN` se asignan por el orden canónico de las filas. Las entradas son fragmentos tokenizados con
EOF y arena/engine nuevos salvo que la fila indique otro setup.

## Cursor, recuperación y lookahead

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-001 | CP-PAR-001 | fuente vacía | CompilationUnit vacío; cursor en EOF; rango estable | PR |
| TP-PAR-002 | CP-PAR-001 | una declaración válida seguida de EOF | un child; rango desde primer token hasta EOF | PR |
| TP-PAR-003 | CP-PAR-001 | dos declaraciones válidas | orden y rangos exactos | PR |
| TP-PAR-004 | CP-PAR-001 | `peek` más allá de EOF | referencia al último token, sin avance | PR |
| TP-PAR-005 | CP-PAR-001 | `consume` repetido en EOF | cursor no rebasa último token | PR |
| TP-PAR-006 | CP-PAR-001 | parser de declaración sin progreso | unidad descarta exactamente un token y termina | PR |
| TP-PAR-007 | CP-PAR-001 | seed externa y 19 nombres de tipo predeterminados | nombres presentes; vacío ignorado; sin duplicación observable | PR |
| TP-PAR-008 | CP-PAR-002 | `expect` acertado | consume uno; no diagnóstico | PR |
| TP-PAR-009 | CP-PAR-002 | `expect` fallido | ExpectedToken; cursor intacto; rango actual | PR |
| TP-PAR-010 | CP-PAR-002 | contextual keyword acertada/fallida | comparación exacta de Identifier lexeme | PR |
| TP-PAR-011 | CP-PAR-002 | error de primary expression | ErrorExpression y progreso contractual | PR |
| TP-PAR-012 | CP-PAR-002 | basura global hasta `;` | sincroniza después del terminador | PR |
| TP-PAR-013 | CP-PAR-002 | basura global antes de `}` | conserva `}` para caller | PR |
| TP-PAR-014 | CP-PAR-002 | basura global antes de inicio de declaración | no consume el inicio recuperable | PR |
| TP-PAR-015 | CP-PAR-002 | statement inválido hasta `;` | consume terminador y retorna | PR |
| TP-PAR-016 | CP-PAR-002 | statement inválido antes de `}` | conserva cierre | PR |
| TP-PAR-017 | CP-PAR-002 | varios errores consecutivos | cada iteración progresa; diagnóstico no entra en loop | PR |
| TP-PAR-018 | CP-PAR-003 | cada builtin y nombre conocido | canStartType true | PR |
| TP-PAR-019 | CP-PAR-003 | Identifier desconocido | false salvo forma inequívoca | PR |
| TP-PAR-020 | CP-PAR-003 | qualifiers owned/borrowed/lifetime | scanTypeAt termina tras tipo completo | PR |
| TP-PAR-021 | CP-PAR-003 | nombre `A.B.C` | consume cadena calificada en lookahead | PR |
| TP-PAR-022 | CP-PAR-003 | tuple y función anidadas | profundidades balanceadas | PR |
| TP-PAR-023 | CP-PAR-003 | genéricos anidados cerrados con `>>` | afterType exacto | PR |
| TP-PAR-024 | CP-PAR-003 | genéricos cerrados con `>>=` | preserva porción de asignación pendiente | PR |
| TP-PAR-025 | CP-PAR-003 | punteros/arrays combinados | consume todos los modifiers | PR |
| TP-PAR-026 | CP-PAR-003 | `T x` con T conocido/desconocido | ambos prefijos inequívocos llegan a resolución semántica | PR |
| TP-PAR-027 | CP-PAR-003 | condición `T x = e` | condition declaration correcta | PR |
| TP-PAR-028 | CP-PAR-003 | `(T)x` frente a `(a+b)` | type/cast lookahead no confunde expresión | PR |
| TP-PAR-029 | CP-PAR-003 | generic type parameter scoped | visible dentro y retirado al salir | PR |
| TP-PAR-030 | CP-PAR-003 | parámetro con nombre igual a tipo exterior | guard no borra el exterior | PR |

## Nombres y tipos

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-031 | CP-PAR-004 | `A` y `A.B.C` | texto y rango completos | PR |
| TP-PAR-032 | CP-PAR-004 | `A.` | ExpectedToken en componente faltante | PR |
| TP-PAR-033 | CP-PAR-004 | token no Identifier | false, salida no falsamente completada | PR |
| TP-PAR-034 | CP-PAR-004 | lifetime ausente | string vacío, cursor intacto | PR |
| TP-PAR-035 | CP-PAR-004 | lifetime válida e inválida | nombre exacto o diagnóstico/rango | PR |
| TP-PAR-036 | CP-PAR-005 | cada builtin | base kind/name exacto | PR |
| TP-PAR-037 | CP-PAR-005 | nombre conocido calificado | TypeSyntax nominal completo | PR |
| TP-PAR-038 | CP-PAR-005 | `owned T`, `borrowed T`, lifetime | qualifier/lifetime exactos | PR |
| TP-PAR-039 | CP-PAR-005 | `dyn Trait` | dynamicTrait true y base exacta | PR |
| TP-PAR-040 | CP-PAR-005 | `some Trait` | opaqueResult y trait exactos | PR |
| TP-PAR-041 | CP-PAR-005 | tuple vacía, unaria y múltiple | tuple flag, elementos y rangos | PR |
| TP-PAR-042 | CP-PAR-005 | tuple con nombres parciales | nombres alineados con elementos | PR |
| TP-PAR-043 | CP-PAR-005 | tipo inesperado | ExpectedType y resultado de recuperación | PR |
| TP-PAR-044 | CP-PAR-005 | `void`, `void[]`, `void*` | objeto/array rechazado; puntero permitido | PR |
| TP-PAR-045 | CP-PAR-005 | type con `?` permitido/no permitido | nulabilidad en modifier correcto | PR |
| TP-PAR-046 | CP-PAR-005 | explicit trait implementation type | flag y nombre preservados | PR |
| TP-PAR-047 | CP-PAR-005 | combinaciones profundas válidas | jerarquía TypeSyntax exacta | NIGHTLY |
| TP-PAR-048 | CP-PAR-006 | función sin parámetros y retorno void | function flag/carrier/return | PR |
| TP-PAR-049 | CP-PAR-006 | función con N parámetros | typeArguments en orden | PR |
| TP-PAR-050 | CP-PAR-006 | calling conventions soportadas | enum/string contractual exacto | PR |
| TP-PAR-051 | CP-PAR-006 | variadic function type | variadic true y posición válida | PR |
| TP-PAR-052 | CP-PAR-006 | unsafe/noheap/async | flags independientes | PR |
| TP-PAR-053 | CP-PAR-006 | nullable y lifetime de función | kind/carrier/nullability/lifetime/arity/return/cursor EOF exactos | PR |
| TP-PAR-054 | CP-PAR-006 | función con error type | throws/error type clonado | PR |
| TP-PAR-055 | CP-PAR-006 | `dyn`/`some` sin trait | diagnóstico y progreso | PR |
| TP-PAR-056 | CP-PAR-006 | SIMD portable lane type/count | base y argumentos exactos | PR |
| TP-PAR-057 | CP-PAR-006 | mask portable lane count | forma exacta | PR |
| TP-PAR-058 | CP-PAR-006 | lane/count inválidos | diagnóstico específico y rango | PR |
| TP-PAR-059 | CP-PAR-006 | tuple/function anidados | no pierde delimitadores | PR |
| TP-PAR-060 | CP-PAR-007 | `T*`, `const T*` y chains | orden de pointer modifiers/const | PR |
| TP-PAR-061 | CP-PAR-007 | pointer align válido/inválido | valor o diagnóstico | PR |
| TP-PAR-062 | CP-PAR-007 | addrspace válido/inválido | valor o diagnóstico | PR |
| TP-PAR-063 | CP-PAR-007 | provenance válido/inválido | texto/flag o diagnóstico | PR |
| TP-PAR-064 | CP-PAR-007 | array `[N]` | modifier fijo y expresión bound | PR |
| TP-PAR-065 | CP-PAR-007 | dynamic/range/rank arrays | kind/rank exactos | PR |
| TP-PAR-066 | CP-PAR-007 | flexible array | modifier exacto y posición | PR |
| TP-PAR-067 | CP-PAR-007 | modifiers pointer/array mixtos | orden fuente preservado | PR |
| TP-PAR-068 | CP-PAR-007 | allocation element válido | TypeSyntax sin consumir initializer | PR |
| TP-PAR-069 | CP-PAR-007 | allocation element inválido/void object | InvalidType y progreso | PR |

## Genéricos

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-070 | CP-PAR-008 | sin `<` | true, lista vacía, cursor intacto | PR |
| TP-PAR-071 | CP-PAR-008 | `<T,U>` | dos type parameters en orden | PR |
| TP-PAR-072 | CP-PAR-008 | lifetime parameter | kind/name exacto | PR |
| TP-PAR-073 | CP-PAR-008 | const parameter con tipo | kind, nombre y constantType | PR |
| TP-PAR-074 | CP-PAR-008 | type pack y const pack | pack flags/kinds exactos | PR |
| TP-PAR-075 | CP-PAR-008 | pack no final | diagnóstico y false | PR |
| TP-PAR-076 | CP-PAR-008 | nombre duplicado entre clases | DuplicateSymbol | PR |
| TP-PAR-077 | CP-PAR-008 | lista vacía/missing comma | ExpectedToken y recuperación | PR |
| TP-PAR-078 | CP-PAR-008 | nested generic close `>>` | ambos niveles cierran | PR |
| TP-PAR-079 | CP-PAR-008 | scope de parámetros type vs const | sólo type/pack entra a known types | PR |
| TP-PAR-080 | CP-PAR-009 | `where T : class` | constraint contextual exacto | PR |
| TP-PAR-081 | CP-PAR-009 | struct/new/copy/move/unmanaged/notnull | cada flag/constraint exacto | PR |
| TP-PAR-082 | CP-PAR-009 | constraints nominales múltiples | TypeSyntaxes y orden | PR |
| TP-PAR-083 | CP-PAR-009 | igualdad de tipos | equalityConstraint exacta | PR |
| TP-PAR-084 | CP-PAR-009 | igualdad constante | expresión/valor exacto | PR |
| TP-PAR-085 | CP-PAR-009 | proyección asociada | associatedProjection exacta | PR |
| TP-PAR-086 | CP-PAR-009 | sustitución genérica | substitutedArgument exacto | PR |
| TP-PAR-087 | CP-PAR-009 | lifetime outlives simple/múltiple | pares y orden exactos | PR |
| TP-PAR-088 | CP-PAR-009 | target no declarado | diagnóstico observable vigente | PR |
| TP-PAR-089 | CP-PAR-009 | constraint repetido | DuplicateSymbol/rango exacto | PR |
| TP-PAR-090 | CP-PAR-009 | separadores o colon faltantes | false, diagnóstico y progreso | PR |
| TP-PAR-091 | CP-PAR-009 | múltiples where | todas las clauses conservadas | PR |
| TP-PAR-092 | CP-PAR-009 | where tras método/class/delegate/struct | misma representación en cuatro contextos | PR |
| TP-PAR-093 | CP-PAR-009 | pack constraint válido/inválido | aceptación/diagnóstico vigente | PR |
| TP-PAR-094 | CP-PAR-010 | `<T>` | un argumento type | PR |
| TP-PAR-095 | CP-PAR-010 | mezcla tipo/const/pack expansion | clases y orden exactos | PR |
| TP-PAR-096 | CP-PAR-010 | nested close con `>` | pendingGenericGreater cero | PR |
| TP-PAR-097 | CP-PAR-010 | nested close con `>>` | un cierre consumido y uno pendiente | PR |
| TP-PAR-098 | CP-PAR-010 | nested close con `>>=` | cierres y `=` preservados | PR |
| TP-PAR-099 | CP-PAR-010 | lista vacía o trailing comma | contrato exacto de aceptación/error | PR |
| TP-PAR-100 | CP-PAR-010 | token prohibido en argumento | false sin desbordar delimitador | PR |
| TP-PAR-101 | CP-PAR-010 | EOF antes de close | ExpectedToken y progreso finito | PR |
| TP-PAR-102 | CP-PAR-011 | `f<T>(x)` | generic call true y argumentos postfix | PR |
| TP-PAR-103 | CP-PAR-011 | `a < b > (c)` | comparación, no llamada genérica | PR |
| TP-PAR-104 | CP-PAR-011 | `Type<T>.Member` | closed static receiver true | PR |
| TP-PAR-105 | CP-PAR-011 | nested `Type<A<B>>.M` | cierre correcto | PR |
| TP-PAR-106 | CP-PAR-011 | `<` sin cierre | lookahead false, cursor intacto | PR |
| TP-PAR-107 | CP-PAR-011 | argumentos con token no admitido | false | PR |
| TP-PAR-108 | CP-PAR-011 | call genérica seguida de postfix chain | no consume fuera de lista | PR |
| TP-PAR-109 | CP-PAR-011 | dos lookaheads repetidos | deterministas y sin side effects | PR |

## Imports, namespaces y declaraciones globales

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-110 | CP-PAR-012 | import namespace/symbol | using node y target exactos | PR |
| TP-PAR-111 | CP-PAR-012 | import con alias | alias/target exactos | PR |
| TP-PAR-112 | CP-PAR-012 | import trait/extension | metadata/kind exacto | PR |
| TP-PAR-113 | CP-PAR-012 | import glob restringido | lista y restricciones preservadas | PR |
| TP-PAR-114 | CP-PAR-012 | reexport de cada forma | flag reexport sin cambiar target | PR |
| TP-PAR-115 | CP-PAR-012 | target faltante | InvalidUsingTarget/ExpectedToken | PR |
| TP-PAR-116 | CP-PAR-012 | alias duplicado en sintaxis | forma AST; semántica diferida | PR |
| TP-PAR-117 | CP-PAR-012 | terminador faltante | diagnóstico y recuperación al miembro siguiente | PR |
| TP-PAR-118 | CP-PAR-012 | keyword contextual con case distinto | no se reconoce como import especializado | PR |
| TP-PAR-119 | CP-PAR-012 | import seguido de declaración | dispatch conserva ambos children | PR |
| TP-PAR-120 | CP-PAR-013 | namespace con llaves | nombre, children y rango | PR |
| TP-PAR-121 | CP-PAR-013 | namespace por archivo | miembros hasta EOF | PR |
| TP-PAR-122 | CP-PAR-013 | namespaces anidados/calificados | estructura/nombre exactos | PR |
| TP-PAR-123 | CP-PAR-013 | using namespace | UsingNamespaceDirective | PR |
| TP-PAR-124 | CP-PAR-013 | using alias de tipo complejo | UsingAliasDirective y TypeSyntax | PR |
| TP-PAR-125 | CP-PAR-013 | extension con receptor/cuerpo | ExtensionDeclaration y contexto acotado | PR |
| TP-PAR-126 | CP-PAR-013 | cierre faltante | diagnóstico y progreso | PR |
| TP-PAR-127 | CP-PAR-013 | token global no reconocido | ExpectedDeclaration y synchronizeGlobal | PR |
| TP-PAR-128 | CP-PAR-014 | cada modificador global aislado | flag/campo exacto | PR |
| TP-PAR-129 | CP-PAR-014 | combinaciones válidas por kind nominal | todos los flags preservados | PR |
| TP-PAR-130 | CP-PAR-014 | modifiers duplicados | diagnóstico exacto | PR |
| TP-PAR-131 | CP-PAR-014 | accesibilidades múltiples | rechazo/rango exacto | PR |
| TP-PAR-132 | CP-PAR-014 | repr válido e inválido | metadata o InvalidArgumentCount/Type | PR |
| TP-PAR-133 | CP-PAR-014 | pack/align/size válidos | enteros exactos | PR |
| TP-PAR-134 | CP-PAR-014 | pack/align/size no literal/overflow | diagnóstico y progreso | PR |
| TP-PAR-135 | CP-PAR-014 | export extern C e inline family | dispatch/flags exactos | PR |
| TP-PAR-136 | CP-PAR-014 | `record struct` y modifiers copy/borrowed | ruta agregada correcta | PR |
| TP-PAR-137 | CP-PAR-014 | `union`, variant y opaque | parser específico correcto | PR |
| TP-PAR-138 | CP-PAR-014 | class/interface/trait/impl/delegate | parser específico correcto | PR |
| TP-PAR-139 | CP-PAR-014 | enum y extern block | parser específico correcto | PR |
| TP-PAR-140 | CP-PAR-014 | función frente a variable global | separación por token posterior al nombre | PR |
| TP-PAR-141 | CP-PAR-014 | modifier sin significado para declaration | InvalidType/ExpectedDeclaration vigente | PR |
| TP-PAR-142 | CP-PAR-014 | declaración válida tras global inválida | recuperación no pierde la segunda | PR |
| TP-PAR-143 | CP-PAR-014 | secuencia heterogénea de todas las families | child kinds/orden/rangos | NIGHTLY |

## Delegate, struct, variant y opaque

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-144 | CP-PAR-015 | delegate mínimo | kind, retorno, nombre y terminador | PR |
| TP-PAR-145 | CP-PAR-015 | parámetros y defaults | children en orden | PR |
| TP-PAR-146 | CP-PAR-015 | genéricos y where | parámetros/constraints exactos | PR |
| TP-PAR-147 | CP-PAR-015 | throws | throws type exacto | PR |
| TP-PAR-148 | CP-PAR-015 | nombre/paren/semicolon faltante | diagnóstico y progreso | PR |
| TP-PAR-149 | CP-PAR-015 | modifiers globales válidos | flags propagados | PR |
| TP-PAR-150 | CP-PAR-015 | forma inválida seguida de declaration | recuperación preserva siguiente | PR |
| TP-PAR-151 | CP-PAR-016 | struct vacío | StructDeclaration y cuerpo vacío | PR |
| TP-PAR-152 | CP-PAR-016 | union vacía | layout form union exacto | PR |
| TP-PAR-153 | CP-PAR-016 | record/copy/borrowed/readonly | flags independientes | PR |
| TP-PAR-154 | CP-PAR-016 | parámetros primarios | campos sintetizados y orden | PR |
| TP-PAR-155 | CP-PAR-016 | constructor primario sintetizado | parámetros/assignments/rango | PR |
| TP-PAR-156 | CP-PAR-016 | bases y where | listas exactas | PR |
| TP-PAR-157 | CP-PAR-016 | fields/methods/nested types | children en orden fuente | PR |
| TP-PAR-158 | CP-PAR-016 | bitfields y anonymous aggregates | representación exacta | PR |
| TP-PAR-159 | CP-PAR-016 | pack/align/size | atributos exactos | PR |
| TP-PAR-160 | CP-PAR-016 | duplicate primary name | DuplicateSymbol | PR |
| TP-PAR-161 | CP-PAR-016 | body member inválido | progresa al próximo member | PR |
| TP-PAR-162 | CP-PAR-016 | missing close brace | diagnóstico/rango finito | PR |
| TP-PAR-163 | CP-PAR-016 | union con modifiers incompatibles | diagnóstico vigente | PR |
| TP-PAR-164 | CP-PAR-016 | nested generic struct | KnownGenericTypeScope acotado | PR |
| TP-PAR-165 | CP-PAR-017 | variant sin payload | tag/factory/test sintetizados | PR |
| TP-PAR-166 | CP-PAR-017 | variant con payload unario | union field/extractor exactos | PR |
| TP-PAR-167 | CP-PAR-017 | payload tuple/nombrado | fields y factory params alineados | PR |
| TP-PAR-168 | CP-PAR-017 | varias alternatives | tags monotónicos y orden | PR |
| TP-PAR-169 | CP-PAR-017 | discriminante explícito no admitido | diagnóstico, recuperación y tags ordinales | PR |
| TP-PAR-170 | CP-PAR-017 | nombres duplicados | DuplicateSymbol | PR |
| TP-PAR-171 | CP-PAR-017 | payload/type inválido | diagnóstico y recuperación | PR |
| TP-PAR-172 | CP-PAR-017 | genéricos/where | síntesis referencia parámetros correctos | PR |
| TP-PAR-173 | CP-PAR-017 | copy/non-copy | flag sólo cuando escrito | PR |
| TP-PAR-174 | CP-PAR-017 | cuerpo vacío/cierre faltante | error y progreso | PR |
| TP-PAR-175 | CP-PAR-018 | opaque mínimo | struct, campo privado y boundary methods | PR |
| TP-PAR-176 | CP-PAR-018 | opaque copy | copy propagado | PR |
| TP-PAR-177 | CP-PAR-018 | opaque no-copy | no inventa copy | PR |
| TP-PAR-178 | CP-PAR-018 | underlying type complejo | field type exacto | PR |
| TP-PAR-179 | CP-PAR-018 | genéricos/where | metadata/scope exactos | PR |
| TP-PAR-180 | CP-PAR-018 | nombre/type/terminador faltante | diagnóstico y progreso | PR |
| TP-PAR-181 | CP-PAR-018 | modifiers incompatibles | rechazo vigente | PR |
| TP-PAR-182 | CP-PAR-018 | dos opaque consecutivos | síntesis higiénica e independiente | PR |

## Clases, interfaces y tipos anidados

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-183 | CP-PAR-019 | class mínima | ClassDeclaration exacta | PR |
| TP-PAR-184 | CP-PAR-019 | partial/abstract/sealed/static | flags exactos | PR |
| TP-PAR-185 | CP-PAR-019 | base class e interfaces | bases en orden | PR |
| TP-PAR-186 | CP-PAR-019 | genéricos/where | scope y constraints exactos | PR |
| TP-PAR-187 | CP-PAR-019 | interface mínima/partial | InterfaceDeclaration flags | PR |
| TP-PAR-188 | CP-PAR-019 | trait mínima | trait marker y miembros | PR |
| TP-PAR-189 | CP-PAR-019 | impl Trait for Type | ClassDeclaration marcada con trait y target exactos | PR |
| TP-PAR-190 | CP-PAR-019 | associated type declaration | kind/name/constraints | PR |
| TP-PAR-191 | CP-PAR-019 | associated const declaration | type/name/default | PR |
| TP-PAR-192 | CP-PAR-019 | associated implementation | valor/type requerido según kind | PR |
| TP-PAR-193 | CP-PAR-019 | member no permitido en interface | diagnóstico y recuperación | PR |
| TP-PAR-194 | CP-PAR-019 | base/brace faltante | diagnóstico y progreso | PR |
| TP-PAR-195 | CP-PAR-019 | generic name tras salir de class | no queda en knownTypeNames | PR |
| TP-PAR-196 | CP-PAR-019 | mezcla members válidos | orden/rangos completos | NIGHTLY |
| TP-PAR-197 | CP-PAR-020 | nested class/struct/interface/enum/delegate | parser existente y child kind correcto | PR |
| TP-PAR-198 | CP-PAR-020 | nested accessibility | modifier aplicado al nodo | PR |
| TP-PAR-199 | CP-PAR-020 | partial válido sólo donde admite | aceptación/rechazo exacto | PR |
| TP-PAR-200 | CP-PAR-020 | new class y combinaciones inválidas de modifiers | new preservado; rechazos por kind diagnosticados | PR |
| TP-PAR-201 | CP-PAR-020 | layout attributes en nested struct | propagación exacta | PR |
| TP-PAR-202 | CP-PAR-020 | borrowed/copy nested struct | flags exactos | PR |
| TP-PAR-203 | CP-PAR-020 | token que no abre nested type | atNestedTypeDeclaration false | PR |
| TP-PAR-204 | CP-PAR-020 | nested inválido seguido de member | recuperación conserva member siguiente | PR |

## Dispatch y miembros de tipo

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-205 | CP-PAR-021 | campo simple y múltiples declarators | FieldDeclaration(s), tipo e initializers | PR |
| TP-PAR-206 | CP-PAR-021 | const/static/readonly field | flags exactos | PR |
| TP-PAR-207 | CP-PAR-021 | explicit offset field | offset exacto | PR |
| TP-PAR-208 | CP-PAR-021 | bitfield width válida/inválida | metadata o diagnóstico | PR |
| TP-PAR-209 | CP-PAR-021 | anonymous struct/union member | children/layout exactos | PR |
| TP-PAR-210 | CP-PAR-021 | constructor por nombre de type | ruta constructor | PR |
| TP-PAR-211 | CP-PAR-021 | destructor | ruta destructor | PR |
| TP-PAR-212 | CP-PAR-021 | event | ruta event | PR |
| TP-PAR-213 | CP-PAR-021 | operator/conversion | ruta específica | PR |
| TP-PAR-214 | CP-PAR-021 | property/indexer | lookahead y ruta correcta | PR |
| TP-PAR-215 | CP-PAR-021 | method vs field | paren posterior decide method | PR |
| TP-PAR-216 | CP-PAR-021 | nested type | ruta CP-PAR-020 | PR |
| TP-PAR-217 | CP-PAR-021 | extern C block/import | child nodes exactos | PR |
| TP-PAR-218 | CP-PAR-021 | modifier duplicate/conflicting | diagnóstico exacto | PR |
| TP-PAR-219 | CP-PAR-021 | unresolved named type permitido/rechazado | conducta según argumento | PR |
| TP-PAR-220 | CP-PAR-021 | extension body member permitido | parsingExtensionBody acotado | PR |
| TP-PAR-221 | CP-PAR-021 | member inválido | ExpectedDeclaration y progreso | PR |
| TP-PAR-222 | CP-PAR-021 | cierre de type | no lo consume como member | PR |

## Eventos, propiedades, operadores y callables

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-223 | CP-PAR-022 | event mínimo | EventDeclaration, tipo/nombre | PR |
| TP-PAR-224 | CP-PAR-022 | public/private static new | modifiers exactos | PR |
| TP-PAR-225 | CP-PAR-022 | event con accessors admitidos | children/rangos exactos | PR |
| TP-PAR-226 | CP-PAR-022 | tipo o nombre faltante | ExpectedType/Token y progreso | PR |
| TP-PAR-227 | CP-PAR-022 | modifier no permitido | diagnóstico exacto | PR |
| TP-PAR-228 | CP-PAR-022 | dos events consecutivos | nodos independientes y orden | PR |
| TP-PAR-229 | CP-PAR-023 | property get auto | PropertyDeclaration, getter y backing field | PR |
| TP-PAR-230 | CP-PAR-023 | property get/set auto | dos accessors y setter assignment | PR |
| TP-PAR-231 | CP-PAR-023 | property con cuerpos | no sintetiza cuerpo/backing indebido | PR |
| TP-PAR-232 | CP-PAR-023 | accessor expression-bodied | MethodDeclaration/body exacto | PR |
| TP-PAR-233 | CP-PAR-023 | accesibilidad de accessor más restrictiva | modifier exacto | PR |
| TP-PAR-234 | CP-PAR-023 | accesibilidad inválida/duplicada | diagnóstico | PR |
| TP-PAR-235 | CP-PAR-023 | accessor duplicado | DuplicateSymbol | PR |
| TP-PAR-236 | CP-PAR-023 | property abstract/interface | bodyless válido sin backing | PR |
| TP-PAR-237 | CP-PAR-023 | property concreta sin accessor body válido | diagnóstico | PR |
| TP-PAR-238 | CP-PAR-023 | static/virtual/override/sealed/new | flags propagados | PR |
| TP-PAR-239 | CP-PAR-023 | reference-return property | tipo/flag exacto | PR |
| TP-PAR-240 | CP-PAR-023 | indexer uno/múltiples parámetros | IndexerDeclaration y parámetros | PR |
| TP-PAR-241 | CP-PAR-023 | indexer reference return | flag y accessors exactos | PR |
| TP-PAR-242 | CP-PAR-023 | indexer sin `this`/bracket | diagnóstico y progreso | PR |
| TP-PAR-243 | CP-PAR-023 | auto setter owned vs non-owned | transferOwnership exacto | PR |
| TP-PAR-244 | CP-PAR-023 | nombres backing de dos properties | únicos y deterministas | PR |
| TP-PAR-245 | CP-PAR-024 | operador unary soportado | nombre canónico y un parámetro | PR |
| TP-PAR-246 | CP-PAR-024 | operador binary soportado | nombre canónico y dos parámetros | PR |
| TP-PAR-247 | CP-PAR-024 | increment/decrement | op_Increment/op_Decrement | PR |
| TP-PAR-248 | CP-PAR-024 | aridad incorrecta | InvalidArgumentCount | PR |
| TP-PAR-249 | CP-PAR-024 | token no operador | ExpectedToken/InvalidOperand | PR |
| TP-PAR-250 | CP-PAR-024 | operator no static/public requerido | diagnóstico vigente | PR |
| TP-PAR-251 | CP-PAR-024 | bodyless permitido/no permitido | aceptación según contexto | PR |
| TP-PAR-252 | CP-PAR-024 | implicit conversion | nombre/flag/retorno/parámetro | PR |
| TP-PAR-253 | CP-PAR-024 | explicit conversion | nombre/flag/retorno/parámetro | PR |
| TP-PAR-254 | CP-PAR-024 | conversion con aridad/modifiers inválidos | diagnóstico | PR |
| TP-PAR-255 | CP-PAR-024 | todos spellings mapeados | nombre de método canónico exacto | PR |
| TP-PAR-256 | CP-PAR-024 | kind fuera del mapping | op_Invalid no se acepta silenciosamente | PR |
| TP-PAR-257 | CP-PAR-025 | constructor vacío | ConstructorDeclaration y cuerpo | PR |
| TP-PAR-258 | CP-PAR-025 | parámetros/defaults | children exactos | PR |
| TP-PAR-259 | CP-PAR-025 | initializer `base(...)` | BaseConstructorInitializer | PR |
| TP-PAR-260 | CP-PAR-025 | initializer `this(...)` | ThisConstructorInitializer | PR |
| TP-PAR-261 | CP-PAR-025 | noheap/throws | flags/type exactos | PR |
| TP-PAR-262 | CP-PAR-025 | nombre distinto al class | no se clasifica como constructor | PR |
| TP-PAR-263 | CP-PAR-025 | destructor válido | DestructorDeclaration, nombre y cuerpo | PR |
| TP-PAR-264 | CP-PAR-025 | destructor con parámetros/retorno | diagnóstico | PR |
| TP-PAR-265 | CP-PAR-025 | constructor bodyless no admitido | diagnóstico | PR |
| TP-PAR-266 | CP-PAR-025 | delimitadores faltantes | recuperación al siguiente member | PR |
| TP-PAR-267 | CP-PAR-026 | método mínimo | retorno/nombre/parámetros/cuerpo | PR |
| TP-PAR-268 | CP-PAR-026 | método genérico con where | parámetros y constraints | PR |
| TP-PAR-269 | CP-PAR-026 | static/virtual/override/abstract/sealed/new | flags exactos | PR |
| TP-PAR-270 | CP-PAR-026 | inline/forceinline/noinline | exclusión y metadata exactas | PR |
| TP-PAR-271 | CP-PAR-026 | export y extern C | flags/calling convention | PR |
| TP-PAR-272 | CP-PAR-026 | unsafe/noheap | flags independientes | PR |
| TP-PAR-273 | CP-PAR-026 | expression body y block body | forma de body exacta | PR |
| TP-PAR-274 | CP-PAR-026 | bodyless permitido en interface/extern | null body sin error | PR |
| TP-PAR-275 | CP-PAR-026 | bodyless concreto | diagnóstico | PR |
| TP-PAR-276 | CP-PAR-026 | función global mínima | FunctionDeclaration exacta | PR |
| TP-PAR-277 | CP-PAR-026 | global variable con/sin initializer | GlobalVariableDeclaration | PR |
| TP-PAR-278 | CP-PAR-026 | extern C block con varias funciones | ExternCDeclaration children/orden | PR |
| TP-PAR-279 | CP-PAR-026 | extern C malformed | diagnóstico y recuperación | PR |
| TP-PAR-280 | CP-PAR-026 | function/global ambiguous prefix | decisión por tokens sin perder cursor | PR |
| TP-PAR-281 | CP-PAR-026 | modifiers incompatibles | diagnóstico exacto | PR |
| TP-PAR-282 | CP-PAR-026 | callable seguido de global | rangos y separación correctos | PR |
| TP-PAR-283 | CP-PAR-027 | parámetros sin/default | ParameterDeclaration exacto | PR |
| TP-PAR-284 | CP-PAR-027 | ref/ownership/lifetime | flags/type exactos | PR |
| TP-PAR-285 | CP-PAR-027 | variadic y packs | clase/posición exacta | PR |
| TP-PAR-286 | CP-PAR-027 | default antes de requerido | AST y diagnóstico vigente | PR |
| TP-PAR-287 | CP-PAR-027 | nombre/tipo faltante | diagnóstico y progreso | PR |
| TP-PAR-288 | CP-PAR-027 | FP mode cada spelling | FloatingPointMode exacto | PR |
| TP-PAR-289 | CP-PAR-027 | FP mode duplicado/inválido | diagnóstico | PR |
| TP-PAR-290 | CP-PAR-027 | throws con tipo | throwsType exacto | PR |
| TP-PAR-291 | CP-PAR-027 | throws sin tipo según forma soportada | contrato AST exacto | PR |
| TP-PAR-292 | CP-PAR-027 | throws duplicado/malformado | diagnóstico | PR |
| TP-PAR-293 | CP-PAR-027 | unwind allow/abort u opciones actuales | policy exacta | PR |
| TP-PAR-294 | CP-PAR-027 | unwind inválido/duplicado | diagnóstico | PR |
| TP-PAR-295 | CP-PAR-027 | combinación FP/throws/unwind | campos independientes | PR |
| TP-PAR-296 | CP-PAR-027 | orden alternativo permitido/no permitido | aceptación/diagnóstico exacto | PR |

## Statements, variables e inline assembly

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-297 | CP-PAR-028 | bloque vacío y con N statements | BlockStatement, children/rango | PR |
| TP-PAR-298 | CP-PAR-028 | nested blocks | jerarquía exacta | PR |
| TP-PAR-299 | CP-PAR-028 | falta `}` | ExpectedToken y terminación | PR |
| TP-PAR-300 | CP-PAR-028 | cada keyword de statement | dispatch al kind correcto | PR |
| TP-PAR-301 | CP-PAR-028 | expresión con `;` | ExpressionStatement | PR |
| TP-PAR-302 | CP-PAR-028 | expresión sin `;` | diagnóstico y recuperación | PR |
| TP-PAR-303 | CP-PAR-028 | variable explícita con initializer | VariableDeclarationStatement | PR |
| TP-PAR-304 | CP-PAR-028 | variable sin initializer | child null y rango | PR |
| TP-PAR-305 | CP-PAR-028 | inferida/const/RAII | flags exactos | PR |
| TP-PAR-306 | CP-PAR-028 | múltiples declarators | nodos/children y orden | PR |
| TP-PAR-307 | CP-PAR-028 | deconstruction tuple | patrón/nombres exactos | PR |
| TP-PAR-308 | CP-PAR-028 | deconstruction nested/discard | árbol exacto | PR |
| TP-PAR-309 | CP-PAR-028 | binding ref/move/value | modifiers exactos | PR |
| TP-PAR-310 | CP-PAR-028 | patrón/initializer faltante | diagnóstico y progreso | PR |
| TP-PAR-311 | CP-PAR-028 | aggregate positional | children en orden | PR |
| TP-PAR-312 | CP-PAR-028 | aggregate designado | argumentName por field | PR |
| TP-PAR-313 | CP-PAR-028 | aggregate con spread | SpreadElementExpression | PR |
| TP-PAR-314 | CP-PAR-028 | statement inválido entre dos válidos | ambos válidos sobreviven recuperación | PR |
| TP-PAR-315 | CP-PAR-029 | asm mínimo con body vacío | InlineAssemblyStatement | PR |
| TP-PAR-316 | CP-PAR-029 | target y dialect válidos | metadata exacta | PR |
| TP-PAR-317 | CP-PAR-029 | target inválido | InvalidInlineAssemblyTarget | PR |
| TP-PAR-318 | CP-PAR-029 | línea raw simple/múltiple | InlineAssemblyLine y bytes exactos | PR |
| TP-PAR-319 | CP-PAR-029 | output operand explícito/implícito | kind, constraint, expression | PR |
| TP-PAR-320 | CP-PAR-029 | input operand explícito/implícito | kind, constraint, expression | PR |
| TP-PAR-321 | CP-PAR-029 | clobbers válidos | lista exacta | PR |
| TP-PAR-322 | CP-PAR-029 | clobber inválido/duplicado | InvalidInlineAssemblyClobber | PR |
| TP-PAR-323 | CP-PAR-029 | constraints output permitidos | aceptación y texto exacto | PR |
| TP-PAR-324 | CP-PAR-029 | constraint output inválido | InvalidInlineAssemblyConstraint | PR |
| TP-PAR-325 | CP-PAR-029 | constraints input permitidos | aceptación y texto exacto | PR |
| TP-PAR-326 | CP-PAR-029 | constraint input inválido | InvalidInlineAssemblyConstraint | PR |
| TP-PAR-327 | CP-PAR-029 | placeholder por índice/nombre válido | resolución exacta | PR |
| TP-PAR-328 | CP-PAR-029 | placeholder inexistente | InvalidInlineAssemblyPlaceholder | PR |
| TP-PAR-329 | CP-PAR-029 | control flow admitido | validación sin error | PR |
| TP-PAR-330 | CP-PAR-029 | branch/call/label prohibido según contrato | InvalidInlineAssemblyControlFlow | PR |
| TP-PAR-331 | CP-PAR-029 | delimitadores de clauses faltantes | diagnóstico y progreso | PR |
| TP-PAR-332 | CP-PAR-029 | asm seguido de statement | lexer/parser recuperan frontera | PR |

## Control, patrones y match

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-333 | CP-PAR-030 | if simple/else/else-if | IfStatement jerárquico | PR |
| TP-PAR-334 | CP-PAR-030 | condition declaration | scope/lowering exacto | PR |
| TP-PAR-335 | CP-PAR-030 | conditional pattern | tests/bindings y body | PR |
| TP-PAR-336 | CP-PAR-030 | while simple | condition/body | PR |
| TP-PAR-337 | CP-PAR-030 | do/while | orden body/condition y semicolon | PR |
| TP-PAR-338 | CP-PAR-030 | for vacío | init/condition/increment null contractual | PR |
| TP-PAR-339 | CP-PAR-030 | for completo | cuatro componentes exactos | PR |
| TP-PAR-340 | CP-PAR-030 | switch vacío | governing y sin sections | PR |
| TP-PAR-341 | CP-PAR-030 | múltiples case labels misma section | labels agrupados | PR |
| TP-PAR-342 | CP-PAR-030 | default label | SwitchLabel default | PR |
| TP-PAR-343 | CP-PAR-030 | section inicia `default(T)` | expresión, no label | PR |
| TP-PAR-344 | CP-PAR-030 | fallthrough implícito | diagnóstico/estructura vigente | PR |
| TP-PAR-345 | CP-PAR-030 | break/continue | kinds/rangos | PR |
| TP-PAR-346 | CP-PAR-030 | statementAlwaysDiverges return/throw | true | PR |
| TP-PAR-347 | CP-PAR-030 | divergence if ambos arms vs uno | true vs false | PR |
| TP-PAR-348 | CP-PAR-030 | malformed control seguido de statement | recuperación preserva siguiente | PR |
| TP-PAR-349 | CP-PAR-031 | wildcard | PatternKind Wildcard | PR |
| TP-PAR-350 | CP-PAR-031 | literal bool/int/char/string/null | kind/value/rango exactos | PR |
| TP-PAR-351 | CP-PAR-031 | signed integer e INT64_MIN | firstInteger exacto sin overflow | PR |
| TP-PAR-352 | CP-PAR-031 | binding implícito/explícito | name y mode Value | PR |
| TP-PAR-353 | CP-PAR-031 | `ref` binding | mode Reference | PR |
| TP-PAR-354 | CP-PAR-031 | `move` binding | mode Move | PR |
| TP-PAR-355 | CP-PAR-031 | tuple pattern vacío/nested | children exactos | PR |
| TP-PAR-356 | CP-PAR-031 | struct fields shorthand/explicit | fieldNames/children alineados | PR |
| TP-PAR-357 | CP-PAR-031 | variant sin/con payload | name y children | PR |
| TP-PAR-358 | CP-PAR-031 | inclusive/exclusive range | endpoints/kind exactos | PR |
| TP-PAR-359 | CP-PAR-031 | `or` simple/nested | alternativas preservadas | PR |
| TP-PAR-360 | CP-PAR-031 | or con bindings compatibles | expansión y bindings exactos | PR |
| TP-PAR-361 | CP-PAR-031 | or con bindings incompatibles | diagnóstico vigente | PR |
| TP-PAR-362 | CP-PAR-031 | patternHasBindings en cada kind | bool correcto | PR |
| TP-PAR-363 | CP-PAR-031 | expand alternatives producto cartesiano | secuencia exacta | PR |
| TP-PAR-364 | CP-PAR-031 | condition-only vs binding | clasificación correcta | PR |
| TP-PAR-365 | CP-PAR-031 | predicate generado por cada kind | AST booleano exacto | PR |
| TP-PAR-366 | CP-PAR-031 | lowerPattern con emitBindings false/true | bindings ausentes/presentes | PR |
| TP-PAR-367 | CP-PAR-031 | temporaryPrefix repetido | nombres deterministas sin colisión local | PR |
| TP-PAR-368 | CP-PAR-031 | patrón malformado | diagnóstico y progreso | PR |
| TP-PAR-369 | CP-PAR-032 | match statement un arm | temporal, test y body | PR |
| TP-PAR-370 | CP-PAR-032 | match expression un arm | MatchExpression/arm value | PR |
| TP-PAR-371 | CP-PAR-032 | varios arms | orden de decisión | PR |
| TP-PAR-372 | CP-PAR-032 | guard | condición combinada después de binding válido | PR |
| TP-PAR-373 | CP-PAR-032 | catchall final | rama final exacta | PR |
| TP-PAR-374 | CP-PAR-032 | catchall antes de arm | arm posterior diagnosticado | PR |
| TP-PAR-375 | CP-PAR-032 | dos catchall | solapamiento/inalcanzable | PR |
| TP-PAR-376 | CP-PAR-032 | ranges no solapados/solapados | aceptación/diagnóstico sintáctico | PR |
| TP-PAR-377 | CP-PAR-032 | or alternatives | arms expandidos sin reevaluar source | PR |
| TP-PAR-378 | CP-PAR-032 | variant payload binding | extractor/bindings exactos | PR |
| TP-PAR-379 | CP-PAR-032 | scrutinee con side effect | aparece una vez en árbol productivo | PR |
| TP-PAR-380 | CP-PAR-032 | match expression sin fallback | marca para cobertura semántica | PR |
| TP-PAR-381 | CP-PAR-032 | arm sin arrow/body | diagnóstico y progreso | PR |
| TP-PAR-382 | CP-PAR-032 | match vacío | contrato de error exacto | PR |
| TP-PAR-383 | CP-PAR-032 | dos matches consecutivos | temporales monotónicos/independientes | PR |
| TP-PAR-384 | CP-PAR-032 | nesting profundo | rangos/temporales correctos | NIGHTLY |

## Loop, comprehension y foreach

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-385 | CP-PAR-033 | loop con block | LoopExpression/body | PR |
| TP-PAR-386 | CP-PAR-033 | loop expression malformada | diagnóstico y progreso | PR |
| TP-PAR-387 | CP-PAR-033 | comprehension mínima | builder/context/body exactos | PR |
| TP-PAR-388 | CP-PAR-033 | yield dentro de comprehension | call/add generado | PR |
| TP-PAR-389 | CP-PAR-033 | yield fuera de comprehension | diagnóstico | PR |
| TP-PAR-390 | CP-PAR-033 | nested comprehensions | stack context LIFO | PR |
| TP-PAR-391 | CP-PAR-033 | early error en nested context | context restaurado | PR |
| TP-PAR-392 | CP-PAR-033 | varias comprehensions | nombres monotónicos | PR |
| TP-PAR-393 | CP-PAR-033 | comprehension con for/if | árbol de control exacto | PR |
| TP-PAR-394 | CP-PAR-033 | source expression con side effect | evaluación única | PR |
| TP-PAR-395 | CP-PAR-034 | foreach range exclusivo/inclusivo | lower de índice/límite correcto | PR |
| TP-PAR-396 | CP-PAR-034 | foreach array | collection temp, length e index | PR |
| TP-PAR-397 | CP-PAR-034 | foreach span | protocolo de length/index | PR |
| TP-PAR-398 | CP-PAR-034 | foreach enumerator | GetEnumerator/MoveNext/Current | PR |
| TP-PAR-399 | CP-PAR-034 | foreach ref/borrowed/move binding | variable flags exactos | PR |
| TP-PAR-400 | CP-PAR-034 | deconstruction binding | lowering de patrón exacto | PR |
| TP-PAR-401 | CP-PAR-034 | colección con side effect | una evaluación | PR |
| TP-PAR-402 | CP-PAR-034 | body vacío/nested control | body preservado | PR |
| TP-PAR-403 | CP-PAR-034 | type explícito/inferido | declaración exacta | PR |
| TP-PAR-404 | CP-PAR-034 | source inválido | diagnóstico y progreso | PR |
| TP-PAR-405 | CP-PAR-034 | dos foreach | temporales distintos | PR |
| TP-PAR-406 | CP-PAR-034 | foreach nested | temporales no colisionan | PR |
| TP-PAR-407 | CP-PAR-034 | helper member/call/index/binary | formas AST canónicas | PR |
| TP-PAR-408 | CP-PAR-034 | range descendente/step si soportado | contrato actual exacto | PR |
| TP-PAR-409 | CP-PAR-034 | protocol candidate ambiguity | orden contractual de candidates | PR |
| TP-PAR-410 | CP-PAR-034 | iterable no reconocido sintácticamente | conserva forma para Sema | PR |

## Exits y cleanup

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-411 | CP-PAR-035 | return vacío/con valor | ReturnStatement child null/value | PR |
| TP-PAR-412 | CP-PAR-035 | defer block sin captures | TryStatement/finally lowering | PR |
| TP-PAR-413 | CP-PAR-035 | defer captures value/ref/move | modos y locals exactos | PR |
| TP-PAR-414 | CP-PAR-035 | múltiples captures | orden y nombres | PR |
| TP-PAR-415 | CP-PAR-035 | múltiples defer | nesting LIFO | PR |
| TP-PAR-416 | CP-PAR-035 | defer nested blocks | cleanup scope correcto | PR |
| TP-PAR-417 | CP-PAR-035 | defer malformado | diagnóstico sin corromper contador/contexto | PR |
| TP-PAR-418 | CP-PAR-035 | try/catch | TryStatement y CatchClause | PR |
| TP-PAR-419 | CP-PAR-035 | múltiples catches | orden exacto | PR |
| TP-PAR-420 | CP-PAR-035 | catch type/name/filter | campos y expresión exactos | PR |
| TP-PAR-421 | CP-PAR-035 | try/finally | third/child contractual exacto | PR |
| TP-PAR-422 | CP-PAR-035 | try sin catch/finally | diagnóstico | PR |
| TP-PAR-423 | CP-PAR-035 | throw expresión | ThrowStatement child | PR |
| TP-PAR-424 | CP-PAR-035 | rethrow | ThrowStatement sin child | PR |
| TP-PAR-425 | CP-PAR-035 | using declaration/body | recurso evaluado una vez y body | PR |
| TP-PAR-426 | CP-PAR-035 | malformed cleanup seguido de return | recuperación preserva return | PR |

## Expresiones

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-427 | CP-PAR-036 | cada assignment simple/compuesto | AssignmentExpression/operator | PR |
| TP-PAR-428 | CP-PAR-036 | `a=b=c` | asociación derecha | PR |
| TP-PAR-429 | CP-PAR-036 | ternario simple/nested | ConditionalExpression y asociación | PR |
| TP-PAR-430 | CP-PAR-036 | `a ?? b ?? c` | asociación derecha | PR |
| TP-PAR-431 | CP-PAR-036 | ranges `..`, `..=`, endpoints omitidos | RangeExpression/flags | PR |
| TP-PAR-432 | CP-PAR-036 | logical or/and | precedencia relativa | PR |
| TP-PAR-433 | CP-PAR-036 | bitwise or/xor/and | precedencia relativa | PR |
| TP-PAR-434 | CP-PAR-036 | equality vs relational | jerarquía exacta | PR |
| TP-PAR-435 | CP-PAR-036 | shift vs additive | jerarquía exacta | PR |
| TP-PAR-436 | CP-PAR-036 | additive vs multiplicative | jerarquía exacta | PR |
| TP-PAR-437 | CP-PAR-036 | cadena same-level | asociación izquierda | PR |
| TP-PAR-438 | CP-PAR-036 | paréntesis | override de precedencia | PR |
| TP-PAR-439 | CP-PAR-036 | `result? + 1` | postfix propagation, no ternary | PR |
| TP-PAR-440 | CP-PAR-036 | `cond ? +1 : 0` | ternary por colon same-level | PR |
| TP-PAR-441 | CP-PAR-036 | ternario nested con delimitadores | colon ownership exacto | PR |
| TP-PAR-442 | CP-PAR-036 | comma/semicolon/cierre antes de colon | postfix question | PR |
| TP-PAR-443 | CP-PAR-036 | operando faltante en cada nivel | ErrorExpression/diagnóstico/progreso | PR |
| TP-PAR-444 | CP-PAR-036 | mezcla de todos los niveles | AST dorado completo | NIGHTLY |
| TP-PAR-445 | CP-PAR-037 | lambda `()=>expr` | LambdaExpression, params/body | PR |
| TP-PAR-446 | CP-PAR-037 | lambda block body | body BlockStatement | PR |
| TP-PAR-447 | CP-PAR-037 | parámetros auto/tipados | ParameterDeclaration exactos | PR |
| TP-PAR-448 | CP-PAR-037 | captures vacías/value/ref/move | capture nodes/modos | PR |
| TP-PAR-449 | CP-PAR-037 | generic lambda/where | params/constraints | PR |
| TP-PAR-450 | CP-PAR-037 | return type explícito | TypeSyntax exacta | PR |
| TP-PAR-451 | CP-PAR-037 | async/unsafe/noheap según sintaxis vigente | flags exactos | PR |
| TP-PAR-452 | CP-PAR-037 | paréntesis que no es lambda | lookahead false y expresión normal | PR |
| TP-PAR-453 | CP-PAR-037 | arrow ausente | no falso positivo | PR |
| TP-PAR-454 | CP-PAR-037 | delimitadores nested en lookahead | encuentra arrow correcto | PR |
| TP-PAR-455 | CP-PAR-037 | lambda malformada | diagnóstico y progreso | PR |
| TP-PAR-456 | CP-PAR-037 | nested lambdas | scopes/rangos exactos | PR |
| TP-PAR-457 | CP-PAR-038 | `+ - ! ~ ++ --` prefijos | UnaryExpression/operator | PR |
| TP-PAR-458 | CP-PAR-038 | dereference/address/reference forms | operator/flags exactos | PR |
| TP-PAR-459 | CP-PAR-038 | move | MoveExpression | PR |
| TP-PAR-460 | CP-PAR-038 | from-end `^` | FromEndExpression | PR |
| TP-PAR-461 | CP-PAR-038 | cast estilo paréntesis | CastExpression/type/value | PR |
| TP-PAR-462 | CP-PAR-038 | static_cast | CastExpression marker exacto | PR |
| TP-PAR-463 | CP-PAR-038 | reinterpret_cast | CastExpression marker exacto | PR |
| TP-PAR-464 | CP-PAR-038 | const_cast | CastExpression marker exacto | PR |
| TP-PAR-465 | CP-PAR-038 | safe/type-test casts vigentes | SafeCast/TypeTest kinds | PR |
| TP-PAR-466 | CP-PAR-038 | sizeof expr/type | kind correcto por lookahead | PR |
| TP-PAR-467 | CP-PAR-038 | alignof expr/type | kind correcto | PR |
| TP-PAR-468 | CP-PAR-038 | offsetof | type/member exactos | PR |
| TP-PAR-469 | CP-PAR-038 | default y default(T) | DefaultValueExpression | PR |
| TP-PAR-470 | CP-PAR-038 | new object/array/placement | placement kind/storage/type/arg count/arg value/cursor EOF/diagnostics exactos | PR |
| TP-PAR-471 | CP-PAR-038 | delete/destroy | kind/operand | PR |
| TP-PAR-472 | CP-PAR-038 | stackalloc | type/count/initializer | PR |
| TP-PAR-473 | CP-PAR-038 | unary chain | asociación derecha y rango | PR |
| TP-PAR-474 | CP-PAR-038 | operand faltante | diagnóstico/progreso | PR |
| TP-PAR-475 | CP-PAR-038 | profundidad grande | resultado o límite/fallo caracterizado | NIGHTLY |
| TP-PAR-476 | CP-PAR-038 | void allocation inválida | InvalidType | PR |
| TP-PAR-477 | CP-PAR-039 | call sin/con args | CallExpression children | PR |
| TP-PAR-478 | CP-PAR-039 | named/ref/move/range args | argument and range metadata exacta | PR |
| TP-PAR-479 | CP-PAR-039 | generic call | typeSyntaxes y args | PR |
| TP-PAR-480 | CP-PAR-039 | index uno/múltiple | IndexExpression indices | PR |
| TP-PAR-481 | CP-PAR-039 | slice/range index | RangeExpression child | PR |
| TP-PAR-482 | CP-PAR-039 | null conditional member | kind/text/range | PR |
| TP-PAR-483 | CP-PAR-039 | dot member | MemberExpression | PR |
| TP-PAR-484 | CP-PAR-039 | arrow member | synthetic dereference + auxiliary `arrow` | PR |
| TP-PAR-485 | CP-PAR-039 | arrow receiver no pointer sintácticamente | parser conserva forma para diagnóstico Sema | PR |
| TP-PAR-486 | CP-PAR-039 | postfix ++/-- | PostfixExpression | PR |
| TP-PAR-487 | CP-PAR-039 | postfix result `?` | ResultPropagationExpression | PR |
| TP-PAR-488 | CP-PAR-039 | `with {}` | WithExpression source y assignments | PR |
| TP-PAR-489 | CP-PAR-039 | with múltiples/trailing comma | names/values en orden | PR |
| TP-PAR-490 | CP-PAR-039 | member/index/call chain | asociación izquierda exacta | PR |
| TP-PAR-491 | CP-PAR-039 | missing member/index close/arg | diagnóstico y progreso | PR |
| TP-PAR-492 | CP-PAR-039 | question disambiguation nested | CP-PAR-036 preservado | PR |
| TP-PAR-493 | CP-PAR-039 | static generic receiver chain | lookahead CP-PAR-011 correcto | PR |
| TP-PAR-494 | CP-PAR-039 | chain profunda | rango desde base hasta último postfix | NIGHTLY |
| TP-PAR-495 | CP-PAR-040 | cada literal | kind, value, range | PR |
| TP-PAR-496 | CP-PAR-040 | identifier/this/base/null | kind/text exactos | PR |
| TP-PAR-497 | CP-PAR-040 | parenthesized expression | child/rango contractual | PR |
| TP-PAR-498 | CP-PAR-040 | tuple vacío/unario/múltiple | TupleExpression/children | PR |
| TP-PAR-499 | CP-PAR-040 | match/loop/comprehension primary | dispatch correcto | PR |
| TP-PAR-500 | CP-PAR-040 | interpolated sin holes | StringLiteral/Interpolated kind vigente | PR |
| TP-PAR-501 | CP-PAR-040 | un hole simple | fragmento relexeado y AST insertado | PR |
| TP-PAR-502 | CP-PAR-040 | múltiples holes y texto | orden/segmentos exactos | PR |
| TP-PAR-503 | CP-PAR-040 | llaves escapadas | texto literal, no hole | PR |
| TP-PAR-504 | CP-PAR-040 | formato/alineación vigente | metadata exacta | PR |
| TP-PAR-505 | CP-PAR-040 | hole con expresión nested | delimitadores y AST exactos | PR |
| TP-PAR-506 | CP-PAR-040 | hole con token sobrante | diagnóstico por consumo incompleto | PR |
| TP-PAR-507 | CP-PAR-040 | hole léxicamente inválido | diagnóstico trasladado a rango exterior | PR |
| TP-PAR-508 | CP-PAR-040 | string inválido | InvalidStringLiteral/rango | PR |
| TP-PAR-509 | CP-PAR-040 | token inesperado | ExpectedExpression y ErrorExpression | PR |
| TP-PAR-510 | CP-PAR-040 | EOF como primary | error sin rebasa cursor | PR |
| TP-PAR-511 | CP-PAR-040 | interpolación nested/profunda | offsets y finalización correctos | NIGHTLY |
| TP-PAR-512 | CP-PAR-040 | literal seguido de postfix | primary entrega control sin consumir postfix | PR |

## Síntesis, estado y robustez

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-513 | CP-PAR-041 | cada helper `make*` | kind, children, text, operator y range | PR |
| TP-PAR-514 | CP-PAR-041 | clone TypeSyntax con todos los fields | copia profunda estructural | PR |
| TP-PAR-515 | CP-PAR-041 | clone AstNode con first/second/third/children | copia profunda sin alias de nodos | PR |
| TP-PAR-516 | CP-PAR-041 | clone generic constraints/projections | todos los tipos reubicados en arena | PR |
| TP-PAR-517 | CP-PAR-041 | clone null | null | PR |
| TP-PAR-518 | CP-PAR-041 | FP mode sobre árbol Inherit | todos los descendientes reciben mode | PR |
| TP-PAR-519 | CP-PAR-041 | child con override FP | rama no sobrescrita ni recorrida | PR |
| TP-PAR-520 | CP-PAR-041 | signed integer 0/MAX | valores exactos | PR |
| TP-PAR-521 | CP-PAR-041 | unary minus e INT64_MIN | valor exacto | PR |
| TP-PAR-522 | CP-PAR-041 | magnitud fuera de int64 | nullopt | PR |
| TP-PAR-523 | CP-PAR-041 | no literal | nullopt | PR |
| TP-PAR-524 | CP-PAR-041 | operator spelling/name por kind/aridad | mappings exactos y op_Invalid fallback | PR |
| TP-PAR-525 | CP-PAR-042 | parser con tokens/arena/engine vivos | no ownership ni copias indebidas | PR |
| TP-PAR-526 | CP-PAR-042 | dos instancias sobre mismos tokens, arenas distintas | árboles equivalentes sin alias | PR |
| TP-PAR-527 | CP-PAR-042 | contador match | nombres monotónicos y deterministas | PR |
| TP-PAR-528 | CP-PAR-042 | contador defer | nombres monotónicos y LIFO | PR |
| TP-PAR-529 | CP-PAR-042 | contador/contexto comprehension | push/pop y nombres exactos | PR |
| TP-PAR-530 | CP-PAR-042 | extension body con error | flag restaurado al salir | PR |
| TP-PAR-531 | CP-PAR-042 | pending generic greater tras error | no contamina declaración posterior | PR |
| TP-PAR-532 | CP-PAR-042 | nesting de tipos/patrones/unary | salida determinista al límite práctico | NIGHTLY |
| TP-PAR-533 | CP-PAR-042 | AST profundo para clone/FP | éxito o límite/fallo explícitamente caracterizado | NIGHTLY |
| TP-PAR-534 | CP-PAR-042 | entrada adversarial con sólo delimitadores | termina con progreso finito | PR |
| TP-PAR-535 | CP-PAR-042 | diagnostics engine con presupuesto agotado | parser termina aunque se supriman mensajes | PR |
| TP-PAR-536 | CP-PAR-042 | fuzz determinista de tokens balanceados/no balanceados | sin hang; rangos dentro de fuente | NIGHTLY |

## Cierre iterativo de `comprehend`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TP-PAR-571 | CP-PAR-033 | golden, LIFO, ordinales, 32768 niveles y recovery | AST y estado exactos sin recursión | NIGHTLY |

## Criterios de cierre futuro

1. Cada fila debe materializarse en la nueva suite o quedar explícitamente bloqueada con productor y evidencia.
2. Cada contrato debe tener al menos un caso positivo, uno negativo y cada frontera enumerada aplicable.
3. El oráculo compara estructura; no se permite aprobar sólo por exit code o ausencia de crash.
4. Los casos PR no deben compilar un ejecutable por fila: se agrupan en un módulo Parser y fixtures de texto.
5. Los casos NIGHTLY profundos no bloquean la iteración local hasta que la Fase 2 autorice su implementación.
6. Coverage instrumental y mutation testing pertenecen a Fase 3 y no se infieren de este diseño.
