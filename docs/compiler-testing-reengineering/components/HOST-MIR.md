# Host.MIR — CFG tipado, lowering, volcado y verificación

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo las inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 22 archivos, 3,054 líneas y 105,640 bytes.
- **Inventario:** 152 símbolos físicos, 38 contratos y 304 matrices de prueba diseñadas.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura o modificación del producto.

## 2. Responsabilidad y frontera

Host.MIR transforma HIR tipado en un grafo de control explícito. Conserva tipos y símbolos de las capas
semánticas, pero reemplaza el árbol fuente por módulos, funciones, bloques básicos, instrucciones y valores
virtuales. Es la primera representación en la que saltos, terminadores, short circuit, cargas, stores y PHI
pueden verificarse como relaciones de CFG.

La frontera se divide en seis responsabilidades:

1. `MirArena` posee módulos, globals, funciones, bloques, instrucciones y valores.
2. `MirValue`, `MirInstruction`, `MirBlock`, `MirFunction`, `MirGlobal` y `MirModule` forman el modelo.
3. `MirBuilder` baja HIR a CFG MIR y conserva el primer error.
4. `MirOpcode` define las 47 operaciones y distingue sus terminadores.
5. `MirDumper` publica una representación textual estable del módulo.
6. `MirVerifier` valida el modelo en los modos `PreSsa` y `Ssa`.

MIR no resuelve nombres ni elige operadores fuente. Consume `Type`, `Symbol`, constantes, offsets y conversiones
ya decididos. Tampoco calcula dominancia, inserta SSA ni optimiza: esos consumidores pertenecen a componentes
posteriores.

## 3. Modelo, ownership y lifetime

### 3.1 Arena e identidades

`MirArena` reserva cada objeto con ownership exclusivo y lo conserva en un vector por clase. Los bloques y los
valores reciben secuencias de ID independientes, monotónicas y comenzadas en cero. Crear un módulo no altera
esas secuencias. `values()` devuelve el registro de valores virtuales, que el verifier usa para comprobar que
las referencias pertenecen a la arena vigente.

El destructor libera en el orden establecido por los vectores propietarios. Las aristas internas, los
punteros a símbolos y los punteros a tipos son no propietarios. Por ello, `TypeContext`, símbolos y arena deben
vivir más que cualquier inspección del módulo.

### 3.2 Valores, bloques, funciones, globals y módulo

`MirValue` es la identidad virtual `(id, type)`. `MirBlock` conserva ID, nombre e instrucciones en orden. Sólo
la última instrucción determina `terminated()`. `replaceInstructions` permite que una fase posterior sustituya
el contenido completo sin cambiar la identidad del bloque.

`MirFunction` distingue prototipo y definición mediante `defined`. Conserva parámetros, locales y bloques en
orden; el primer bloque es la entrada. `MirGlobal` conserva símbolo e inicializador constante opcional.
`MirModule` mantiene globals y funciones en orden de inserción. `findFunction` compara identidad del puntero a
símbolo mediante búsqueda lineal.

### 3.3 Instrucción y canales de payload

Una `MirInstruction` contiene opcode y rango fuente, resultado opcional, operands, entradas PHI, símbolo,
constante, tipo auxiliar, flags de forma y tres targets de control. Cada opcode utiliza sólo un subconjunto.
Los setters permiten construcción incremental y el verifier rechaza combinaciones incompatibles.

`resetTo` cambia el opcode y limpia operands, PHI, símbolo, tipo auxiliar, constante, flags y targets. Conserva
el resultado y el rango fuente. Este detalle es contractual para las fases que reescriben una instrucción sin
invalidar sus usos ni perder procedencia.

`replaceOperand` cambia todas las apariciones de un valor, tanto en operands ordinarios como en entradas PHI,
y devuelve el número total de sustituciones.

## 4. Opcodes y terminadores

`MirOpcode` contiene 47 valores desde `Parameter` hasta `Unreachable`. La tabla textual de `mirOpcodeName`
debe cubrirlos exactamente. `isTerminator` clasifica `Jump`, `Branch`, `Return` y `Unreachable`; ninguna otra
operación termina un bloque.

Las familias son:

- valores y símbolos: parameter, constant, load/store symbol, load/store indirect;
- aritmética, bitwise, shifts y comparaciones;
- conversiones y copy;
- llamadas y addressing de índice, field y pointer;
- allocation, deallocation y stack allocation;
- control: PHI, jump, branch, return y unreachable.

Los opcodes no codifican por sí solos todas sus invariantes. El tipo del resultado, operands, símbolo, tipo
auxiliar, flags y targets forman juntos el contrato que valida `MirVerifier`.

## 5. Algoritmo de construcción

### 5.1 Raíz, globals y funciones

`MirBuilder::build` exige una raíz HIR `CompilationUnit`, crea un módulo y baja sus globals en orden. Una función
se busca primero por identidad de símbolo: una declaración puede crear el prototipo y una definición posterior
completar el mismo objeto. Definiciones duplicadas se rechazan.

Los parámetros se registran en la función y, para funciones definidas, producen en el bloque de entrada una
instrucción `Parameter` seguida de `StoreSymbol`. Las variables locales se agregan una sola vez. Un cuerpo que
termina sin terminador recibe `Return` si la función es void y `Unreachable` en otro caso.

El builder conserva el primer error mediante `fail`. `build` no limpia `error_`; el uso documentado por la
implementación es una instancia por construcción.

### 5.2 Disciplina de emisión

`emit` crea una instrucción en el bloque vigente. `emitValue` crea además su `MirValue` resultado. Antes de
emitir código ordinario, `ensureWritableBlock` comprueba si el bloque ya terminó. En ese caso crea un bloque
`unreachable.code`, de modo que el lowering conserva el código fuente posterior sin agregar instrucciones
después de un terminador.

`emitJumpIfNeeded` sólo añade un salto si el bloque actual continúa abierto. Esta regla evita terminadores
dobles al unir ramas cuyos cuerpos ya retornaron, saltaron o quedaron inalcanzables.

### 5.3 Statements y CFG

Un block conserva el orden de statements. Variables locales materializan su initializer y un `StoreSymbol`.
Returns, expression statements y control básico delegan al lowering de expresiones y terminadores.

Un `if` crea bloques para ramas y merge según sean necesarios. La condición termina el bloque de origen con
`Branch`; cada rama abierta salta al merge. `while` y `for` crean bloques explícitos para condición, cuerpo,
incremento y salida. Una pila iterativa de `LoopTargets` enlaza `break` con exit y `continue` con condition o
increment según la forma del loop.

No existe terminador implícito entre bloques: cada arista se representa mediante `Jump` o `Branch`.

### 5.4 Expresiones y short circuit

`lowerExpression` despacha por kind HIR. Cada expresión que produce valor debe devolver un `MirValue`; cada
operación void debe completar sin inventar resultado.

`&&` y `||` no se reducen a un opcode eager. El builder crea bloques RHS, short y merge, reserva un resultado
compartido y lo define con copies en ambas rutas. Esa forma PreSsa preserva evaluación condicional y deja a la
fase SSA la normalización posterior.

Compound assignment e increment cargan el valor anterior, aplican conversiones y operación, almacenan el
nuevo valor y seleccionan resultado previo o posterior según el contrato postfix.

### 5.5 Direcciones, memoria y conversiones

`AddressRef` separa una dirección de símbolo de una dirección calculada. El primer caso permite
`LoadSymbol`/`StoreSymbol`; el segundo usa `LoadIndirect`/`StoreIndirect`. Índices, fields y dereference
materializan sus respectivos opcodes. Arrays y strings transportan bounds-check y count fijo; pointers no
inventan un límite.

Allocation, deallocation y stack allocation conservan tipo auxiliar, array form y count. Las conversiones de
identidad devuelven el valor original, `ToVoid` devuelve ausencia de valor y las demás producen `Convert`.
La elección entre suma, resta y diferencia de pointers depende de tipos de operands y resultado, no sólo del
token HIR.

## 6. Dump textual

`MirDumper` imprime globals y funciones en su orden. Los locals reciben ordinales estables por función. Un
valor se escribe como `%v<ID>:<tipo>` y un bloque con su identidad y nombre. Cada instrucción incluye resultado,
opcode y sólo el payload aplicable: operands, PHI pairs, símbolo, constante, tipo auxiliar, flags o targets.

Las cadenas de bytes escapan slash, comillas, LF, CR, tab y NUL. Bytes imprimibles ASCII se conservan y los
demás usan hexadecimal. El formato sirve como superficie diagnóstica y como oracle futuro de determinismo; no
es un formato binario persistente.

## 7. Verificación

### 7.1 Protocolo y modos

`MirVerifier::verify` limpia su error y valida módulo, globals y funciones. `PreSsa` admite valores compartidos
por la forma que produce el builder. `Ssa` exige definiciones únicas y habilita las reglas PHI correspondientes.
`fail` conserva el primer diagnóstico y lo asocia con la frontera comprobada.

### 7.2 Módulo y funciones

Los símbolos de globals y funciones son obligatorios y únicos dentro del módulo. Globals deben referir una
variable global y su inicializador, cuando existe, debe coincidir en kind y tipo.

Un prototipo no puede tener blocks, locals ni parámetros materiales. Una definición requiere firma función,
entry block, parámetros posicionales de tipo exacto, locales únicos y bloques pertenecientes a la misma
función. Cada valor registrado por la arena tiene tipo no nulo; en SSA cada resultado se define exactamente
una vez.

### 7.3 Bloques, parámetros y PHI

Todo bloque debe contener instrucciones, terminar exactamente con un terminador y no contener terminadores
previos. Las PHI sólo pueden formar un prefijo contiguo. `Parameter` sólo aparece en entry y cada posición se
define exactamente una vez con el tipo de la firma.

En SSA, una PHI requiere resultado, entradas no vacías, tipos idénticos y bloques pertenecientes a la función.
La revisión actual no comprueba aquí que cada bloque entrante sea realmente predecessor ni que una definición
domine cada uso; esas responsabilidades se completan en CFG, Dominance y SSA.

### 7.4 Reglas por opcode

El verifier aplica cantidad de operands, presencia o ausencia de resultado y relaciones exactas de tipos para:

- constants, copies, parameter y PHI;
- símbolos, loads, stores y llamadas;
- conversiones, unary, aritmética, bitwise, shifts y comparaciones;
- suma, resta y diferencia de pointers;
- index, field, dereference y bounds-check;
- allocation, deallocation y stack allocation;
- jump, branch, return y unreachable.

Las llamadas contrastan firma, retorno y argumentos. Las operaciones pointer exigen pointee completo cuando
corresponde. Los targets deben pertenecer a la función y los terminadores no pueden conservar payload ajeno.

## 8. Invariantes observables

1. IDs de bloques y valores son monotónicos e independientes.
2. El orden de globals, funciones, parámetros, locals, blocks e instrucciones se conserva.
3. Una función prototipo permanece vacía y una definición posee entry.
4. Un bloque no contiene instrucciones después de su único terminador final.
5. El resultado de una instrucción conserva identidad durante `resetTo`.
6. Los tipos y símbolos se contrastan por la identidad publicada por capas anteriores.
7. Short circuit se representa como CFG, no como evaluación eager.
8. Cada opcode usa una forma de payload definida y rechaza residuos incompatibles.
9. El primer error del builder y del verifier es estable.
10. El dump refleja determinísticamente el orden del modelo.

## 9. Errores, límites y complejidad

- `MirBuilder` recorre expresiones HIR recursivamente; profundidad adversarial consume stack del host.
- `build` no reinicia el error de una instancia reutilizada.
- `MirModule::findFunction` es lineal en la cantidad de funciones.
- `MirBlock::terminated` observa sólo la última instrucción; el verifier detecta terminadores anteriores.
- El verifier preindexa todos los resultados y comprueba pertenencia y tipos, pero no dominancia ni orden de uso.
- PHI comprueba pertenencia de bloques, no relación predecessor-successor en esta capa.
- Los setters del modelo permiten estados parciales hasta el gate del verifier.

Construcción, dump y verificación son lineales en objetos y aristas, salvo búsquedas lineales explícitas y la
profundidad del lowering recursivo. Estas observaciones se documentan; la fase actual no autoriza refactors.

## 10. Dependencias y consumidores

| Dirección | Componentes | Contrato |
|---|---|---|
| entrada | Basic, Source, Language, Sema y HIR | rangos, tipos, símbolos, constantes, offsets y árbol HIR |
| interna | STL | vectores, strings, streams, sets/maps y ownership manual de objetos |
| salida | CFG, SSA, Dominance, Passes y LLVM | módulo MIR tipado, CFG explícito, dump y verdict |

Un fixture futuro debe conservar vivos arena, tipos y símbolos durante construcción, dump y verificación. Los
tests de fases posteriores no sustituyen los contratos propios del modelo y builder MIR.

## 11. Reconstrucción y estrategia de prueba

Un reemplazo compatible debe reproducir:

- las seis entidades propietarias del modelo, sus secuencias y orden;
- los 47 opcodes, nombres y clasificación de terminadores;
- todos los canales de payload y la semántica exacta de `resetTo`;
- lowering de funciones, statements, expresiones, direcciones, memoria y short circuit;
- disciplina de bloques escribibles y terminadores explícitos;
- formato textual del dumper;
- reglas `PreSsa` y `Ssa`, incluidas todas las matrices por opcode.

Las 38 familias `CF-MIR-*` separan esa superficie por contrato observable. Cada una posee ocho matrices en
`HOST-MIR-TEST-PLAN.md`: mínimo, representativo, dependencia ausente, relación inválida, límites, variantes,
determinismo y escala. Las matrices permanecen en estado `DESIGNED`; no son pruebas ejecutables.

### Gate de las veinte preguntas

1. **Por qué existe:** normaliza HIR en CFG tipado para análisis y codegen.
2. **Problema:** hace explícitos valores, bloques, memoria y transferencias de control.
3. **Entradas:** HIR, arena, tipos, símbolos y stream.
4. **Salidas:** módulo MIR, error, verdict o dump.
5. **Contratos:** 38 familias trazadas.
6. **Estructuras:** arena y seis entidades enlazadas por punteros no propietarios.
7. **Algoritmos:** lowering CFG, emisión, dump y verifier por opcode.
8. **Invariantes:** sección 8.
9. **Errores:** first-error latch y diagnósticos por frontera.
10. **Estados:** prototipo/definición, bloque abierto/terminado y PreSsa/Ssa.
11. **Ownership:** arena propietaria; tipos, símbolos y aristas no propietarios.
12. **Interacción:** tabla de sección 10.
13. **Formatos:** dump textual MIR determinista.
14. **Complejidad:** lineal salvo búsquedas y profundidad documentadas.
15. **Alternativas:** SSA inmediato o nodos tipados por opcode no están implementados en esta revisión.
16. **Elección vigente:** modelo uniforme, lowering separado y verifier posterior.
17. **Evolución:** no reconstruida en esta fase de revisión congelada.
18. **Defectos históricos:** no se infieren; límites vigentes están en sección 9.
19. **Pruebas:** plan contractual nuevo; batería histórica sólo como respaldo congelado.
20. **Reimplementación:** secciones 2–11 y los inventarios contienen los datos necesarios.

**Dictamen documental:** `RECONSTRUIBLE`. No equivale a pruebas implementadas ni a cobertura medida.
