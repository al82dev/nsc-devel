# Host.HIR — Modelo intermedio tipado, lowering y verificación

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo las inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 12 archivos, 2,061 líneas y 74,969 bytes.
- **Inventario:** 88 símbolos físicos, 29 contratos y 232 matrices de prueba diseñadas.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura o modificación del producto.

## 2. Responsabilidad y frontera

Host.HIR es la primera representación intermedia explícita del compilador clásico. Recibe un AST ya analizado
por Sema y produce un árbol tipado que hace explícitos lugares, cargas, stores, conversiones, operaciones,
llamadas, direcciones y operaciones de memoria. Después valida que la representación conserve las relaciones
de tipos, símbolos y topología exigidas antes de que la consuman fases posteriores.

La frontera está formada por cinco responsabilidades:

1. `HirArena` posee todos los nodos y fija su lifetime.
2. `HirNode` almacena clase, rango, metadatos semánticos y aristas.
3. `HirBuilder` baja AST más `SemanticModel` a HIR.
4. `HirVerifier` rechaza grafos estructural o semánticamente incoherentes.
5. `HirDumper` publica una vista textual determinista para diagnóstico y comparación.

No resuelve nombres, tipos ni operadores: consume esas decisiones de Sema. Tampoco ejecuta optimizaciones ni
genera MIR o LLVM. Los punteros a `Type` y `Symbol` son no propietarios y deben sobrevivir al grafo HIR.

## 3. Estructuras de datos, ownership y lifetime

### 3.1 `HirArena`

`make(kind, range)` reserva un `HirNode`, conserva su puntero en `nodes_` y lo devuelve. El destructor recorre
ese vector en orden y libera cada nodo. La arena no se copia ni se asigna. El costo de creación es amortizado
`O(1)` por inserción; destrucción y `size()` son `O(n)` y `O(1)` respectivamente.

El contrato no impide aristas entre arenas. Esa disciplina pertenece al consumidor: una arista sólo es válida
si el nodo destino vive al menos tanto como el grafo que la consulta.

### 3.2 `HirNode`

Cada nodo conserva:

- `kind_` y `range_`, inmutables después del constructor;
- tipo de resultado, tipo de operación, símbolo y token de operación;
- constante y tres canales de conversión: operand, read y result;
- flags de bounds-check, forma de arreglo y postfix;
- offset opcional, donde `hasOffset_` distingue offset cero de ausencia;
- aristas nominadas `first`, `second`, `third` y una lista ordenada `children`.

Los setters no validan combinaciones. Permiten construir incrementalmente; `HirVerifier` es quien aplica las
invariantes por clase de nodo. Los canales de conversión son independientes porque un compound assignment puede
convertir la lectura y el resultado mediante reglas distintas.

### 3.3 Clases de nodo

`HirNodeKind` contiene 34 clases: ocho declaraciones globales o parámetros, nueve statements y diecisiete
expresiones o direcciones. `hirNodeKindName` cubre cada enumerador y devuelve `Unknown` para valores fuera del
enum. Es un contrato textual estable consumido por dump y diagnósticos del verifier.

## 4. Algoritmo de construcción

### 4.1 Entrada y globals

`HirBuilder::build` exige una raíz `CompilationUnit`. Crea la raíz HIR y conserva el orden de declaraciones.
Un bloque `extern C` no produce nodo propio: sus hijos se bajan como funciones consecutivas. `lowerGlobal`
acepta struct, enum, función y variable global; cualquier otra clase fija el primer error y aborta.

Structs y enums enlazan el símbolo del tipo y agregan fields o members en orden. Los fields copian offset sólo
si Sema lo publicó. Los enum members copian el valor constante. Funciones copian el tipo función, crean un
parámetro por hijo y bajan el bloque cuando existe cuerpo. Las declaraciones externas pueden conservar un
parámetro sin símbolo de runtime y usar el tipo posicional de la firma.

### 4.2 Statements

`lowerBlock` conserva el orden y aborta al primer hijo fallido. `lowerStatement` representa:

- bloques y variables locales;
- `if` con condición en `first`, cuerpo en `second` y block o `if` en `third`;
- `while` con condición y cuerpo;
- `for` con initializer, condición, incremento y exactamente un block en `children`;
- `break`, `continue`, `return` y expression statement;
- `static_assert` como `Block` vacío porque su efecto ya fue consumido por Sema.

Un return con valor usa la conversión registrada para el statement. Sin valor, su tipo HIR es `Void`.

### 4.3 Expresiones, lugares y cargas

`lowerExpression(node, loadLValue)` consulta `ExpressionInfo`. Rechaza ausencia, tipo `Error` y categorías no
utilizables; en particular emite `NS3020` si llega una expresión moved-from. Una categoría place se baja primero
como dirección. Si `loadLValue` es verdadero, envuelve esa dirección en `Load`; si es falso, devuelve la dirección.

Literales, `sizeof`, `alignof` y `offsetof` requieren una constante semántica. Un enum member también se vuelve
`Constant`, conservando su símbolo. Los operadores copian token, tipo de resultado y tipo de operación resuelto.
`++` y `--` conservan si eran postfix. La asignación simple produce `Store`; la compuesta produce
`CompoundAssignment` con conversiones separadas de lectura y resultado.

Calls guardan el símbolo función y agregan argumentos convertidos en orden. Casts delegan al registro de
conversiones de Sema. El builder sólo aplica conversiones cuyo operand pointer y, cuando existe, índice coinciden;
las conversiones de rol `Result` se excluyen de esa cadena.

### 4.4 Direcciones

`lowerAddress` materializa:

- `SymbolAddress` para almacenamiento identificado;
- `DereferenceAddress` para `*pointer`;
- una cadena de `IndexAddress` para uno o varios índices;
- `FieldAddress` para member y pointer-member.

Arrays y string literals marcan bounds-check; pointers no. Cada índice avanza el tipo de dimensión. Fields
conservan símbolo y offset. Una expresión clasificada como place pero sin lowering de dirección es un error del
contrato AST/Sema-HIR, no se degrada silenciosamente a valor.

### 4.5 Memoria

`new` produce `Allocation`; el tipo de operación es el elemento apuntado, `arrayForm` distingue scalar y array,
y `first` contiene el count sólo cuando existe. `delete` produce `Deallocation` con resultado void según la
información semántica. `stackalloc` exige count bajable y conserva el elemento apuntado.

### 4.6 Error latch

`fail` sólo escribe `error_` cuando está vacío. Todos los caminos derivados retornan `nullptr` o `false` sin
reemplazar la causa inicial. El builder no limpia `error_` al comenzar `build`; su uso documentado es una
instancia por construcción.

## 5. Dump textual

`HirDumper` recorre el grafo en preorden, imprime dos espacios por profundidad y emite las aristas en este orden:
`first`, `second`, `third`, después `children`. Publica kind, símbolo, qualifiers `const`/`extern-C`, tipos,
operación, constante, conversiones, flags y offset.

Strings escapan slash, comillas, LF, CR, tab y NUL. Bytes imprimibles ASCII pasan sin cambio y los demás usan
`\xNN` mayúscula. La rutina restaura flags y fill del stream después del hexadecimal. Floating usa precisión 17.
Un nodo nulo no produce salida.

El recorrido es recursivo y no detecta ciclos. **INFERENCIA:** el consumidor debe entregar un grafo finito y
acíclico; profundidad extrema puede agotar stack antes de producir el dump completo.

## 6. Verificación

### 6.1 Protocolo

`verify` limpia el error, rechaza raíz nula o distinta de `CompilationUnit` y llama `verifyNode` con retorno nulo
y loop depth cero. `fail` antepone el nombre del kind y conserva sólo el primer mensaje.

### 6.2 Declaraciones

La unidad sólo admite globals. Structs contienen fields; enums contienen members. Cada declaración debe apuntar
al símbolo de kind exacto y compartir el mismo objeto `Type`. Fields requieren offset incluso cuando es cero.
Functions validan firma, número y tipo posicional de parámetros. Un parámetro de función definida requiere
símbolo runtime; uno externo puede carecer de él. El cuerpo, si existe, debe ser `Block` válido.

### 6.3 Statements y contexto

Blocks sólo contienen statements. Condiciones de if, while y for deben ser bool. For admite initializer local o
expresión, condición e incremento opcionales y exactamente un body. El loop depth aumenta sólo al verificar los
bodies de while y for. Break y continue exigen profundidad positiva. Return exige contexto de función; void no
puede retornar valor y un retorno no-void debe coincidir por identidad exacta de `Type`.

### 6.4 Expresiones y almacenamiento

Constants requieren tipo, payload y cero aristas. `SymbolAddress` sólo admite variable, global o parámetro y
debe ser pointer-to-symbol-type. Loads y stores verifican la relación pointer-to-T. Conversion, unary y binary
exigen sus metadatos, pero la validez específica de cada `ConversionKind` y token se confía a Sema.

Calls requieren símbolo función, tipo de retorno exacto, cantidad de argumentos y tipos posicionales idénticos.
Compound assignment e increment validan destino pointer-to-value; increment restringe el token a `++` o `--`.

### 6.5 Direcciones y memoria

Index distingue array, pointer y string literal. Verifica índice entero y relación exacta base-element-result.
FieldAddress exige símbolo field, offset y pointer-to-field; ExtractField exige base struct, field, type y offset.
DereferenceAddress conserva el tipo pointer evaluado.

Allocation exige resultado pointer, operation type igual al element type y count entero sólo para array.
Deallocation exige resultado void y operand pointer o null literal. StackAllocation exige pointer, pointee y
count entero.

## 7. Invariantes observables

1. Orden de globals, parameters, statements, arguments, indices y children no cambia.
2. Tipos y símbolos se comparan por identidad de puntero, no sólo por igualdad nominal.
3. Places y values son distintos; una carga se materializa explícitamente.
4. Las conversiones se materializan en el orden publicado por Sema.
5. El primer error es estable.
6. Offset cero y offset ausente son estados diferentes.
7. Bounds-check sólo se marca para array y string index.
8. Dump y nombres de kind son superficies textuales deterministas.

## 8. Errores y limitaciones vigentes

- Builder y verifier usan recorridos recursivos de árbol; no existe worklist ni detección de ciclos.
- `HirDumper` también es recursivo y asume aciclicidad.
- `HirBuilder::build` no reinicia `error_`; reutilizar una instancia fallida conserva el fallo anterior.
- El verifier comprueba presencia y relaciones generales de operadores/conversiones, pero no revalida toda la
  tabla semántica que produjo Sema. Esa separación es deliberada por capas.
- `static_assert` desaparece como conducta y queda representado por un block vacío; su resultado debe haberse
  resuelto antes de HIR.
- Los setters permiten estados intermedios inválidos; sólo un verifier posterior ofrece cierre estructural.

Estos puntos se conservan como conducta o riesgo observado. Esta fase no autoriza modificarlos.

## 9. Dependencias y consumidores

| Dirección | Componentes | Contrato |
|---|---|---|
| entrada | Basic, Lexer, AST, Sema | rangos, tokens, árbol, tipos, símbolos, constantes y conversiones |
| interna | STL | vector, string, stream, formatting y ownership manual de nodos |
| salida | MIR/CodeGen y herramientas de diagnóstico | árbol HIR validado y dump textual |

La implementación futura de pruebas debe crear objetos de Sema con lifetime explícito. Un fixture que destruya
`TypeContext`, símbolos o AST antes del HIR estaría probando un uso inválido.

## 10. Reconstrucción y estrategia de prueba

Un reemplazo compatible debe reproducir:

- las 34 clases de nodo y sus spellings;
- los slots y flags de `HirNode` con sus presence bits;
- ownership de arena y aristas no propietarias;
- lowering y orden exactos descritos en las secciones 4.1–4.5;
- first-error latch y textos contractuales;
- escape y orden byte-exactos del dumper;
- todas las relaciones verificadas en la sección 6.

Las 29 familias `CF-HIR-*` dividen esa superficie por contrato observable. Cada una tiene ocho matrices en
`HOST-HIR-TEST-PLAN.md`: mínimo, representativo, dependencia ausente, relación inválida, límites, variantes,
determinismo y escala. Las matrices no son todavía casos ejecutables.

### Gate de las veinte preguntas

1. **Por qué existe:** separa semántica fuente de representaciones posteriores.
2. **Problema:** hace explícitas operaciones tipadas y relaciones de almacenamiento.
3. **Entradas:** AST, `SemanticModel`, `TypeContext` y stream.
4. **Salidas:** grafo HIR, verdict, error o dump.
5. **Contratos:** 29 familias trazadas.
6. **Estructuras:** arena, nodo con slots y vector ordenado.
7. **Algoritmos:** lowering recursivo, verificación por kind y dump preorden.
8. **Invariantes:** sección 7.
9. **Errores:** first-error latch y mensajes por frontera.
10. **Estados:** nodo incremental; builder success/error; verifier verdict/error.
11. **Ownership:** arena propietaria, referencias semánticas y aristas no propietarias.
12. **Interacción:** tabla de sección 9.
13. **Formatos:** texto HIR determinista.
14. **Complejidad:** lineal en nodos y aristas; profundidad de stack proporcional al árbol.
15. **Alternativas:** worklist o nodos tipados no están implementados en esta revisión.
16. **Elección vigente:** nodo uniforme más verifier posterior.
17. **Evolución:** no reconstruida en esta fase de revisión congelada.
18. **Defectos históricos:** no se infieren; limitaciones vigentes están en sección 8.
19. **Pruebas:** plan contractual nuevo; la batería histórica sólo es respaldo congelado.
20. **Reimplementación:** secciones 3–10 y los inventarios contienen los datos necesarios.

**Dictamen documental:** `RECONSTRUIBLE`. No equivale a pruebas implementadas ni a cobertura medida.
