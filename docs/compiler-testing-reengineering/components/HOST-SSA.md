# Host.SSA — Forma estática de asignación sobre MIR

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo las inferencias marcadas.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 14 archivos, 2,645 líneas y 83,761 bytes.
- **Inventario:** 102 símbolos físicos, 42 contratos y 336 matrices de prueba diseñadas.
- **Validación realizada:** lectura estática completa, extracción mecánica, trazado y segunda auditoría.
- **No realizado:** build, ejecución, baseline, cobertura o modificación del producto.

## 2. Responsabilidad y frontera

Host.SSA transforma cada función MIR definida en otra función MIR que cumple forma SSA. Recibe, para la misma
fuente, el módulo MIR, su CFG y sus resultados de dominancia. El resultado conserva globals, prototipos,
símbolos, tipos, orden de funciones y payload de instrucciones, pero:

1. excluye bloques inalcanzables;
2. promociona almacenamiento local cuya dirección no se toma;
3. elimina los `LoadSymbol` y `StoreSymbol` de ese almacenamiento;
4. inserta `Undef` y `Phi` donde corresponde;
5. asigna una definición única a cada `MirValue` de salida;
6. valida forma, procedencia, dominancia y colocación de PHI mediante un verificador independiente.

La frontera no calcula CFG, dominadores ni frontera de dominancia. Tampoco baja a LLVM. Consume esos análisis y
publica un nuevo `MirModule` propiedad de `SsaArena` junto con metadata de correspondencia.

## 3. Modelo, ownership e identidad

### 3.1 Arena

`SsaArena` contiene un `MirArena` para todo el MIR de salida y posee con `new/delete` los `SsaModule` y
`SsaFunction`. Destruye primero funciones SSA y después módulos. Las referencias a MIR fuente, CFG, dominancia,
símbolos y tipos son no propietarias y deben sobrevivir a la representación SSA.

### 3.2 Módulo

`SsaModule` conserva el par `sourceModule/module`, la secuencia de metadata de funciones definidas y un índice
hash por identidad de `MirFunction` fuente. Prototipos aparecen en el MIR de salida, pero no reciben
`SsaFunction`. `addFunction` conserva incluso un puntero nulo en la secuencia y sólo crea índice cuando la
función y su fuente existen; el verifier cierra esos estados parciales.

### 3.3 Función

`SsaFunction` une cinco universos: función MIR fuente, función MIR de salida, CFG, dominancia y metadata SSA.
Conserva el conjunto ordenado de símbolos promocionados, dos mapas inversos source/output block y el conteo de
bloques inalcanzables excluidos. `mapBlock` usa `emplace`; no reemplaza una identidad ya registrada.

## 4. Promotabilidad y clasificación

`SsaPromotability` sólo considera símbolos `Variable` o `Parameter` que pertenezcan a `function.locals()` de
una función definida. Recorre todos sus bloques e instrucciones. Una sola `AddressOfSymbol` asociada al símbolo
lo vuelve no promocionable. Otros usos no lo excluyen. El orden del resultado es exactamente el orden de
`locals()`.

El builder guarda ese orden y un set auxiliar. Por separado cuenta definiciones de cada `MirValue` en bloques
CFG alcanzables. Los valores con más de una definición forman el universo virtual multi-definido; se ordenan de
forma ascendente por `MirValue::id()` mediante inserción iterativa. Los bloques inalcanzables nunca influyen en
esa clasificación.

## 5. Construcción del módulo y las funciones

### 5.1 Gate de procedencia

`SsaBuilder::build` limpia error y estado de función. Requiere tres módulos no nulos y exige:

- `cfgModule.mirModule == sourceModule`;
- `dominanceModule.cfgModule == cfgModule`.

Crea el MIR de salida, clona globals en orden y preserva inicializador explícito o ausencia de éste. Los
prototipos conservan símbolo y estado no definido. Cada definición necesita exactamente su CFG y su análisis de
dominancia.

### 5.2 Pipeline por función

Para cada definición se crea una función de salida con el mismo símbolo, se copian los símbolos locales en
orden y se ejecuta:

```text
resetFunctionState
→ classifyVariables
→ createOutputBlocks
→ planPhiPlacement
→ materializePlannedPhis
→ renameFunction
→ reconstruir parameters por índice
```

Cada parámetro debe haberse materializado; un hueco posicional falla. El reset elimina todos los mapas, sets,
stacks, planes y resultados de la función anterior.

### 5.3 Universo de bloques

Los bloques se recorren en orden MIR y sólo se clonan cuando el nodo CFG existe y es alcanzable. Se preservan
nombre y orden relativo. El entry alcanzable debe quedar primero y corresponder al entry de salida. La
diferencia entre bloques fuente y alcanzables se publica como `excludedUnreachableBlocks`.

## 6. Colocación de PHI

### 6.1 Símbolos

Para un local promocionado `Variable`, el entry cuenta como definición por el valor inicial indefinido. Cada
`StoreSymbol` alcanzable agrega su nodo una sola vez. Para un `Parameter`, las definiciones proceden de sus
stores/parameter lowering; un parámetro sin definición alcanzable es error.

### 6.2 Valores virtuales multi-definidos

El entry se incluye como definición inicial y después se agregan, sin repetir nodo, todas las instrucciones
alcanzables cuyo `result()` es el mismo valor MIR fuente.

### 6.3 Frontera iterada

`placePhiSpecs` usa una worklist vectorial y un índice creciente, no recursión. Para cada nodo de definición
visita su frontera de dominancia, ignora nodos nulos, inalcanzables o sin bloque y agrega una sola especificación
por nodo. Un nodo de frontera que no era definición entra a la worklist; así se calcula el cierre iterado.

Cada `PhiSpec` identifica exactamente un símbolo o un valor virtual y su tipo. La materialización recorre los
bloques fuente en orden, crea `Phi` al comienzo lógico del bloque de salida, asigna un resultado nuevo y conserva
la asociación símbolo/valor mediante listas paralelas cuya cardinalidad debe coincidir.

## 7. Renombrado y clonación

### 7.1 Recorrido iterativo del árbol dominador

`renameFunction` usa `vector<RenameFrame>` como stack explícita. Cada frame conserva nodo, siguiente hijo,
estado de entrada y las claves empujadas. En entrada:

1. publica las definiciones PHI del bloque;
2. si es entry, emite seeds `Undef`;
3. clona o elimina las instrucciones fuente;
4. agrega el valor vigente a los PHI de sucesores.

Al salir, desapila valores y símbolos en orden inverso. Así una definición queda vigente exactamente en el
subárbol dominado correspondiente, sin recursión y sin contaminar hermanos.

### 7.2 Seeds `Undef`

En entry se emite un `Undef` por cada local `Variable` promocionado. Los parámetros no reciben seed artificial.
También se emite uno por valor virtual multi-definido. Esos seeds se guardan como definición exterior al frame,
por lo que permanecen disponibles durante todo el recorrido.

### 7.3 Promoción de loads y stores

- `LoadSymbol` promocionado no se clona: su resultado fuente apunta directamente al valor vigente del símbolo.
- `StoreSymbol` promocionado exige exactamente un operando, lo resuelve y lo empuja como nueva definición.
- `AddressOfSymbol` nunca debe llegar asociado a un símbolo promocionado porque la clasificación lo excluyó.

### 7.4 Resto de instrucciones

`cloneInstructionShell` rechaza `Phi` y `Undef` en MIR fuente. Conserva opcode, rango, símbolo, tipo auxiliar,
constante, conversión, inmediato, offset y targets. Cada target debe mapear a un bloque SSA alcanzable. Después
resuelve operandos y asigna un resultado nuevo. Un resultado multi-definido se apila; cualquier otro se registra
en el mapa directo.

Los `Parameter` usan su immediate como índice de `outputParameters`; índice ausente o fuera de rango falla. Un
resultado requiere tipo semántico no nulo.

### 7.5 Incomings de PHI

Cada bloque, una vez renombrado, visita sus sucesores CFG alcanzables. Para cada PHI planificado obtiene el valor
vigente del símbolo o valor virtual y registra `(predecessor,value)`. `addPhiIncomingOnce` permite repetir el
mismo par, pero rechaza que el mismo predecessor entregue dos valores distintos.

## 8. Dump textual

`SsaDumper` no produce texto para módulo nulo o sin MIR de salida. Un módulo válido comienza con `ssa.module` y
conserva orden de globals y funciones. Globals publican tipo e inicializador explícito o `zeroinit`; prototipos
publican `extern-C` cuando aplica.

Una definición publica símbolos promocionados, conteo de bloques excluidos, bloques e instrucciones. Los valores
usan `%v<ID>:<tipo>`; los símbolos usan `@nombre` y agregan `#índice` cuando pertenecen a los locales de la
función. PHI imprime pares `[bb<ID>: %v...]`; otras instrucciones imprimen operandos y targets.

Las constantes cubren none, enteros signed/unsigned, floating con precisión 17, boolean, null y string. Strings
escapan barra, comillas, controles comunes y bytes no imprimibles como `\xNN`, restaurando flags y fill del
stream después del byte hexadecimal.

## 9. Verificación independiente

### 9.1 Forma MIR y módulo

`verify` limpia todo estado y primero delega forma/tipos a `MirVerifier` en modo SSA. Luego exige cardinalidad de
globals y funciones, identidad y orden de símbolos y metadata `SsaFunction` exactamente para cada definición.

### 9.2 Procedencia y promoción

Cada función debe preservar definición, símbolo, CFG, dominancia y orden/identidad de locales. El verifier
recalcula la promotabilidad sin llamar a `SsaPromotability`: vuelve a recorrer source blocks, detecta
`AddressOfSymbol` y compara contenido, orden y unicidad.

### 9.3 Bloques y definiciones

El verifier recalcula reachability desde CFG, exige mapa bidireccional sólo para alcanzables, conteo de
excluidos, orden fuente y entry. Después registra cada resultado de instrucción; cualquier `MirValue` con dos
definiciones de salida falla.

### 9.4 Instrucciones, seeds y dominancia

No puede quedar load, store ni address de un símbolo promocionado. Todos los `Undef` viven en entry. Cada
variable promocionada tiene exactamente un seed, cada parámetro cero y el número de seeds virtuales iguala el
número de valores fuente multi-definidos.

Un uso ordinario en el mismo bloque debe estar después de su definición. En otro bloque, el nodo de definición
debe dominar al de uso. Un incoming de PHI se valida en el predecessor: la definición debe anteceder su
terminador o dominar ese predecessor.

### 9.5 PHI

Cada PHI tiene exactamente un incoming por predecessor CFG alcanzable único. Un PHI con símbolo sólo puede
referir almacenamiento promocionado. El verifier recalcula de forma iterativa:

- nodos de definición de cada símbolo;
- frontera de dominancia iterada esperada;
- valores fuente multi-definidos;
- conteo total esperado de PHI virtuales.

La colocación de PHI de símbolos se compara por nodo y cardinalidad. Para valores virtuales se compara el total
de PHI derivado de cada cierre IDF.

## 10. Invariantes observables

1. MIR, CFG y dominancia pertenecen a la misma cadena de procedencia.
2. Globals, funciones, símbolos y locales conservan identidad y orden.
3. Sólo funciones definidas tienen metadata SSA.
4. Sólo bloques CFG alcanzables aparecen en salida, en orden MIR.
5. Sólo variables o parámetros locales sin dirección tomada se promocionan.
6. Cada valor SSA de salida tiene una definición única.
7. Todo uso ordinario está dominado; todo incoming es válido en su edge predecessor.
8. Cada PHI contiene exactamente un incoming por predecessor alcanzable único.
9. Los PHI aparecen en el cierre de frontera de dominancia iterada esperado.
10. Cada local promocionado recibe un seed; los parámetros no.
11. El renombrado y la colocación usan memoria explícita, no stack de llamadas recursivo.
12. Builder y verifier conservan únicamente el primer error de una operación.

## 11. Complejidad y límites vigentes

- Construcción base y clonación: `O(I + B + E)` antes del costo de PHI.
- Cierre IDF: proporcional a nodos procesados y tamaños de frontera por variable/valor.
- Renombrado: `O(B + I + E + P)` con stack explícita, donde `P` son incomings de PHI.
- `isPromotable`: escaneo de todos los bloques/instrucciones por símbolo; para muchos locales puede ser
  `O(L·I)`.
- Ordenar valores multi-definidos usa inserción, `O(M²)` en el peor caso.
- `SsaFunction::isPromoted` y helpers de deduplicación usan búsquedas lineales.
- El verifier recalcula varias veces definiciones y fronteras; privilegia independencia del builder sobre costo.
- La verificación de valores virtuales contrasta cardinalidad total de PHI, mientras la de símbolos contrasta
  nodos exactos.

Estos límites describen la revisión congelada; no son autorización para optimizarla en esta fase.

## 12. Estrategia de prueba futura

Los 42 contratos se diseñan en `test-plans/HOST-SSA-TEST-PLAN.md`. Cada contrato tiene ocho matrices: nominal
mínima, nominal representativa, prerrequisito ausente, estructura inválida, límites/duplicados, variantes,
determinismo y escala. La unidad de prueba es el contrato estable, no cada rama interna.

Las pruebas unitarias deben construir MIR/CFG/dominancia pequeños y controlados. Las pruebas de componente deben
comparar tanto el MIR SSA como el dictamen independiente del verifier. El dump requiere oracles byte a byte. Los
casos `NIGHTLY` ejercitan grafos profundos y anchos para demostrar que los algoritmos iterativos no dependen del
stack del proceso.

## 13. Reconstructibilidad

Con esta especificación, los manifiestos, los 102 símbolos, los 42 contratos y el plan de 336 casos, un
ingeniero puede reconstruir ownership, modelo, clasificación, algoritmo IDF, renombrado, clonación, dump y
verificación. La segunda auditoría está en `audits/TANDA-07-HOST-SSA-AUDIT.md`.

**Resultado:** `RECONSTRUIBLE` para la revisión congelada. No implica que la suite futura esté implementada ni
que se haya ejecutado validación.
