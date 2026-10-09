# Host.Sema — inventario semántico y contrato reconstructivo

## Estado y alcance

- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Estado:** `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada.
- **Universo:** 30 archivos, 5,143 líneas y 175,338 bytes.
- **Naturaleza:** modelo de tipos, símbolos, scopes, resultados semánticos, análisis del frontend histórico y dumpers.
- **Fuera de alcance:** corregir decisiones semánticas, modificar diagnósticos, implementar pruebas o ejecutar código.

## 1. Frontera del componente

`Host.Sema` recibe el AST histórico, el layout del target y un motor de diagnósticos. Produce símbolos, tipos
internados, anotaciones por nodo, conversiones y operadores resueltos. La ruta auditada no es la semántica de
proyecto completa: `SemanticAnalyzer` rechaza explícitamente algunas construcciones que otras capas del
compilador procesan. Esa bifurcación se conserva como hecho observable y no se normaliza en esta fase.

## 2. Contratos propietarios

| Contrato | Archivos principales | Responsabilidad |
|---|---|---|
| `CS-SEMA-001` | `ConstantValue.*` | valores constantes etiquetados |
| `CS-SEMA-002` | `ConversionKind.*`, `ConversionInfo.h` | taxonomía y registros de conversión |
| `CS-SEMA-003` | `OwnershipKind.*` | taxonomía de ownership |
| `CS-SEMA-004` | `EffectContext.h` | conjuntos y herencia de efectos |
| `CS-SEMA-005` | `ExpressionInfo.h`, `ResolvedOperatorInfo.h` | categoría, flags y operador de expresiones |
| `CS-SEMA-006` | `Scope.*` | declaración y lookup léxico |
| `CS-SEMA-007` | `SymbolKind.*`, `Symbol.*` | identidad y estado mutable de símbolos |
| `CS-SEMA-008` | `TypeKind.*`, `Type.*` | forma, payload y nombre visible de tipos |
| `CS-SEMA-009` | `TypeContext.*` | propiedad e interning de tipos |
| `CS-SEMA-010` | `SemanticModel.*` | mapas de anotaciones semánticas |
| `CS-SEMA-011` | `SemanticAnalyzer.*` | ciclo de vida y despacho global |
| `CS-SEMA-012` | `SemanticAnalyzer.*` | declaración, shadowing y lookup |
| `CS-SEMA-013` | `SemanticAnalyzer.*` | structs, fields y layout |
| `CS-SEMA-014` | `SemanticAnalyzer.*` | enums y evaluación de miembros |
| `CS-SEMA-015` | `SemanticAnalyzer.*` | funciones, entry point y `extern C` |
| `CS-SEMA-016` | `SemanticAnalyzer.*` | variables globales/locales y constantes |
| `CS-SEMA-017` | `SemanticAnalyzer.*` | statements y control de flujo |
| `CS-SEMA-018` | `SemanticAnalyzer.*` | identificadores y literales |
| `CS-SEMA-019` | `SemanticAnalyzer.*` | operadores unarios y binarios |
| `CS-SEMA-020` | `SemanticAnalyzer.*` | asignación, compound assignment y calls |
| `CS-SEMA-021` | `SemanticAnalyzer.*` | indexación y acceso a miembros |
| `CS-SEMA-022` | `SemanticAnalyzer.*` | casts y registro de conversiones |
| `CS-SEMA-023` | `SemanticAnalyzer.*` | `sizeof`, `alignof`, `offsetof` y layout |
| `CS-SEMA-024` | `SemanticAnalyzer.*` | `new`, `delete` y `stackalloc` |
| `CS-SEMA-025` | `SemanticAnalyzer.*` | resolución y validez de tipos, incluido C ABI |
| `CS-SEMA-026` | `SemanticAnalyzer.*` | clasificación/promoción/conversión numérica |
| `CS-SEMA-027` | `SemanticAnalyzer.*` | evaluación constante y traps |
| `CS-SEMA-028` | `SemanticAnalyzer.*` | análisis de fallthrough y breaks |
| `CS-SEMA-029` | `SemanticDumper.*` | serialización del modelo semántico |
| `CS-SEMA-030` | `SymbolDumper.*` | serialización del árbol de símbolos |

## 3. Valores constantes — `CS-SEMA-001`

- `ConstantKind` distingue `None`, enteros signed/unsigned, floating, boolean, null y string.
- Cada factory cambia el discriminante y únicamente el canal de payload correspondiente.
- El valor por defecto conserva todos los payloads en cero/falso/vacío y discriminante `None`.
- Los getters no verifican el discriminante: leer un canal inactivo devuelve su almacenamiento vigente.
- String mueve su argumento al valor; el getter expone referencia constante con lifetime del objeto.

## 4. Conversiones — `CS-SEMA-002`

- La tabla contiene trece `ConversionKind` y spellings exactos; un enum externo retorna `unknown`.
- `ConversionRole` separa conversión contextual, de operando y de resultado.
- `ConversionInfo` conserva owner externo, operando, tipos origen/destino e índice; `-1` significa sin índice.
- El registro de conversiones del analyzer omite identidades y tipos de error, salvo las decisiones explícitas
  codificadas por cada helper de registro.

## 5. Ownership — `CS-SEMA-003`

- La tabla pública contiene `None`, `Owned` y `Borrowed` con spellings `none`, `owned` y `borrowed`.
- Un enum externo cae en `none`; esto es conducta vigente, no validación segura del caller.
- El ownership aquí califica un `Type`; no implementa por sí mismo movimientos o préstamos.

## 6. Efectos — `CS-SEMA-004`

- `EffectSet` es un bitset inmutable desde la API: `with` retorna copia, `contains` consulta y `bits` expone.
- Los cinco bits son `Unsafe`, `Throws`, `Async`, `Comptime` y `NoHeap`.
- Sólo `Throws`, `Async` y `NoHeap` participan en el tipo de función.
- `EffectContext::allows` busca en el contexto local y después en padres; `require` retorna el efecto faltante.
- La búsqueda de padres es recursiva y no tiene límite/ciclo guardado. Se registra como contradicción con la
  política posterior de evitar recursión, sin modificarla aquí.

## 7. Información de expresiones — `CS-SEMA-005`

- `ValueCategory` conserva identidades numéricas históricas; `LValue` y `RValue` son aliases de `Place` y
  `Value`.
- Place y BorrowedValue son places; sólo MovedFrom no es usable; sólo Temporary requiere materialización.
- Un enum externo para nombre cae en `value`.
- `ExpressionInfo` agrupa tipo, categoría, mutabilidad, addressability, constancia, bounds check, valor constante
  y símbolo.
- `ResolvedOperatorInfo` conserva token, tipos izquierdo/derecho/operación/resultado y marca unario.

## 8. Scopes — `CS-SEMA-006`

- `declare` inserta por nombre y devuelve falso ante duplicado local; no reemplaza el símbolo anterior.
- `lookupLocal` materializa un `std::string` para consultar el mapa y retorna null si no existe.
- `lookup` sube iterativamente por `parent` y retorna la primera coincidencia visible.
- El scope no posee los símbolos ni el padre. La tabla usa `unordered_map`; el orden no es contrato observable.

## 9. Símbolos — `CS-SEMA-007`

- Ocho `SymbolKind` tienen nombres exactos; un enum externo cae en `Symbol`.
- Un símbolo conserva kind, name, type y declaration más flags const/defined/extern C.
- Enum value inicia en cero. Field offset ausente informa `has=false` pero `fieldOffset()` devuelve cero.
- `setConstantValue` fija payload y presencia; no existe operación pública de borrado.
- Members preserva orden de inserción; `findMember` retorna la primera coincidencia lineal o null.

## 10. Tipos — `CS-SEMA-008`

- `TypeKind` contiene 22 clases y spellings exactos; enum externo cae en `<error>`.
- Un tipo calificado conserva puntero al tipo base y ownership. Casi todos los getters y setters delegan al base.
- Por esa delegación, mutar nombre, elemento, count, retorno, parámetros, completeness o declaration mediante un
  alias calificado muta el tipo base compartido.
- `unqualifiedType()` retorna el base o `this`; el kind visible también es el del base.
- `displayName` compone punteros, arrays, string literals y funciones; para las demás clases usa nombre o tabla.
- Las composiciones asumen element/return no nulos; no hay fallback contractual para tipos internamente rotos.

## 11. Contexto e interning de tipos — `CS-SEMA-009`

- El contexto crea builtins escalares, void, error y literales null; el vector queda dimensionado hasta
  `StringLiteral`, pero no toda celda representa un builtin inicializado.
- Es propietario de todos los `Type*` y los destruye en orden de inserción al finalizar.
- `builtin` valida el índice y puede devolver null para una celda no inicializada.
- `qualified(nullptr, …)` retorna Error; `None` o Error no crean wrapper. El resto se interna por base+ownership.
- Pointer, array, string literal y function se internan por igualdad estructural; la búsqueda es lineal.
- Struct y enum siempre crean una identidad nominal nueva, inicialmente incompleta, con nombre y declaration.

## 12. Modelo semántico — `CS-SEMA-010`

- Cuatro mapas independientes asocian nodo a expresión, vector de conversiones, operador y símbolo.
- `set` y `setOperator` reemplazan la entrada del nodo; `addConversion` preserva orden.
- Getters ausentes retornan null o span vacío.
- `bindSymbol` ignora silenciosamente nodo o símbolo nulos; los demás setters no tienen ese guard explícito.

## 13. Ciclo de vida y despacho — `CS-SEMA-011`

- El analyzer posee símbolos y scopes crudos en vectores; el destructor los libera en orden de inserción.
- `analyze` exige `CompilationUnit`, crea el scope global, recorre children y retorna ausencia de diagnósticos.
- El despacho global acepta struct, enum, extern C, función y global; class se rechaza en esta ruta histórica.
- Cualquier nodo global restante produce `ExpectedDeclaration`.
- `validateExecutableEntryPoint` localiza `Main` y exige símbolo function+defined; su firma se valida antes.

## 14. Declaración y lookup — `CS-SEMA-012`

- `createSymbol` registra ownership y opcionalmente agrega al orden global.
- `declareInCurrent` rechaza duplicado local y también shadowing de un struct/enum visible en padres.
- Un duplicado emite el diagnóstico principal y, si existe declaration previa, la nota correspondiente.
- El lookup usado por el analyzer comienza en `currentScope_`; scopes nested se crean por bloques y `for`.

## 15. Structs y layout — `CS-SEMA-013`

- El nombre se declara antes de fields mediante un tipo struct incompleto.
- Fields deben tener nombre único, tipo de objeto válido, no void y completo.
- Cada field crea símbolo, conserva orden y alimenta `structFieldTypes_`.
- Si todos los layouts están disponibles, se calcula alignment/padding/offset y se fijan field offsets.
- El tipo se marca completo al terminar, aun cuando haya diagnósticos; layout parcial no inventa offsets.

## 16. Enums — `CS-SEMA-014`

- La ruta single-file auditada rechaza underlying type declarado y usa `Int` signed de 32 bits.
- El símbolo enum se declara antes de miembros. Cada miembro previo queda visible para inicializadores posteriores.
- Inicializador debe ser constante integral convertible a signed; duplicados se rechazan.
- Miembros sin inicializador usan cero o anterior+1; overflow de `INT64_MAX` produce diagnóstico.
- Cada member conserva enumValue y ConstantValue signed.

## 17. Funciones, entry point y C ABI — `CS-SEMA-015`

- Return array se rechaza; parámetros no pueden ser void/array/incompletos.
- Redeclaración exige mismo tipo internado y mismo linkage; múltiples cuerpos se rechazan.
- `extern C` no admite cuerpo y valida retorno/parámetros mediante la matriz C ABI.
- Main se exige definido con firma exacta codificada; los textos vigentes contienen referencias a
  `NativeSharp 2.0`, contradicción documental frente a la política posterior neutral a edición.
- Una función non-void cuyo body puede caer al final produce `NotAllPathsReturn`.
- Al analizar body se crea scope de parámetros, se fija `currentFunction_` y se restaura el contexto previo.

## 18. Variables — `CS-SEMA-016`

- Global y local validan tipo de objeto; local puede admitir array donde el caller lo solicita.
- Inicializador debe convertir implícitamente al tipo declarado y su conversión se registra.
- Global sin initializer conserva símbolo; global `const` requiere constante.
- Inicializador global que no es constante produce `GlobalRequiresConstant`; local const no constante produce
  `ConstRequiresConstant`.
- El valor constante convertido se almacena en el símbolo cuando corresponde.

## 19. Statements y control — `CS-SEMA-017`

- Block puede reutilizar scope o crear uno nested. Variable, if, while, for, return, static_assert, inline asm y
  expression tienen rutas explícitas.
- Condiciones deben convertir a bool. `for` crea scope propio y mantiene `loopDepth_` durante body.
- Break/continue fuera de loop diagnostican; dentro no producen valor.
- Return valida presencia/ausencia y conversión contra el return type de la función actual.
- Static assert exige bool constante; false emite el mensaje suministrado o el default.
- Inline asm valida constraints/operands y siempre termina en `InlineAssemblyNotAvailable` en esta ruta.

## 20. Identificadores y literales — `CS-SEMA-018`

- Identifier resuelve símbolo, enlaza el nodo y deriva categoría/flags por kind/const.
- Type symbols no son expresiones; function sólo es válido cuando el caller permite designator.
- Integer literal selecciona tipo por suffix, signo y rango; overflow o spelling inválido diagnostica.
- Float, char, string, bool y null producen tipos y constantes específicos; string count incluye terminador.
- `ThisExpression` y placement construction no están disponibles en esta ruta histórica.

## 21. Operadores — `CS-SEMA-019`

- Address-of requiere lvalue addressable y no const conforme al flag actual; dereference exige pointer a objeto
  completo no void.
- Inc/dec exige target modificable y scalar increment type. `!`, `~`, unary `+/-` aplican matrices propias.
- `&&`/`||` convierten operands a bool y suprimen traps constantes en el lado corto-circuitado.
- Equality/relational admiten matrices numéricas y de pointers/null. Bitwise y shifts exigen enteros.
- Pointer arithmetic acepta pointer±integer y pointer-pointer sólo para resta compatible.
- Arithmetic numérica usa common type; `%` sólo enteros. Cada operación guarda `ResolvedOperatorInfo`.

## 22. Asignación y calls — `CS-SEMA-020`

- Assignment exige lhs modificable. Simple assignment registra conversión al tipo lhs.
- Compound assignment traduce token a operador base, analiza restricciones propias y registra promoción,
  conversiones de operandos y una conversión explícita de resultado cuando el tipo de operación difiere.
- Call exige function designator/type, analiza todos los argumentos aun si el count difiere y registra cada
  conversión válida a parameter.
- El resultado de función void sigue siendo rvalue void; el resto usa el return type internado.

## 23. Indexación y miembros — `CS-SEMA-021`

- Array fijo soporta children adicionales para dimensiones sucesivas; cada índice debe ser entero.
- String literal y pointer admiten un índice. Array/string marcan `requiresBoundsCheck`.
- Índice constante fuera de rango diagnostica pero la expresión resultante aún se registra.
- Member directo exige struct/enum; pointer member exige pointer a struct. Unknown member diagnostica.
- Enum member se resuelve como constante; struct field conserva mutabilidad/addressability del receiver según
  la forma codificada.

## 24. Casts y conversiones registradas — `CS-SEMA-022`

- Cast exige `canExplicitlyConvert`; registra una conversión explícita con source/destination.
- Float constante a entero hace comprobación de rango/trap antes de convertir.
- Conversiones constantes numéricas usan semántica modular del ancho destination cuando aplica.
- Los tres helpers de registro fijan role contextual/operand/result e índice según el caller.

## 25. Layout e introspección — `CS-SEMA-023`

- `sizeof`/`alignof` aceptan forma de tipo o expresión; durante el operando de expresión suprimen traps
  constantes y consultan el layout target.
- Tipo no sized produce `InvalidSizeof` para ambas operaciones según la implementación vigente.
- `offsetof` exige struct completo y field existente con offset materializado.
- `layoutOfType` cubre scalars, pointer, enum, string literal, arrays y structs; void/function/null/error fallan.
- Array y struct detectan overflow y aplican alignment/padding del `TargetDataLayout`.

## 26. Memoria explícita — `CS-SEMA-024`

- `new`/`new[]` exigen tipo allocatable; count debe ser integral, no negativo y multiplicación no overflow.
- `delete` admite pointer o null literal; no modela ownership en esta ruta.
- `stackalloc` exige tipo de objeto no void y count integral; negativo/overflow diagnostican.
- Algunos errores de count producen diagnóstico pero conservan un pointer resultante; el plan congela ese estado.

## 27. Resolución de tipos y C ABI — `CS-SEMA-025`

- Base type resuelve 14 keywords escalares y nombres globales struct/enum; unknown name diagnostica.
- Modifiers se aplican en orden; fixed array exige count constante positivo y representable por target.
- Ownership qualifier se aplica al final mediante `TypeContext::qualified`.
- Object type rechaza void/function, arrays cuando no están permitidos e incompletos.
- C ABI admite scalars/pointers/enums y structs completos no vacíos cuyos fields son compatibles; arrays se
  validan por elemento en fields. Error type pasa para evitar cascadas.

## 28. Clasificación y conversiones numéricas — `CS-SEMA-026`

- Byte/Char son unsigned; Short/Int/Long/NInt signed; UShort/UInt/ULong/NUInt unsigned.
- Widths son 8/16/32/64 con nint/nuint fijados a 64 en esta ruta Win64.
- Promoción lleva tipos menores a Int. Common integral recorre una tabla de candidates estable; numeric elige
  Double, Float o integral.
- Conversión implícita cubre identity, constant-fit, promoción, numérica, null→pointer, pointer→void* y string
  literal→char*.
- Conversión explícita cubre numérica/enum/pointer/pointer-integer/null/to-void; bool queda fuera de numeric.

## 29. Evaluación constante — `CS-SEMA-027`

- Helpers convierten signed a dominio de bits y viceversa por ancho; width 64 evita shifts inválidos.
- Unary `!`, `-`, `~`, `+` opera sobre constantes compatibles.
- Binary implementa bool, comparisons y enteros bit-exactos; add/sub/mul envuelven por máscara.
- Division/modulo por cero y shift inválido producen `ConstantEvaluationTrap`, salvo bajo suppression depth.
- Signed division mínima por -1 también es trap. Right shift signed depende del shift aritmético del host.
- Un resultado no constante conserva type/category y sólo omite el payload constante.

## 30. Fallthrough — `CS-SEMA-028`

- Return, break y continue previenen fallthrough en su contexto sintáctico.
- Block se detiene en el primer child que previene; código posterior no altera el resultado.
- If sólo previene cuando existen ambas ramas y ambas previenen.
- Loop constante true previene si no contiene break dirigido a ese loop.
- La búsqueda de fallthrough y breaks es recursiva, detiene búsqueda al entrar a loops nested y carece de guard
  de profundidad/ciclo; la contradicción con la política posterior de no recursión queda documentada.

## 31. Dump semántico — `CS-SEMA-029`

- Recorre el AST en orden de children, imprime sólo nodos con info, conversiones u operador y conserva edge/depth.
- Serializa kind, type display name, category, flags, symbol, constantes, conversiones y operador resuelto.
- Constant string se escribe según la implementación sin prometer escaping adicional.
- El recorrido es recursivo y sin visited set/depth guard; asume tipos no nulos en varias rutas.

## 32. Dump de símbolos — `CS-SEMA-030`

- Recorre globals en orden y members en orden de inserción.
- Emite kind, name, type, flags, enum value, field offset y constant value según presencia.
- El helper interno es recursivo y no detecta ciclos ni limita profundidad.

## 33. Dependencias y observabilidad

- **Entradas:** AST, `TypeContext`, `TargetDataLayout`, `DiagnosticEngine` y texto/tokenización ya producidos.
- **Salidas:** booleano de análisis, diagnósticos, globals, símbolos, tipos, modelo, conversiones, operadores y dumps.
- **Dependencias internas:** AST, Lexer tokens, SourceRange, diagnostics y target layout.
- **Oráculo:** estados exactos, identidad de punteros internados, orden, diagnósticos y texto de dump.
- **No contrato:** orden interno de `unordered_map`, performance absoluta o comportamiento ante grafos cíclicos
  fabricados fuera de las precondiciones, salvo su caracterización NIGHTLY de robustez.

## 34. Contradicciones y riesgos conservados

1. Diagnósticos y aliases todavía contienen `NativeSharp 2.0`, `3.0`, `unsafe`, `owned` y `borrowed`, frente a
   decisiones posteriores del usuario de neutralidad de edición y de no forzar esas capacidades.
2. `EffectContext`, los dumpers y el análisis de fallthrough usan recursión sin límites explícitos.
3. La ruta single-file rechaza classes, instance context, enum underlying types, placement construction e inline
   assembly aunque otras capas del compilador contienen soporte parcial o completo.
4. Setters de un `Type` calificado mutan el tipo base compartido; debe probarse como conducta vigente.
5. El interning del contexto es lineal y el analyzer continúa después de varios diagnósticos; ningún test futuro
   debe asumir fail-fast donde el código no lo implementa.

## 35. Dictamen

Los 30 archivos tienen propietario contractual, inventario de símbolos y casos diseñados. `RECONSTRUIBLE`
significa que el comportamiento de esta revisión puede reproducirse desde contratos, matrices y oráculos
documentados. No afirma que las pruebas estén implementadas o ejecutadas ni que exista cobertura instrumental.
