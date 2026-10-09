# Test plan — Host.AST

## Estado

- **Clasificación:** `DISEÑO PROPUESTO`.
- **Implementación/ejecución:** ninguna.
- **Unidad lógica:** filas parametrizables dentro de módulos AST; no ejecutables separados.
- **Oráculos:** estado exacto, orden/lifetime/destrucción y dump byte a byte.

Los IDs `TA-AST-NNN` se asignan por orden canónico de filas.

## Arena y catálogo

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TA-AST-001 | CA-AST-001 | arena vacía | destructor no invoca deleter | PR |
| TA-AST-002 | CA-AST-001 | make de un objeto con argumentos | constructor recibe valores y puntero estable | PR |
| TA-AST-003 | CA-AST-001 | make de tipos heterogéneos | cada dynamic type se destruye correctamente | PR |
| TA-AST-004 | CA-AST-001 | tres objetos con registro de destructor | orden de destrucción C B A | PR |
| TA-AST-005 | CA-AST-001 | objetos con relaciones no propietarias | todos siguen vivos hasta destruir arena | PR |
| TA-AST-006 | CA-AST-001 | crecimiento que realoca entries | punteros de objetos no cambian | PR |
| TA-AST-007 | CA-AST-001 | destructor con contador | exactamente una destrucción por make exitoso | PR |
| TA-AST-008 | CA-AST-001 | arena en scope anidado | destrucción al salir del scope | PR |
| TA-AST-009 | CA-AST-001 | objeto de alineación alta | puntero cumple alignof(T) | PR |
| TA-AST-010 | CA-AST-001 | muchos objetos | orden LIFO completo | NIGHTLY |
| TA-AST-011 | CA-AST-002 | trait compile-time copy constructible | false | PR |
| TA-AST-012 | CA-AST-002 | trait compile-time copy assignable | false | PR |
| TA-AST-013 | CA-AST-002 | constructor de T lanza | excepción propagada y no entry fantasma | PR |
| TA-AST-014 | CA-AST-002 | fallo simulado de registro después de new | caracteriza fuga vigente | NIGHTLY |
| TA-AST-015 | CA-AST-002 | destructor noexcept normal | continúa con todos los entries | PR |
| TA-AST-016 | CA-AST-002 | destructor que lanza | caracteriza terminación/entries restantes según C++ | NIGHTLY |
| TA-AST-017 | CA-AST-002 | llamadas concurrentes sin sincronización | documenta ausencia de garantía; no se usa como caso positivo | NIGHTLY |
| TA-AST-018 | CA-AST-002 | destroy template por cada tipo | delete invoca destructor dinámico esperado | PR |
| TA-AST-019 | CA-AST-003 | los 100 valores válidos | nombre exacto igual al enumerador | PR |
| TA-AST-020 | CA-AST-003 | nombres válidos | todos únicos | PR |
| TA-AST-021 | CA-AST-003 | ordinales 0..99 | contiguos y en orden declarado | PR |
| TA-AST-022 | CA-AST-003 | valor -1 convertido al enum | `UnknownAstNode` | PR |
| TA-AST-023 | CA-AST-003 | valor 100 y máximo subyacente | `UnknownAstNode` | PR |
| TA-AST-024 | CA-AST-003 | primeros valores históricos | ordinales congelados exactos | PR |
| TA-AST-025 | CA-AST-003 | kinds appended de null-coalescing a loop | orden relativo exacto | PR |
| TA-AST-026 | CA-AST-003 | llamadas repetidas | string_view estable y sin asignación observable | PR |

## Records y AstNode

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TA-AST-027 | CA-AST-004 | GenericParameterSyntax default | kind Type, null constantType, demás defaults | PR |
| TA-AST-028 | CA-AST-004 | cuatro GenericParameterSyntaxKind | valores distintos y orden exacto | PR |
| TA-AST-029 | CA-AST-004 | record generic completo | campos conservados tras move/copy de record | PR |
| TA-AST-030 | CA-AST-004 | LifetimeParameterSyntax | name/range exactos | PR |
| TA-AST-031 | CA-AST-004 | LifetimeOutlivesConstraint | longer/shorter/range exactos | PR |
| TA-AST-032 | CA-AST-004 | cuatro FP modes | orden y valores distintos | PR |
| TA-AST-033 | CA-AST-004 | tres unwind policies | orden y valores distintos | PR |
| TA-AST-034 | CA-AST-004 | GenericConstraintClause default | flags false, pointers null, vectors vacíos | PR |
| TA-AST-035 | CA-AST-004 | cada capability aislada | sólo el bit seleccionado true | PR |
| TA-AST-036 | CA-AST-004 | todas capabilities | campos ortogonales true simultáneamente | PR |
| TA-AST-037 | CA-AST-004 | equality/projection/substituted pointers | identidad de puntero conservada | PR |
| TA-AST-038 | CA-AST-004 | typeConstraints con null/duplicado | record conserva exactamente el vector | PR |
| TA-AST-039 | CA-AST-004 | parameter/range y clause range distintos | ambos rangos independientes | PR |
| TA-AST-040 | CA-AST-004 | records fuera de coherencia semántica | AST no valida ni modifica | PR |
| TA-AST-041 | CA-AST-005 | constructor por cada kind/range | kind/range exactos | PR |
| TA-AST-042 | CA-AST-005 | setRange repetido | último rango reemplaza anterior | PR |
| TA-AST-043 | CA-AST-005 | text vacío/no vacío/Unicode bytes | string propietario exacto | PR |
| TA-AST-044 | CA-AST-005 | input text temporal | valor sobrevive al input | PR |
| TA-AST-045 | CA-AST-005 | auxiliaryText y argumentName | canales independientes | PR |
| TA-AST-046 | CA-AST-005 | TokenKind default | Invalid | PR |
| TA-AST-047 | CA-AST-005 | setTokenKind cada clase representativa | valor exacto | PR |
| TA-AST-048 | CA-AST-005 | strings cambiados varias veces | último valor y sin cruce de canales | PR |
| TA-AST-049 | CA-AST-005 | kind después de todos los setters | permanece inmutable | PR |
| TA-AST-050 | CA-AST-005 | rango inválido construido externamente | nodo conserva sin validar | PR |
| TA-AST-051 | CA-AST-006 | modifiers inicialmente vacíos | vector vacío y has false | PR |
| TA-AST-052 | CA-AST-006 | add one/multiple/duplicate modifier | orden/duplicados exactos | PR |
| TA-AST-053 | CA-AST-006 | has modifier presente/ausente | true/false | PR |
| TA-AST-054 | CA-AST-006 | typeSyntax default/set/null reset | identidad exacta | PR |
| TA-AST-055 | CA-AST-006 | throwsTypeSyntax independiente | no altera ordinary type | PR |
| TA-AST-056 | CA-AST-006 | addTypeSyntax null | ignorado | PR |
| TA-AST-057 | CA-AST-006 | addTypeSyntax punteros/duplicados | orden y duplicados exactos | PR |
| TA-AST-058 | CA-AST-006 | clearTypeSyntaxes | lista vacía; slots single intactos | PR |
| TA-AST-059 | CA-AST-006 | referencias const a vectores | reflejan append posterior según lifetime STL | PR |
| TA-AST-060 | CA-AST-006 | many modifiers | hasModifier encuentra extremos | NIGHTLY |
| TA-AST-061 | CA-AST-007 | generic vectors default | ambos vacíos | PR |
| TA-AST-062 | CA-AST-007 | addGenericParameter string | legacy name y record Type alineados | PR |
| TA-AST-063 | CA-AST-007 | add record Constant con type | ambos vectores alineados | PR |
| TA-AST-064 | CA-AST-007 | TypePack/ConstantPack | kinds conservados | PR |
| TA-AST-065 | CA-AST-007 | nombres duplicados/vacíos | contenedor conserva; no valida | PR |
| TA-AST-066 | CA-AST-007 | rangos distintos | record completo preservado | PR |
| TA-AST-067 | CA-AST-007 | clearGenericParameters | ambos vectores vacíos | PR |
| TA-AST-068 | CA-AST-007 | add después de clear | alineación reinicia correctamente | PR |
| TA-AST-069 | CA-AST-007 | input string/record temporal | ownership de strings preservado | PR |
| TA-AST-070 | CA-AST-007 | N parámetros mixtos | índice legacy coincide con record name | NIGHTLY |
| TA-AST-071 | CA-AST-008 | lifetime vectors default | vacíos y lifetimeName vacío | PR |
| TA-AST-072 | CA-AST-008 | add lifetime parameter | move y orden exactos | PR |
| TA-AST-073 | CA-AST-008 | add outlives | pares/rangos y orden | PR |
| TA-AST-074 | CA-AST-008 | set lifetimeName | string propietario independiente | PR |
| TA-AST-075 | CA-AST-008 | add constraint completo | todos los campos preservados | PR |
| TA-AST-076 | CA-AST-008 | constraints múltiples/duplicados | orden y duplicados | PR |
| TA-AST-077 | CA-AST-008 | clear constraints | sólo constraints se vacían | PR |
| TA-AST-078 | CA-AST-008 | no clear de lifetime vía API | estado sigue accesible | PR |
| TA-AST-079 | CA-AST-008 | pointers en constraint | no ownership ni clonación | PR |
| TA-AST-080 | CA-AST-009 | first/second/third default | null independientes | PR |
| TA-AST-081 | CA-AST-009 | set slots a tres nodos | identidad exacta | PR |
| TA-AST-082 | CA-AST-009 | reset un slot a null | otros intactos | PR |
| TA-AST-083 | CA-AST-009 | addChild null | null conservado | PR |
| TA-AST-084 | CA-AST-009 | addChild duplicado | duplicado conservado | PR |
| TA-AST-085 | CA-AST-009 | append A B y prepend C | orden C A B | PR |
| TA-AST-086 | CA-AST-009 | prepend repetido | cada nuevo elemento queda primero | PR |
| TA-AST-087 | CA-AST-009 | child igual a first | ambos enlaces permitidos | PR |
| TA-AST-088 | CA-AST-009 | self-cycle | contenedor lo acepta sin validar | PR |
| TA-AST-089 | CA-AST-009 | prepend sobre vector grande | orden exacto; complejidad caracterizada | NIGHTLY |
| TA-AST-090 | CA-AST-010 | defaults de bool/policies/números | false/Inherit/Default/0/0.0 | PR |
| TA-AST-091 | CA-AST-010 | flag true/false | round-trip | PR |
| TA-AST-092 | CA-AST-010 | reference true/false | independiente de flag | PR |
| TA-AST-093 | CA-AST-010 | variadic true/false | independiente | PR |
| TA-AST-094 | CA-AST-010 | unsafe true/false | independiente | PR |
| TA-AST-095 | CA-AST-010 | cuatro FP modes | round-trip | PR |
| TA-AST-096 | CA-AST-010 | tres unwind policies | round-trip | PR |
| TA-AST-097 | CA-AST-010 | uint64 0/max | exacto | PR |
| TA-AST-098 | CA-AST-010 | double 0,-0,finite,inf,nan | bit/semántica accesible según assignment | PR |
| TA-AST-099 | CA-AST-010 | todos canales activados | no se pisan | PR |
| TA-AST-100 | CA-AST-010 | valores incongruentes con kind | contenedor los conserva | PR |
| TA-AST-101 | CA-AST-010 | secuencia de resets | último valor por canal | PR |

## TypeSyntax

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TA-AST-102 | CA-AST-011 | constructor | range/token/name exactos y demás defaults | PR |
| TA-AST-103 | CA-AST-011 | setRange | reemplazo exacto | PR |
| TA-AST-104 | CA-AST-011 | setBaseTokenKind | token reemplazado | PR |
| TA-AST-105 | CA-AST-011 | setBaseName temporal | string propietario | PR |
| TA-AST-106 | CA-AST-011 | prepend modifiers vacío | sin cambio | PR |
| TA-AST-107 | CA-AST-011 | prepend A B sobre C D | orden A B C D | PR |
| TA-AST-108 | CA-AST-011 | prepend con duplicados | duplicados conservados | PR |
| TA-AST-109 | CA-AST-011 | alias pointer sobre use-site pointer | dos modifiers en orden | PR |
| TA-AST-110 | CA-AST-011 | referencias previas a vector y prepend | invalidación se caracteriza; no se reutiliza | PR |
| TA-AST-111 | CA-AST-011 | kind/name sin coherencia | contenedor no valida | PR |
| TA-AST-112 | CA-AST-012 | ownership default/name | None y `none` | PR |
| TA-AST-113 | CA-AST-012 | Owned/Borrowed | setter y nombres exactos | PR |
| TA-AST-114 | CA-AST-012 | ownership enum desconocido | nombre fallback `none` | PR |
| TA-AST-115 | CA-AST-012 | lifetime vacío/no vacío | round-trip | PR |
| TA-AST-116 | CA-AST-012 | dynamic trait toggle/default arg | true y reset false | PR |
| TA-AST-117 | CA-AST-012 | opaque result toggle | independiente de dynamic | PR |
| TA-AST-118 | CA-AST-012 | opaque trait name temporal | ownership exacto | PR |
| TA-AST-119 | CA-AST-012 | explicit trait implementation | toggle independiente | PR |
| TA-AST-120 | CA-AST-012 | pointee const | toggle independiente | PR |
| TA-AST-121 | CA-AST-012 | todos marcadores combinados | no alteran base fields/modifiers | PR |
| TA-AST-122 | CA-AST-012 | setters bool sin argumento | default true | PR |
| TA-AST-123 | CA-AST-012 | setters false explícito | limpian sólo su bit | PR |
| TA-AST-124 | CA-AST-013 | typeArguments default | vacío | PR |
| TA-AST-125 | CA-AST-013 | add null | null conservado | PR |
| TA-AST-126 | CA-AST-013 | add A B A | orden/duplicado | PR |
| TA-AST-127 | CA-AST-013 | clear arguments | vector vacío | PR |
| TA-AST-128 | CA-AST-013 | tuple default/set | bool round-trip | PR |
| TA-AST-129 | CA-AST-013 | addTupleElement | fuerza tuple y alinea type/name | PR |
| TA-AST-130 | CA-AST-013 | tuple name vacío/no vacío | strings exactos | PR |
| TA-AST-131 | CA-AST-013 | addTypeArgument después de tuple elements | desalineación vigente observable | PR |
| TA-AST-132 | CA-AST-013 | clear typeArguments de tuple | names permanecen | PR |
| TA-AST-133 | CA-AST-013 | elementos temporales | nombres propietarios; types no propietarios | PR |
| TA-AST-134 | CA-AST-014 | function default | false, Pointer, NativeSharp y flags false | PR |
| TA-AST-135 | CA-AST-014 | setFunction true/false | sólo bit function | PR |
| TA-AST-136 | CA-AST-014 | carrier Pointer/Reference | round-trip | PR |
| TA-AST-137 | CA-AST-014 | seis calling conventions | round-trip | PR |
| TA-AST-138 | CA-AST-014 | return/error null y puntero | independientes | PR |
| TA-AST-139 | CA-AST-014 | variadic toggle/default arg | round-trip | PR |
| TA-AST-140 | CA-AST-014 | unsafe toggle/default arg | round-trip | PR |
| TA-AST-141 | CA-AST-014 | noheap toggle/default arg | round-trip | PR |
| TA-AST-142 | CA-AST-014 | async toggle/default arg | round-trip | PR |
| TA-AST-143 | CA-AST-014 | nullable toggle/default arg | round-trip | PR |
| TA-AST-144 | CA-AST-014 | lifetime name temporal | propietario y exacto | PR |
| TA-AST-145 | CA-AST-014 | parameters in typeArguments | orden exacto | PR |
| TA-AST-146 | CA-AST-014 | all fields combined | no cross-field mutation | PR |
| TA-AST-147 | CA-AST-014 | setFunction false con metadata poblada | metadata permanece | PR |
| TA-AST-148 | CA-AST-014 | missing return | contenedor permite null | PR |
| TA-AST-149 | CA-AST-014 | function plus tuple true | contenedor permite combinación incongruente | PR |
| TA-AST-150 | CA-AST-015 | constExpression default | null | PR |
| TA-AST-151 | CA-AST-015 | set/reset constExpression | identidad/null exactos | PR |
| TA-AST-152 | CA-AST-015 | pack default/toggle | false/true/false | PR |
| TA-AST-153 | CA-AST-015 | const + pack simultáneos | independientes | PR |
| TA-AST-154 | CA-AST-015 | expression de otro arena | puntero conservado sin ownership | PR |
| TA-AST-155 | CA-AST-015 | uso fuera de generic argument | contenedor no valida | PR |
| TA-AST-156 | CA-AST-016 | TypeModifier default | Pointer/null/rank0/legacy/provenance/alignment1/as0 | PR |
| TA-AST-157 | CA-AST-016 | cinco modifier kinds | valores distintos y round-trip de record | PR |
| TA-AST-158 | CA-AST-016 | arrayCount null/puntero | identidad exacta | PR |
| TA-AST-159 | CA-AST-016 | rank 0/1/N | exacto | PR |
| TA-AST-160 | CA-AST-016 | tres nullabilities | exactas | PR |
| TA-AST-161 | CA-AST-016 | provenance true/false | exacto | PR |
| TA-AST-162 | CA-AST-016 | alignment 1/power/nonpower/0 | record conserva sin validar | PR |
| TA-AST-163 | CA-AST-016 | address space 0/max | exacto | PR |
| TA-AST-164 | CA-AST-016 | modifier range | exacto | PR |
| TA-AST-165 | CA-AST-016 | add modifiers mixtos | orden exacto | PR |
| TA-AST-166 | CA-AST-016 | duplicate modifiers | conservados | PR |
| TA-AST-167 | CA-AST-016 | pointer-only fields en array kind | conservados aunque incongruentes | PR |
| TA-AST-168 | CA-AST-016 | arrayCount de otro arena | no ownership | PR |
| TA-AST-169 | CA-AST-016 | many modifiers | vector completo | NIGHTLY |
| TA-AST-170 | CA-AST-017 | origin defaults | ambos nullopt | PR |
| TA-AST-171 | CA-AST-017 | project ordinal 0/max | optional poblado exacto | PR |
| TA-AST-172 | CA-AST-017 | namespace vacío/no vacío | optional poblado con string exacto | PR |
| TA-AST-173 | CA-AST-017 | namespace temporal | ownership después de input | PR |
| TA-AST-174 | CA-AST-017 | ambos origins | independientes | PR |
| TA-AST-175 | CA-AST-017 | setters repetidos | último valor | PR |
| TA-AST-176 | CA-AST-017 | origin set | baseName permanece idéntico | PR |
| TA-AST-177 | CA-AST-017 | ausencia de clear explícito | optional no vuelve a null por API | PR |

## Dumper

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TA-AST-178 | CA-AST-018 | nodo mínimo | `<Kind>\n` exacto | PR |
| TA-AST-179 | CA-AST-018 | text/aux/name | orden y comillas exactas sin escape | PR |
| TA-AST-180 | CA-AST-018 | generic names 0/1/N | ` generic=<...>` exacto | PR |
| TA-AST-181 | CA-AST-018 | lifetime parameters | ` lifetime=<...>` | PR |
| TA-AST-182 | CA-AST-018 | outlives múltiples | una secuencia por record | PR |
| TA-AST-183 | CA-AST-018 | lifetimeName | ` lifetime-name=` | PR |
| TA-AST-184 | CA-AST-018 | constraint equality | ` where T==<type>` y continue | PR |
| TA-AST-185 | CA-AST-018 | cada capability | spelling/orden fijo exacto | PR |
| TA-AST-186 | CA-AST-018 | multiple capabilities/types | commas exactas | PR |
| TA-AST-187 | CA-AST-018 | TokenKind válido/Invalid | op presente/ausente | PR |
| TA-AST-188 | CA-AST-018 | type y throws | orden `type` luego `throws` | PR |
| TA-AST-189 | CA-AST-018 | flag/ref/variadic | markers sólo al true | PR |
| TA-AST-190 | CA-AST-018 | cuatro FP modes | inherit omitido; otros spellings | PR |
| TA-AST-191 | CA-AST-018 | unsafe/unwind/numeric/modifiers distintos | dump idéntico caracteriza omisión | PR |
| TA-AST-192 | CA-AST-018 | strings con quote/newline | bytes sin escape adicional | PR |
| TA-AST-193 | CA-AST-018 | ostream failure state | comportamiento de escritura caracterizado | PR |
| TA-AST-194 | CA-AST-019 | base nominal simple | baseName exacto | PR |
| TA-AST-195 | CA-AST-019 | ownership none/owned/borrowed | prefijo exacto; none omitido | PR |
| TA-AST-196 | CA-AST-019 | borrowed lifetime | `borrowed[a] ` | PR |
| TA-AST-197 | CA-AST-019 | dyn nominal | `dyn ` antes de base | PR |
| TA-AST-198 | CA-AST-019 | generic nominal 0/1/N | angle list/comma exactos | PR |
| TA-AST-199 | CA-AST-019 | tuple vacío/unnamed/named parcial | parentheses/names exactos | PR |
| TA-AST-200 | CA-AST-019 | function pointer/reference | `fn*`/`fn&` | PR |
| TA-AST-201 | CA-AST-019 | function nullable/lifetime | marker/annotation exactos | PR |
| TA-AST-202 | CA-AST-019 | seis calling conventions | spelling exacto incluido native | PR |
| TA-AST-203 | CA-AST-019 | function params/variadic | comma/ellipsis exactos | PR |
| TA-AST-204 | CA-AST-019 | return presente/ausente | type o `<missing>` | PR |
| TA-AST-205 | CA-AST-019 | unsafe/noheap/async/throws | suffixes en orden | PR |
| TA-AST-206 | CA-AST-019 | pointee const | ` const` antes de modifiers | PR |
| TA-AST-207 | CA-AST-019 | pointer legacy/nullable/nonnull | `*`, `*?`, `*!` | PR |
| TA-AST-208 | CA-AST-019 | pointer alignment/address space/provenance | suffixes y omisión de defaults | PR |
| TA-AST-209 | CA-AST-019 | dynamic array | `[]` | PR |
| TA-AST-210 | CA-AST-019 | range rank 0/1/N | commas según max(rank-1,0) | PR |
| TA-AST-211 | CA-AST-019 | flexible array | `[*]` | PR |
| TA-AST-212 | CA-AST-019 | fixed array con cualquier count | `[expr]`, count no volcado | PR |
| TA-AST-213 | CA-AST-019 | nested types | concatenación recursiva exacta | PR |
| TA-AST-214 | CA-AST-019 | opaque/trait/const/pack/origin/range distintos | dump idéntico caracteriza omisiones | PR |
| TA-AST-215 | CA-AST-019 | null type argument | precondición violada y fallo caracterizado | NIGHTLY |
| TA-AST-216 | CA-AST-019 | deeply nested type | salida o límite de stack caracterizado | NIGHTLY |
| TA-AST-217 | CA-AST-020 | dump null root | salida vacía | PR |
| TA-AST-218 | CA-AST-020 | root con first/second/third | preorden y edge labels exactos | PR |
| TA-AST-219 | CA-AST-020 | children A B | después de slots y en orden | PR |
| TA-AST-220 | CA-AST-020 | null slots/child | null no produce línea | PR |
| TA-AST-221 | CA-AST-020 | profundidad 0/1/N | dos espacios por nivel | PR |
| TA-AST-222 | CA-AST-020 | DAG con nodo repetido | se imprime en cada enlace | PR |
| TA-AST-223 | CA-AST-020 | self-cycle | no terminación vigente caracterizada con watchdog | NIGHTLY |
| TA-AST-224 | CA-AST-020 | cycle de dos nodos | no terminación vigente caracterizada | NIGHTLY |
| TA-AST-225 | CA-AST-020 | árbol profundo | límite de stack caracterizado | NIGHTLY |
| TA-AST-226 | CA-AST-020 | dos árboles que difieren sólo en unsafe | dumps iguales | PR |
| TA-AST-227 | CA-AST-020 | dos árboles que difieren sólo en integerValue | dumps iguales | PR |
| TA-AST-228 | CA-AST-020 | mismo árbol dos veces | bytes deterministas | PR |
| TA-AST-229 | CA-AST-020 | orden de children permutado | orden de líneas cambia en consecuencia | PR |
| TA-AST-230 | CA-AST-020 | topology completa | bytes dorados incluyendo newline final | PR |

## Criterios de cierre futuro

1. Cada caso debe implementarse o quedar bloqueado con evidencia; el diseño no equivale a ejecución.
2. Los tests de dumper comparan bytes completos, no substrings.
3. Las omisiones vigentes se prueban como colisiones hasta que un cambio de contrato las modifique.
4. Los casos de excepción/ciclo/profundidad requieren aislamiento y watchdog en Fase 2.
5. Coverage instrumental pertenece a Fase 3.
