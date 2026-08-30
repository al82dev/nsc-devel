# Stage 40.2 -- ABI Win64 de agregados: structs por valor de cualquier tamaño

Ronda elegida por Alberto tras el cierre de Bloque 1 ("ABI de structs + callbacks"). Cierra la
mitad del compilador; deja **pendiente explicito** la verificacion cruzada con host C bajo Wine y
los callbacks entrantes (abajo).

## El bloqueador que se midio

Antes de esta ronda, cualquier struct cuyo tamaño no fuera 1/2/4/8 bytes era inutilizable como
**valor de primera clase** en todo el lenguaje:

```
nsc: error: Stage 40 Windows x64 direct struct value ABI supports only 1, 2, 4 or 8-byte
aggregates; 'type:proj::proj.M4' has size 64
```

Y no solo en llamadas: un **local** de 12 bytes, sin ninguna llamada a la vista, fallaba igual.
Matriz medida: campos de clase y elementos de array **funcionaban** (acceso por direccion);
locals, copias, parametros y returns eran el hueco. Consecuencia directa: `Mat4` (64B), un record
tipo `MSG` (48B) y `WNDCLASSEXW` (80B) irrepresentables por valor -- **math (Stage 59) y windowing
(Stage 56) bloqueados por lo mismo** antes de empezar.

## Ground truth primero: clang, no razonamiento

Antes de escribir codigo se extrajo el contrato del IR de clang para `x86_64-pc-windows-msvc`
(C y C++):

- **1/2/4/8 bytes**: directo, como entero de ese ancho. `{float,float}` viaja como `i64` en GPR --
  Win64 nunca lo parte en XMM.
- **Todo otro tamaño -- 3 bytes incluido**: INDIRECTO. El parametro viaja como `ptr` plano (sin
  `byval`) a una copia que el **caller** hace para esa llamada; el return viaja por un puntero
  `sret(<tipo>) noalias align <n>` que va primero.
- **Metodos de instancia**: `this` primero, sret **segundo** (RCX = this, RDX = sret). Confirmado
  contra el IR C++ de clang.
- **La copia es por llamada**: el callee puede escribirla; re-pasar el struct hacia abajo hace
  OTRA copia (el fixture `chain` de clang lo muestra con un memcpy explicito por llamada).

## Diseño

El hallazgo que hizo el diseño barato: la arquitectura ya era **memory-homed**. El "carrier" de
`llvmType` solo cumple dos roles -- dimensionar el alloca del slot y mover el valor completo --
mientras que el acceso a miembros siempre fue GEP+offset por direccion. Entonces:

- **Carrier de agregado indirecto: `[N x i8]`**. Locals, copias, miembros, anidados, `default(T)`
  y genericos cerrados sobre el struct funcionan mecanicamente. La alineacion real del layout se
  estampa en cada alloca (un `[N x i8]` alinea 1 por defecto, y los loads tipados de miembros
  asumen alineacion natural).
- **El call boundary nunca ve ese tipo**. El mapeo (param indirecto -> ptr; return indirecto ->
  void + sret a la cabeza, despues de this) se aplica en los **nueve** sitios que construyen
  firmas o llamadas: `declareFunctions`, el prologo de definicion, los dos caminos de return
  estructurado (normal y spill de EH), los cuatro lowerers de llamada
  (directa/estatica/virtual/interfaz), el wrapper `export extern C`, y los dos emisores de
  delegate.
- **Callee de parametro indirecto**: el puntero entrante ES el home del parametro (la memoria
  pertenece a la llamada) -- cero copias extra.
- **Copias de argumento y slots sret: allocas HOISTED al entry block** (como clang). Emitidos en
  el call site se re-alocarian en cada iteracion de loop y crecerian el stack sin limite --
  verificado con 200k llamadas en loop.
- Atributos como clang: `sret(<carrier>) noalias align <n>` en definicion Y call site.

## Bug encontrado a mitad de ronda (medido, no leido)

Los **delegates** quedaron para el final y la sonda los agarro: un delegate tomando un struct de
64B leia basura -- **1 en -O0, 5 en -O2** para un fixture cuya respuesta es 4. La divergencia
O0/O2 es la firma de un type mismatch en el boundary, no de un bug de logica. Los dos emisores de
delegate recibieron el mismo mapeo (con el detalle de que el sret del branch de instancia va
despues del receiver, y el merge lee el slot en vez de hacer phi de un agregado).

## Tests que fijaban la limitacion vieja

Tres tests asertaban el **rechazo** ("supports only 1, 2, 4 or 8-byte aggregates") para que la
limitacion documentada no degenerara en miscompile silencioso -- el patron correcto mientras la
limitacion existia. Con la limitacion implementada, los tres se invirtieron a asserts positivos
(lower limpio + `sret([12 x i8])` visible en el IR), cada uno con su HISTORY note:
`ProjectOopLoweringTests`, `Stage40StructValueAbiTests`, `Stage52GenericCollectionsCompilerFixesTests`.

## Suite nueva: `Stage40_2AggregateAbiExecutionTests.cpp`

16 escenarios, 64 checks, PE real bajo Wine, `-O0` y `-O2`, exit codes exactos:

- guardas de regresion de los tamaños directos (1B y 8B siguen directos);
- local + copia de 64B (independencia de la copia); anidado 16B; 12B;
- static param+return; **aislamiento de la copia del callee** (Clobber no toca el original);
- **re-paso en cadena copia otra vez**;
- metodo de instancia con sret despues de this; **virtual** con sret; **interfaz** en ambas
  direcciones;
- 12B/24B (la regla es "≠1/2/4/8", no "mayor que 8");
- 200k llamadas en loop (hoisting de allocas);
- sret a traves de try/finally (composicion con el spill de EH);
- metodo de instancia del propio struct de 64B;
- generico cerrado sobre 64B (`Echo(T)` + `default(T)`);
- delegate con param y return de 64B (el caso que estaba roto).

## Verificacion

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
- `ctest` completo: **231/231, 100%** (230 previos + la suite nueva).
- Manifest regenerado y verificado dentro del zip extraido; extraccion aislada
  configure+build+ctest desde cero.

## PENDIENTE EXPLICITO (siguiente ronda)

1. **Verificacion cruzada con host C real**: exportar estos structs via `export extern C`,
   compilar un host C con clang msvc, linkear con lld y **ejecutar bajo Wine** intercambiando
   valores campo a campo. El wrapper ya emite el ABI correcto y los tests estructurales de Stage
   47/48 siguen verdes, pero la prueba de compatibilidad de verdad es el host corriendo. La
   infraestructura existe (Stage 48.3 compila y linkea hosts C; falta ejecutarlos).
2. **Callbacks entrantes**: un metodo NativeSharp como function pointer C que un host invoque (la
   mitad que `WndProc` necesita). Sondear primero que hace hoy `ExternCallback cb = Metodo;` con
   un extern C delegate.
3. `ref`/`out`/`in` no existen (ni lexer): fuera de alcance de esta ronda, en el roadmap §7.

## Nota de alcance

Los **constructores de struct** siguen rechazados ("Stage 40 structs support fields plus ordinary
instance/static methods...") -- restriccion deliberada del lenguaje preexistente, no parte del
ABI; queda como estaba.
