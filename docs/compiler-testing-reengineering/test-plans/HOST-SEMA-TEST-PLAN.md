# Plan de pruebas nuevo — Host.Sema

## Alcance

Este plan diseña pruebas nuevas para los contratos congelados en `components/HOST-SEMA.md`. Los casos son
matrices parametrizables, no ejecutables individuales ni una cuota artificial. No se implementó ni ejecutó
ninguno durante la Fase 1. Los casos que requieren AST se construirán con factories del futuro harness, sin
reutilizar estado entre filas.

## Reglas de oráculo

- comparar valores, tipos internados, flags, orden, diagnóstico y estado posterior;
- probar aceptación, rechazo y borde de cada tabla completa;
- no depender del orden de `unordered_map`;
- no convertir diagnósticos actuales en política deseada: se caracteriza la revisión congelada;
- `NIGHTLY` reserva profundidad, ciclos fabricados o matrices combinatorias costosas, no contratos esenciales.

| ID | Contrato | Entrada / preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TS-SEMA-001 | CS-SEMA-001 | `ConstantValue{}` | kind None y payloads cero/falso/vacío | PR |
| TS-SEMA-002 | CS-SEMA-001 | factories signed con min,-1,0,1,max | kind y bits exactos | PR |
| TS-SEMA-003 | CS-SEMA-001 | factories unsigned con 0,1,max | kind y valor exactos | PR |
| TS-SEMA-004 | CS-SEMA-001 | floating con ±0, finitos, infinidades y NaN | kind y payload IEEE conservado | PR |
| TS-SEMA-005 | CS-SEMA-001 | boolean false/true y nullValue | discriminante y payload exactos | PR |
| TS-SEMA-006 | CS-SEMA-001 | string vacía, ASCII, UTF-8 y embedded NUL | bytes y lifetime de referencia exactos | PR |
| TS-SEMA-007 | CS-SEMA-001 | leer cada getter bajo cada discriminante | canales activos e inactivos caracterizados | PR |
| TS-SEMA-008 | CS-SEMA-001 | copy/move de cada clase de constante | discriminante y payload preservados | PR |
| TS-SEMA-009 | CS-SEMA-002 | `ToString` sobre los 13 kinds | spellings exactos y únicos | PR |
| TS-SEMA-010 | CS-SEMA-002 | `ToString` con enum externo | `unknown` | PR |
| TS-SEMA-011 | CS-SEMA-002 | default `ConversionInfo` | identity/contextual/null/null/-1 | PR |
| TS-SEMA-012 | CS-SEMA-002 | `ConversionInfo` por cada role e índice | todos los campos preservados | PR |
| TS-SEMA-013 | CS-SEMA-002 | conversion info con operand/type null parciales | registro pasivo sin normalización | PR |
| TS-SEMA-014 | CS-SEMA-002 | tabla kind × role | independencia entre clasificación y uso | PR |
| TS-SEMA-015 | CS-SEMA-003 | nombre de None/Owned/Borrowed | `none`/`owned`/`borrowed` | PR |
| TS-SEMA-016 | CS-SEMA-003 | enum externo | fallback `none` vigente | PR |
| TS-SEMA-017 | CS-SEMA-003 | default y asignación de OwnershipKind | identidad exacta | PR |
| TS-SEMA-018 | CS-SEMA-004 | `EffectSet{}` | bits 0 y ningún effect contenido | PR |
| TS-SEMA-019 | CS-SEMA-004 | constructor por cada effect | sólo el bit correspondiente | PR |
| TS-SEMA-020 | CS-SEMA-004 | `with` acumulativo y repetido | copia original intacta, unión idempotente | PR |
| TS-SEMA-021 | CS-SEMA-004 | effectName para cinco effects y externo | tabla exacta y `unknown` | PR |
| TS-SEMA-022 | CS-SEMA-004 | participation para cinco effects | sólo Throws/Async/NoHeap true | PR |
| TS-SEMA-023 | CS-SEMA-004 | contexto local permite/rechaza cada effect | allows y require duales | PR |
| TS-SEMA-024 | CS-SEMA-004 | cadena padre de profundidad 1,2,N | primera autorización visible | PR |
| TS-SEMA-025 | CS-SEMA-004 | shadow de sets en cadena | unión lógica sin mutar padres | PR |
| TS-SEMA-026 | CS-SEMA-004 | cadena muy profunda acíclica | conducta/límite vigente caracterizado | NIGHTLY |
| TS-SEMA-027 | CS-SEMA-005 | numeric identity y aliases de ValueCategory | Value=0, Place=1, aliases iguales | PR |
| TS-SEMA-028 | CS-SEMA-005 | `isPlaceCategory` sobre todas las categorías | true sólo Place/BorrowedValue | PR |
| TS-SEMA-029 | CS-SEMA-005 | `isUsableValueCategory` completo | false sólo MovedFrom | PR |
| TS-SEMA-030 | CS-SEMA-005 | `requiresMaterialization` completo | true sólo Temporary | PR |
| TS-SEMA-031 | CS-SEMA-005 | nombres de categorías y enum externo | spelling exacto y fallback `value` | PR |
| TS-SEMA-032 | CS-SEMA-005 | default `ExpressionInfo` | null/value/flags false/constant None | PR |
| TS-SEMA-033 | CS-SEMA-005 | ExpressionInfo con cada flag y símbolo | campos ortogonales preservados | PR |
| TS-SEMA-034 | CS-SEMA-005 | default ResolvedOperatorInfo | invalid/null/null/null/null/false | PR |
| TS-SEMA-035 | CS-SEMA-005 | operador unario y binario completos | token, tipos y unary preservados | PR |
| TS-SEMA-036 | CS-SEMA-006 | scope raíz vacío | parent/null y lookups null | PR |
| TS-SEMA-037 | CS-SEMA-006 | declare símbolo único | true y lookup local/global exacto | PR |
| TS-SEMA-038 | CS-SEMA-006 | duplicado por mismo nombre distinto puntero | false y original retenido | PR |
| TS-SEMA-039 | CS-SEMA-006 | child encuentra símbolo del padre | lookup jerárquico | PR |
| TS-SEMA-040 | CS-SEMA-006 | child shadow local directo | lookup retorna local; parent intacto | PR |
| TS-SEMA-041 | CS-SEMA-006 | lookupLocal no sube al padre | null local, valor en lookup | PR |
| TS-SEMA-042 | CS-SEMA-006 | nombres vacío, UTF-8 y case variants | comparación exacta de bytes | PR |
| TS-SEMA-043 | CS-SEMA-006 | cadena de scopes profunda | búsqueda iterativa y miss estable | NIGHTLY |
| TS-SEMA-044 | CS-SEMA-007 | nombres de ocho SymbolKind | tabla exacta | PR |
| TS-SEMA-045 | CS-SEMA-007 | SymbolKind externo | fallback `Symbol` | PR |
| TS-SEMA-046 | CS-SEMA-007 | construcción de cada kind | kind/name/type/declaration exactos | PR |
| TS-SEMA-047 | CS-SEMA-007 | flags const/defined/extern C false→true→false | transiciones independientes | PR |
| TS-SEMA-048 | CS-SEMA-007 | enumValue min,0,max | setter/getter exactos | PR |
| TS-SEMA-049 | CS-SEMA-007 | field offset ausente | has=false y getter=0 | PR |
| TS-SEMA-050 | CS-SEMA-007 | field offset 0 y max presente | has=true distingue cero presente | PR |
| TS-SEMA-051 | CS-SEMA-007 | constant value ausente/presente | flag y payload exactos | PR |
| TS-SEMA-052 | CS-SEMA-007 | members vacíos y orden A,B,A | orden/duplicados preservados | PR |
| TS-SEMA-053 | CS-SEMA-007 | findMember hit primero y miss | primera coincidencia o null | PR |
| TS-SEMA-054 | CS-SEMA-008 | typeKindName sobre 22 kinds | spellings exactos | PR |
| TS-SEMA-055 | CS-SEMA-008 | TypeKind externo | `<error>` | PR |
| TS-SEMA-056 | CS-SEMA-008 | construcción por cada kind | kind, ownership none y defaults exactos | PR |
| TS-SEMA-057 | CS-SEMA-008 | wrapper Owned/Borrowed | kind/base/ownership exactos | PR |
| TS-SEMA-058 | CS-SEMA-008 | unqualifiedType const/nonconst | base o self exacto | PR |
| TS-SEMA-059 | CS-SEMA-008 | setters en tipo no calificado | cada campo cambia aisladamente | PR |
| TS-SEMA-060 | CS-SEMA-008 | setters vía wrapper calificado | mutación delegada al base compartido | PR |
| TS-SEMA-061 | CS-SEMA-008 | display de 14 builtins, error y nominales | texto exacto | PR |
| TS-SEMA-062 | CS-SEMA-008 | display pointer/array/string literal | composición exacta | PR |
| TS-SEMA-063 | CS-SEMA-008 | display function 0/1/N params | firma exacta y orden | PR |
| TS-SEMA-064 | CS-SEMA-008 | completeness/declaration nominal | estado y delegación exactos | PR |
| TS-SEMA-065 | CS-SEMA-008 | payloads count 0/1/max | conservación exacta | PR |
| TS-SEMA-066 | CS-SEMA-009 | constructor de TypeContext | builtins requeridos no null y estables | PR |
| TS-SEMA-067 | CS-SEMA-009 | builtin por cada TypeKind | puntero o null exacto según tabla | PR |
| TS-SEMA-068 | CS-SEMA-009 | builtin con enum externo | null sin out-of-range | PR |
| TS-SEMA-069 | CS-SEMA-009 | qualified null/None/Error | Error o identidad exacta | PR |
| TS-SEMA-070 | CS-SEMA-009 | qualified same base+ownership repetido | misma identidad internada | PR |
| TS-SEMA-071 | CS-SEMA-009 | qualified Owned vs Borrowed | wrappers distintos, base común | PR |
| TS-SEMA-072 | CS-SEMA-009 | pointerTo repetido y pointees distintos | identidad estructural exacta | PR |
| TS-SEMA-073 | CS-SEMA-009 | arrayOf por element/count | reuse sólo con ambos iguales | PR |
| TS-SEMA-074 | CS-SEMA-009 | stringLiteral por counts 0/1/N | interning por count | PR |
| TS-SEMA-075 | CS-SEMA-009 | functionType por return/order/arity | igualdad estructural completa | PR |
| TS-SEMA-076 | CS-SEMA-009 | createStruct/createEnum mismo nombre repetido | identidades nominales nuevas e incompletas | PR |
| TS-SEMA-077 | CS-SEMA-009 | destrucción con grafo de tipos internados | ownership único sin doble liberación | NIGHTLY |
| TS-SEMA-078 | CS-SEMA-010 | getters en modelo vacío | null/spans vacíos | PR |
| TS-SEMA-079 | CS-SEMA-010 | set expression y reemplazo mismo nodo | último valor visible | PR |
| TS-SEMA-080 | CS-SEMA-010 | addConversion 0/1/N | orden de inserción exacto | PR |
| TS-SEMA-081 | CS-SEMA-010 | conversiones en owners distintos | aislamiento por nodo | PR |
| TS-SEMA-082 | CS-SEMA-010 | setOperator y reemplazo | último operador visible | PR |
| TS-SEMA-083 | CS-SEMA-010 | bindSymbol válido | identidad exacta | PR |
| TS-SEMA-084 | CS-SEMA-010 | bindSymbol con nodo o símbolo null | no crea entrada | PR |
| TS-SEMA-085 | CS-SEMA-010 | cuatro mapas para mismo nodo | estados coexistentes independientes | PR |
| TS-SEMA-086 | CS-SEMA-010 | nodos distintos con direcciones reutilizadas controladas | key por identidad de puntero actual | NIGHTLY |
| TS-SEMA-087 | CS-SEMA-011 | analyze con null/no CompilationUnit | rechazo/diagnóstico vigente | PR |
| TS-SEMA-088 | CS-SEMA-011 | CompilationUnit vacío | scope global y resultado exactos | PR |
| TS-SEMA-089 | CS-SEMA-011 | global dispatch para cinco kinds soportados | ruta propietaria invocada | PR |
| TS-SEMA-090 | CS-SEMA-011 | ClassDeclaration | InvalidType vigente | PR |
| TS-SEMA-091 | CS-SEMA-011 | nodo global no declaración | ExpectedDeclaration | PR |
| TS-SEMA-092 | CS-SEMA-011 | globals preservan source order | `globalSymbols` exacto | PR |
| TS-SEMA-093 | CS-SEMA-011 | entry point ausente/no function/no definition | MissingMain por matriz | PR |
| TS-SEMA-094 | CS-SEMA-011 | analyzer destruido tras símbolos/scopes | ownership sin fuga/doble delete | NIGHTLY |
| TS-SEMA-095 | CS-SEMA-012 | create/declare símbolo global | ownership, lista y lookup exactos | PR |
| TS-SEMA-096 | CS-SEMA-012 | duplicado mismo scope | DuplicateSymbol + previous declaration | PR |
| TS-SEMA-097 | CS-SEMA-012 | shadow variable sobre variable padre | conducta vigente aceptada | PR |
| TS-SEMA-098 | CS-SEMA-012 | shadow sobre struct/enum visible | DuplicateSymbol vigente | PR |
| TS-SEMA-099 | CS-SEMA-012 | scopes de block y for | entrada/salida restaura current scope | PR |
| TS-SEMA-100 | CS-SEMA-012 | lookup hit local/padre/miss | prioridad exacta | PR |
| TS-SEMA-101 | CS-SEMA-012 | diagnóstico con declaration previa null/no null | nota sólo cuando procede | PR |
| TS-SEMA-102 | CS-SEMA-013 | struct vacío | símbolo/tipo completo y layout vigente | PR |
| TS-SEMA-103 | CS-SEMA-013 | fields escalares variados | offsets, padding, alignment y orden | PR |
| TS-SEMA-104 | CS-SEMA-013 | nested struct completo | layout transitivo exacto | PR |
| TS-SEMA-105 | CS-SEMA-013 | field duplicado | diagnóstico y primer member retenido | PR |
| TS-SEMA-106 | CS-SEMA-013 | field void/function | InvalidType | PR |
| TS-SEMA-107 | CS-SEMA-013 | field struct incompleto | IncompleteType | PR |
| TS-SEMA-108 | CS-SEMA-013 | layout de field no disponible | sin offset inventado | PR |
| TS-SEMA-109 | CS-SEMA-013 | overflow de layout/alignment | diagnóstico/estado vigente | PR |
| TS-SEMA-110 | CS-SEMA-013 | recursive nominal directo/indirecto | rechazo por incompletitud | PR |
| TS-SEMA-111 | CS-SEMA-013 | type marcado completo tras diagnósticos | postestado exacto | PR |
| TS-SEMA-112 | CS-SEMA-014 | enum vacío | tipo nominal Int y estado exacto | PR |
| TS-SEMA-113 | CS-SEMA-014 | miembros auto 0,1,2 | valores/constantes exactos | PR |
| TS-SEMA-114 | CS-SEMA-014 | initializer signed/unsigned representable | conversión exacta | PR |
| TS-SEMA-115 | CS-SEMA-014 | referencia a miembro anterior | lookup y valor exactos | PR |
| TS-SEMA-116 | CS-SEMA-014 | referencia a miembro posterior/desconocido | diagnóstico exacto | PR |
| TS-SEMA-117 | CS-SEMA-014 | initializer no integral/no constante | InvalidEnumInitializer | PR |
| TS-SEMA-118 | CS-SEMA-014 | miembro duplicado | DuplicateSymbol y estado retenido | PR |
| TS-SEMA-119 | CS-SEMA-014 | auto tras INT64_MAX | overflow diagnosticado | PR |
| TS-SEMA-120 | CS-SEMA-014 | underlying type explícito | rechazo vigente de ruta single-file | PR |
| TS-SEMA-121 | CS-SEMA-015 | función void/valor, 0/1/N params válidos | tipo internado y símbolos exactos | PR |
| TS-SEMA-122 | CS-SEMA-015 | retorno array | InvalidType | PR |
| TS-SEMA-123 | CS-SEMA-015 | params void/array/incompleto | diagnóstico por posición | PR |
| TS-SEMA-124 | CS-SEMA-015 | declaración repetida idéntica sin body | símbolo reutilizado | PR |
| TS-SEMA-125 | CS-SEMA-015 | redeclaración type/linkage conflictivo | ConflictingFunctionDeclaration | PR |
| TS-SEMA-126 | CS-SEMA-015 | definición tras declaración | defined cambia una vez | PR |
| TS-SEMA-127 | CS-SEMA-015 | segunda definición | DuplicateFunctionDefinition | PR |
| TS-SEMA-128 | CS-SEMA-015 | extern C con body | rechazo y texto vigente | PR |
| TS-SEMA-129 | CS-SEMA-015 | C ABI scalars/pointers/enums | aceptación completa | PR |
| TS-SEMA-130 | CS-SEMA-015 | C ABI structs/arrays válidos e inválidos | matriz transitiva | PR |
| TS-SEMA-131 | CS-SEMA-015 | Main firmas válidas e inválidas | InvalidMainSignature sólo inválidas | PR |
| TS-SEMA-132 | CS-SEMA-015 | Main ausente/declarado/definido | entry-point gate exacto | PR |
| TS-SEMA-133 | CS-SEMA-015 | nonvoid body con/sin fallthrough | NotAllPathsReturn exacto | PR |
| TS-SEMA-134 | CS-SEMA-015 | parámetros y body scope | lookup y restauración exactos | PR |
| TS-SEMA-135 | CS-SEMA-015 | diagnósticos con strings de edición vigentes | texto exacto caracterizado | PR |
| TS-SEMA-136 | CS-SEMA-016 | global sin initializer | símbolo/type/flags exactos | PR |
| TS-SEMA-137 | CS-SEMA-016 | global initializer convertible constante | conversión y constant payload | PR |
| TS-SEMA-138 | CS-SEMA-016 | global initializer incompatible | InvalidConversion | PR |
| TS-SEMA-139 | CS-SEMA-016 | global mutable no constante initializer | GlobalRequiresConstant vigente | PR |
| TS-SEMA-140 | CS-SEMA-016 | global const no constante | ConstRequiresConstant | PR |
| TS-SEMA-141 | CS-SEMA-016 | local sin initializer | símbolo declarado sin constant | PR |
| TS-SEMA-142 | CS-SEMA-016 | local convertible constante/no constante | conversión y flag exactos | PR |
| TS-SEMA-143 | CS-SEMA-016 | local const no constante | ConstRequiresConstant | PR |
| TS-SEMA-144 | CS-SEMA-016 | arrays locales permitidos vs globals/reglas | matriz allowArray exacta | PR |
| TS-SEMA-145 | CS-SEMA-016 | duplicados y shadowing | delegación a contrato de scopes | PR |
| TS-SEMA-146 | CS-SEMA-017 | block con/sin nested scope | visibilidad y restauración | PR |
| TS-SEMA-147 | CS-SEMA-017 | if condition bool/convertible/inválida | conversion o InvalidCondition | PR |
| TS-SEMA-148 | CS-SEMA-017 | while condition válida/inválida | diagnóstico y loopDepth | PR |
| TS-SEMA-149 | CS-SEMA-017 | for init/condition/increment/body | orden y scope exactos | PR |
| TS-SEMA-150 | CS-SEMA-017 | break/continue fuera/dentro de loop | diagnósticos exactos | PR |
| TS-SEMA-151 | CS-SEMA-017 | nested loops y loopDepth restoration | break/continue aceptados correctamente | PR |
| TS-SEMA-152 | CS-SEMA-017 | return fuera de función | InvalidReturn | PR |
| TS-SEMA-153 | CS-SEMA-017 | void return vacío/con valor | aceptación/rechazo exactos | PR |
| TS-SEMA-154 | CS-SEMA-017 | valued return vacío/incompatible/convertible | diagnóstico o conversión | PR |
| TS-SEMA-155 | CS-SEMA-017 | static_assert true/false | ausencia o StaticAssertionFailed | PR |
| TS-SEMA-156 | CS-SEMA-017 | static_assert no bool/no constante | InvalidCondition/ConstRequiresConstant | PR |
| TS-SEMA-157 | CS-SEMA-017 | inline asm constraints válidas/inválidas | validaciones y error final unavailable | PR |
| TS-SEMA-158 | CS-SEMA-017 | inline asm operands modifiables/no modificables | InvalidAssignmentTarget cuando procede | PR |
| TS-SEMA-159 | CS-SEMA-017 | expression statement | análisis y modelo conservados | PR |
| TS-SEMA-160 | CS-SEMA-017 | nodo statement desconocido | conducta/diagnóstico vigente | PR |
| TS-SEMA-161 | CS-SEMA-017 | restauración tras diagnóstico dentro de loop/function | contexto no contaminado | PR |
| TS-SEMA-162 | CS-SEMA-018 | identifier variable/parameter/global/field | tipo, categoría, flags y binding | PR |
| TS-SEMA-163 | CS-SEMA-018 | identifier function permitido/no permitido | designator o InvalidOperand | PR |
| TS-SEMA-164 | CS-SEMA-018 | identifier struct/enum como expresión | InvalidOperand | PR |
| TS-SEMA-165 | CS-SEMA-018 | identifier desconocido | UnknownIdentifier y Error info | PR |
| TS-SEMA-166 | CS-SEMA-018 | integer literals por bases/suffix/rangos | type y constant exactos | PR |
| TS-SEMA-167 | CS-SEMA-018 | integer literal malformed/overflow | InvalidNumericLiteral | PR |
| TS-SEMA-168 | CS-SEMA-018 | floating literals válidos/especiales/invalid | type/value o diagnóstico | PR |
| TS-SEMA-169 | CS-SEMA-018 | char/string incluido embedded NUL | type/count/constant exactos | PR |
| TS-SEMA-170 | CS-SEMA-018 | bool true/false y null | types y constants exactos | PR |
| TS-SEMA-171 | CS-SEMA-018 | ThisExpression | rechazo vigente | PR |
| TS-SEMA-172 | CS-SEMA-018 | PlacementConstructionExpression | diagnóstico unavailable | PR |
| TS-SEMA-173 | CS-SEMA-019 | address-of place mutable, const, rvalue y borrowed | pointer o InvalidAddressOf | PR |
| TS-SEMA-174 | CS-SEMA-019 | dereference pointer válido/void/incompleto/no pointer | place o InvalidDereference | PR |
| TS-SEMA-175 | CS-SEMA-019 | prefix/postfix inc/dec sobre matriz scalar | resultado/categoría/operator exactos | PR |
| TS-SEMA-176 | CS-SEMA-019 | inc/dec target no modificable | InvalidAssignmentTarget | PR |
| TS-SEMA-177 | CS-SEMA-019 | unary ! con conversion bool válida/inválida | bool/operator o error | PR |
| TS-SEMA-178 | CS-SEMA-019 | unary ~ sobre todos enteros/promotions | type, conversion y constant exactos | PR |
| TS-SEMA-179 | CS-SEMA-019 | unary +/- sobre int/float/no numeric | type/operator o error | PR |
| TS-SEMA-180 | CS-SEMA-019 | `&&`/`\|\|` truth table constante | short-circuit y resultado exactos | PR |
| TS-SEMA-181 | CS-SEMA-019 | `&&`/`\|\|` con operand inválido | InvalidOperand por lado | PR |
| TS-SEMA-182 | CS-SEMA-019 | equality numeric completa | common type, conversions y result bool | PR |
| TS-SEMA-183 | CS-SEMA-019 | equality pointer/null/void pointer | compatibilidad exacta | PR |
| TS-SEMA-184 | CS-SEMA-019 | relational numeric y pointers | aceptación/rechazo exactos | PR |
| TS-SEMA-185 | CS-SEMA-019 | bitwise sobre todos pares enteros | promotions/common type/constants | PR |
| TS-SEMA-186 | CS-SEMA-019 | shifts por width/sign/count | operation type y trap contractual | PR |
| TS-SEMA-187 | CS-SEMA-019 | pointer + integer e integer + pointer | result pointer y conversions | PR |
| TS-SEMA-188 | CS-SEMA-019 | pointer - integer | result pointer | PR |
| TS-SEMA-189 | CS-SEMA-019 | pointer - pointer compatible/incompatible | nint o InvalidOperand | PR |
| TS-SEMA-190 | CS-SEMA-019 | modulo integer/no integer | result o InvalidOperand | PR |
| TS-SEMA-191 | CS-SEMA-019 | +,-,*,/ numeric matrix | common type y operator record | PR |
| TS-SEMA-192 | CS-SEMA-019 | operands Error | cascada suprimida y Error result | PR |
| TS-SEMA-193 | CS-SEMA-019 | token unario/binario desconocido | InvalidOperand vigente | PR |
| TS-SEMA-194 | CS-SEMA-019 | operator record por toda familia | token/tipos/unary exactos | PR |
| TS-SEMA-195 | CS-SEMA-020 | assignment simple válido | lhs result y conversion registrada | PR |
| TS-SEMA-196 | CS-SEMA-020 | assignment lhs no modificable | InvalidAssignmentTarget | PR |
| TS-SEMA-197 | CS-SEMA-020 | assignment rhs incompatible | InvalidConversion | PR |
| TS-SEMA-198 | CS-SEMA-020 | compound tokens 10→underlying | mapping exacto | PR |
| TS-SEMA-199 | CS-SEMA-020 | compound aritmético/bitwise/shift válido | conversion roles y result exactos | PR |
| TS-SEMA-200 | CS-SEMA-020 | compound operación inválida | InvalidOperand sin records espurios | PR |
| TS-SEMA-201 | CS-SEMA-020 | compound operation type distinto de lhs | conversión explícita Result | PR |
| TS-SEMA-202 | CS-SEMA-020 | call a function 0/1/N args | result y conversions por índice | PR |
| TS-SEMA-203 | CS-SEMA-020 | call callee no function | InvalidCall | PR |
| TS-SEMA-204 | CS-SEMA-020 | call argument count menor/mayor | InvalidArgumentCount; args analizados | PR |
| TS-SEMA-205 | CS-SEMA-020 | call argumento incompatible | InvalidConversion por índice | PR |
| TS-SEMA-206 | CS-SEMA-020 | call return void/value | ExpressionInfo exacta | PR |
| TS-SEMA-207 | CS-SEMA-021 | array fijo índice válido constante/dinámico | element place y bounds flag | PR |
| TS-SEMA-208 | CS-SEMA-021 | array multidimensional con N children | descenso por dimensión exacto | PR |
| TS-SEMA-209 | CS-SEMA-021 | índice no entero en cada dimensión | InvalidIndex | PR |
| TS-SEMA-210 | CS-SEMA-021 | array constant index -1,count,count+1 | ConstantIndexOutOfBounds | PR |
| TS-SEMA-211 | CS-SEMA-021 | string literal index válido/borde | char value y bounds flag | PR |
| TS-SEMA-212 | CS-SEMA-021 | pointer index | element place sin bounds flag | PR |
| TS-SEMA-213 | CS-SEMA-021 | index base no indexable | InvalidIndex | PR |
| TS-SEMA-214 | CS-SEMA-021 | index de pointer void/incompleto | InvalidIndex vigente | PR |
| TS-SEMA-215 | CS-SEMA-021 | direct member struct hit/miss | field info o UnknownMember | PR |
| TS-SEMA-216 | CS-SEMA-021 | direct member enum hit/miss | constant info o UnknownMember | PR |
| TS-SEMA-217 | CS-SEMA-021 | direct member en no aggregate | InvalidMemberAccess | PR |
| TS-SEMA-218 | CS-SEMA-021 | pointer member pointer-to-struct | field info exacta | PR |
| TS-SEMA-219 | CS-SEMA-021 | pointer member sobre struct/no pointer | InvalidMemberAccess | PR |
| TS-SEMA-220 | CS-SEMA-021 | mutabilidad/addressability del receiver | propagación exacta a field | PR |
| TS-SEMA-221 | CS-SEMA-021 | member binding en SemanticModel | símbolo exacto | PR |
| TS-SEMA-222 | CS-SEMA-021 | errors en base/índice | cascada controlada | PR |
| TS-SEMA-223 | CS-SEMA-022 | explicit conversion identity | clasificación/registro vigente | PR |
| TS-SEMA-224 | CS-SEMA-022 | numeric integral/floating matrix | kind y constant result exactos | PR |
| TS-SEMA-225 | CS-SEMA-022 | enum↔numeric y enum↔enum | aceptación/clasificación exacta | PR |
| TS-SEMA-226 | CS-SEMA-022 | pointer↔pointer | ExplicitPointer | PR |
| TS-SEMA-227 | CS-SEMA-022 | pointer↔integer | ExplicitPointerInteger | PR |
| TS-SEMA-228 | CS-SEMA-022 | null↔pointer y any→void | kinds exactos | PR |
| TS-SEMA-229 | CS-SEMA-022 | bool conversion cases | exclusiones exactas | PR |
| TS-SEMA-230 | CS-SEMA-022 | float constant in/out integer range | constant o ConstantEvaluationTrap | PR |
| TS-SEMA-231 | CS-SEMA-022 | record helpers identity/error | omisión exacta | PR |
| TS-SEMA-232 | CS-SEMA-022 | roles Contextual/Operand/Result e índices | ConversionInfo exacto | PR |
| TS-SEMA-233 | CS-SEMA-023 | sizeof type sobre cada supported kind | size target exacto | PR |
| TS-SEMA-234 | CS-SEMA-023 | alignof type sobre cada supported kind | alignment target exacto | PR |
| TS-SEMA-235 | CS-SEMA-023 | sizeof/alignof expression | operand analizado y traps suprimidos | PR |
| TS-SEMA-236 | CS-SEMA-023 | void/function/null/error | InvalidSizeof/estado exacto | PR |
| TS-SEMA-237 | CS-SEMA-023 | array count 0/1/N/overflow | layout o nullopt exacto | PR |
| TS-SEMA-238 | CS-SEMA-023 | struct fields con padding | size/alignment exactos | PR |
| TS-SEMA-239 | CS-SEMA-023 | incomplete struct | layout no disponible | PR |
| TS-SEMA-240 | CS-SEMA-023 | offsetof valid field offsets 0/N | constant nuint exacta | PR |
| TS-SEMA-241 | CS-SEMA-023 | offsetof no struct/incomplete | InvalidOffsetof | PR |
| TS-SEMA-242 | CS-SEMA-023 | offsetof unknown field/no offset | InvalidOffsetof | PR |
| TS-SEMA-243 | CS-SEMA-023 | target layout scalar/pointer variants | delegación exacta | PR |
| TS-SEMA-244 | CS-SEMA-023 | layout nested overflow | nullopt sin wrap | PR |
| TS-SEMA-245 | CS-SEMA-024 | new objeto válido | pointer al tipo exacto | PR |
| TS-SEMA-246 | CS-SEMA-024 | new void/function/incomplete | InvalidAllocationType | PR |
| TS-SEMA-247 | CS-SEMA-024 | new[] count integer 0/1/N | pointer y análisis exactos | PR |
| TS-SEMA-248 | CS-SEMA-024 | new[] count no integer | InvalidAllocationType | PR |
| TS-SEMA-249 | CS-SEMA-024 | new[] negativo/size overflow | ConstantEvaluationTrap y postestado | PR |
| TS-SEMA-250 | CS-SEMA-024 | delete pointer/null | void rvalue aceptado | PR |
| TS-SEMA-251 | CS-SEMA-024 | delete scalar/aggregate no pointer | InvalidDeleteOperand | PR |
| TS-SEMA-252 | CS-SEMA-024 | stackalloc tipo/count válidos | pointer exacto | PR |
| TS-SEMA-253 | CS-SEMA-024 | stackalloc type/count inválidos | InvalidStackalloc | PR |
| TS-SEMA-254 | CS-SEMA-024 | stackalloc negativo/overflow | trap y pointer/error state vigente | PR |
| TS-SEMA-255 | CS-SEMA-025 | resolve 14 builtin keywords | builtin exacto | PR |
| TS-SEMA-256 | CS-SEMA-025 | resolve struct/enum global | identidad nominal exacta | PR |
| TS-SEMA-257 | CS-SEMA-025 | unknown identifier/base syntax inválida | UnknownTypeName/ExpectedType | PR |
| TS-SEMA-258 | CS-SEMA-025 | pointer modifiers 1/N | nesting/order exactos | PR |
| TS-SEMA-259 | CS-SEMA-025 | fixed arrays positive constant | type/count exactos | PR |
| TS-SEMA-260 | CS-SEMA-025 | fixed arrays zero/negative/nonconstant | InvalidArrayCount | PR |
| TS-SEMA-261 | CS-SEMA-025 | fixed array count fuera de target | InvalidArrayCount | PR |
| TS-SEMA-262 | CS-SEMA-025 | ownership none/owned/borrowed | wrapper final exacto | PR |
| TS-SEMA-263 | CS-SEMA-025 | validateObjectType matrix kind×allowArray | aceptación/diagnóstico exactos | PR |
| TS-SEMA-264 | CS-SEMA-025 | C ABI direct type matrix | bool exacto | PR |
| TS-SEMA-265 | CS-SEMA-025 | C ABI struct vacío/incompleto/recursive | rechazo y terminación exactos | PR |
| TS-SEMA-266 | CS-SEMA-025 | C ABI field array/nested struct/error | compatibilidad transitiva exacta | PR |
| TS-SEMA-267 | CS-SEMA-026 | isInteger/signed/unsigned/floating/numeric por 22 kinds | matrices completas | PR |
| TS-SEMA-268 | CS-SEMA-026 | isPointer/scalarIncrement/sized por 22 kinds | matrices completas | PR |
| TS-SEMA-269 | CS-SEMA-026 | integerWidth por todos enteros y no enteros | 8/16/32/64/0 exacto | PR |
| TS-SEMA-270 | CS-SEMA-026 | promoteInteger por todos los kinds | Int o identidad exacta | PR |
| TS-SEMA-271 | CS-SEMA-026 | commonIntegralType producto cartesiano | candidate exacto o Error | PR |
| TS-SEMA-272 | CS-SEMA-026 | commonNumericType producto cartesiano | Double/Float/integral/Error | PR |
| TS-SEMA-273 | CS-SEMA-026 | constantFits signed/unsigned en límites de cada destino | true/false exacto | PR |
| TS-SEMA-274 | CS-SEMA-026 | implicit identity y promotion | clasificación exacta | PR |
| TS-SEMA-275 | CS-SEMA-026 | implicit constant integer | sólo fits y destination integer | PR |
| TS-SEMA-276 | CS-SEMA-026 | implicit numeric matrix | aceptación/clase exactas | PR |
| TS-SEMA-277 | CS-SEMA-026 | null→pointer, pointer→void*, string→char* | kinds exactos | PR |
| TS-SEMA-278 | CS-SEMA-026 | forbidden implicit pointer/numeric/bool cases | false exacto | PR |
| TS-SEMA-279 | CS-SEMA-026 | explicit conversion full matrix | clase exacta o rechazo | PR |
| TS-SEMA-280 | CS-SEMA-026 | Error source/destination | cascada/identity vigente | PR |
| TS-SEMA-281 | CS-SEMA-027 | maskForWidth 0,1,8,32,63,64 | máscara exacta | PR |
| TS-SEMA-282 | CS-SEMA-027 | signedFromBits/bitsFromSigned round-trip por widths | bits y signo exactos | PR |
| TS-SEMA-283 | CS-SEMA-027 | constantAsSigned por signed/unsigned/bool/otros | optional exacto | PR |
| TS-SEMA-284 | CS-SEMA-027 | constantAsUnsigned por signed no negativo/negativo/unsigned/bool | optional exacto | PR |
| TS-SEMA-285 | CS-SEMA-027 | convertConstant por destinos integer widths | truncación/extensión modular exacta | PR |
| TS-SEMA-286 | CS-SEMA-027 | convertConstant float/bool/enum/pointer cases | payload exacto vigente | PR |
| TS-SEMA-287 | CS-SEMA-027 | unary ! sobre bool/integer constants | resultado bool exacto | PR |
| TS-SEMA-288 | CS-SEMA-027 | unary - signed/unsigned/float límites | bits/trap exactos | PR |
| TS-SEMA-289 | CS-SEMA-027 | unary ~ y + | resultado exacto | PR |
| TS-SEMA-290 | CS-SEMA-027 | bool `&&`/`\|\|` truth table | resultado y short-circuit exactos | PR |
| TS-SEMA-291 | CS-SEMA-027 | equality/relational signed matrix | bool exacto | PR |
| TS-SEMA-292 | CS-SEMA-027 | equality/relational unsigned matrix | bool exacto | PR |
| TS-SEMA-293 | CS-SEMA-027 | add/sub/mul por widths con overflow | wrapping bit exacto | PR |
| TS-SEMA-294 | CS-SEMA-027 | signed/unsigned div/mod normales | quotient/remainder exactos | PR |
| TS-SEMA-295 | CS-SEMA-027 | divide/mod cero y min/-1 | trap exacto | PR |
| TS-SEMA-296 | CS-SEMA-027 | bitwise and/or/xor | bits exactos | PR |
| TS-SEMA-297 | CS-SEMA-027 | shifts 0,width-1,width y negativos modelados | resultado/trap exactos | PR |
| TS-SEMA-298 | CS-SEMA-027 | trap suppression en short-circuit/sizeof/alignof | diagnóstico omitido sólo dentro | PR |
| TS-SEMA-299 | CS-SEMA-028 | return/break/continue | previenen fallthrough | PR |
| TS-SEMA-300 | CS-SEMA-028 | block vacío y secuencias con terminador en posiciones | primer terminador domina | PR |
| TS-SEMA-301 | CS-SEMA-028 | if sin else/una rama/ambas ramas | true sólo ambas previenen | PR |
| TS-SEMA-302 | CS-SEMA-028 | while/for constant true sin break | previene | PR |
| TS-SEMA-303 | CS-SEMA-028 | loops false/dinámicos | no previenen | PR |
| TS-SEMA-304 | CS-SEMA-028 | break directo y en block/if | loop deja de prevenir | PR |
| TS-SEMA-305 | CS-SEMA-028 | break dentro de loop nested | no cuenta para loop exterior | PR |
| TS-SEMA-306 | CS-SEMA-028 | AST profundo acíclico | límite/stack vigente caracterizado | NIGHTLY |
| TS-SEMA-307 | CS-SEMA-029 | dump root null/model vacío | salida exacta vacía | PR |
| TS-SEMA-308 | CS-SEMA-029 | nodo con ExpressionInfo mínimo | formato, indent y edge exactos | PR |
| TS-SEMA-309 | CS-SEMA-029 | cada ValueCategory y flag | spellings exactos | PR |
| TS-SEMA-310 | CS-SEMA-029 | siete ConstantKind | representación exacta | PR |
| TS-SEMA-311 | CS-SEMA-029 | 0/1/N conversiones y roles | orden y campos exactos | PR |
| TS-SEMA-312 | CS-SEMA-029 | operador unario/binario | formato y tipos exactos | PR |
| TS-SEMA-313 | CS-SEMA-029 | árbol con children anotados/no anotados | traversal y omisión exactos | PR |
| TS-SEMA-314 | CS-SEMA-029 | AST profundo/cíclico fabricado | limitación vigente documentada | NIGHTLY |
| TS-SEMA-315 | CS-SEMA-030 | dump span vacío | salida vacía | PR |
| TS-SEMA-316 | CS-SEMA-030 | símbolo mínimo por cada kind | formato exacto | PR |
| TS-SEMA-317 | CS-SEMA-030 | flags, enum, offset y constant presentes/ausentes | campos condicionales exactos | PR |
| TS-SEMA-318 | CS-SEMA-030 | members 0/1/N | indent y orden de inserción | PR |
| TS-SEMA-319 | CS-SEMA-030 | múltiples globals | orden del span | PR |
| TS-SEMA-320 | CS-SEMA-030 | nombres/types complejos | texto exacto del display name | PR |
| TS-SEMA-321 | CS-SEMA-030 | árbol profundo/cíclico fabricado | limitación vigente documentada | NIGHTLY |

## Criterio de implementación futura

Cada fila es indivisible como contrato de matriz: una implementación puede parametrizar sus celdas, pero no
reducirlas a un ejemplo positivo. La promoción de `DESIGNED` a `IMPLEMENTED` exigirá referencia a archivo de
prueba, oráculo ejecutable y evidencia de ejecución; nada de eso se afirma en esta fase.
