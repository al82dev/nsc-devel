# NativeSharp 1.0.1-dev — cambios acumulados desde 0.9

Esta lista resume la línea actual para consumidores. Los razonamientos, repros y commits completos
permanecen en `mkdown/`; aquí sólo se conserva el resultado que afecta cómo se programa, depura o
empaqueta con NativeSharp 1.0.1.

## Lenguaje y Core
- `volatile` marca campos escalares, enum o puntero crudo para accesos MMIO observables sin
  convertirlos en operaciones atómicas ni insertar fences. La semántica atraviesa HIR/MIR/LLVM y
  NSMETA 1.14, incluidos campos de instancia, estáticos y asignaciones compuestas; tipos managed,
  agregados, ownership y colocaciones no-field se rechazan.
- `AtomicInt64` queda certificado para todo el rango signed de 64 bits, overflow definido,
  alineación 8, formas legacy/ordenadas y retornos exactos. El target Windows x64 emite atomics i64
  nativos sin exports `Interlocked*64` ni fallback `__atomic_*`.
- `AtomicPointer` publica Load/Store/Exchange/CompareExchange sobre `void*` no propietario. Conserva
  identidad y `null`, aplica órdenes explícitos, emite atomics LLVM `ptr` alineados a ocho bytes y
  no usa conversiones a entero, imports Win32 ni helpers externos.

- `Interlocked` ofrece Load/Store/Increment/Decrement/Add/Exchange/CompareExchange estáticos sobre
  ubicaciones `ref int` y `ref long`, y Load/Store/Exchange/CompareExchange sobre `ref void*`.
  Las formas legacy/ordenadas reutilizan el lowering atómico existente, conservan retornos y
  ownership, y evalúan la dirección una sola vez.
- Los locales `ref T alias = ref ubicación;` crean alias no propietarios de storage mutable y
  direccionable con tipo exacto. Lecturas, escrituras, incrementos, encadenamiento y reenvío `ref`
  conservan la misma ubicación; temporales, readonly, conversiones y capturas por lambda se
  rechazan.
- Los métodos y funciones `ref T` devuelven storage mutable con `return ref`; sus llamadas son
  lvalues que conservan la dirección en asignaciones, reenvíos y dispatch virtual/interfaz.
  NSMETA 1.15 transporta el modo y la ABI de puntero entre bibliotecas precompiladas.
- `NativeSharp.Core.Threading.MemoryOrder` publica el vocabulario ABI estable para atomics 2.0:
  `Relaxed`, `Acquire`, `Release`, `AcquireRelease` y `SequentiallyConsistent`; las operaciones sin
  parámetro conservan por ahora su semántica `seq_cst`.
- `AtomicInt32` y `AtomicInt64` aceptan orden explícito en `Load` y `Store`; validan el
  subconjunto legal en compilación y emiten LLVM `monotonic`, `acquire`, `release` o `seq_cst`
  con alineación natural, sin dependencias `__atomic_*`.
- `Increment`, `Decrement`, `Add` y `Exchange` aceptan los cinco `MemoryOrder`, emiten
  `atomicrmw` alineado en 32/64 bits y mantienen sus contratos de retorno.
- `CompareExchange` acepta órdenes independientes de éxito y fallo. El frontend rechaza órdenes
  de fallo Release/AcquireRelease o más fuertes que éxito; LLVM recibe ambos órdenes en un
  `cmpxchg` alineado y la API mantiene el retorno del valor anterior.
- `AtomicFence.Thread` y `AtomicFence.Compiler` ofrecen fences explícitos entre hilos y de
  compilador. Se emiten directamente como LLVM `fence` de alcance global o `singlethread`,
  rechazan Relaxed y órdenes seleccionados en runtime.



- La gramática 2.0 reconoce bloques `asm { ... }` dentro de funciones y métodos mediante un nodo
  AST explícito y diagnostica su uso en namespace. Un modo léxico acotado al cuerpo conserva bytes,
  saltos físicos y rangos exactos por línea, incluso para puntuación que no pertenece al lenguaje
  NativeSharp. La semántica ejecutable se completa de forma incremental en la tanda 4.
- Los bloques asm aceptan una cláusula de entradas `in(expr0, expr1);`. Cada expresión se conserva
  como operando AST posicional, participa en resolución de nombres y corresponde por orden a los
  placeholders `$0`, `$1`, etc.; constraints y lowering permanecen en ítems posteriores.
- La cláusula contextual `out(destino0, destino1)` precede a `in`, conserva ordinales `$outN` y
  exige lvalues modificables. El análisis la registra como escritura para no leer una local sin
  inicializar antes de que assembly produzca su valor.
- Cada operando asm acepta un constraint string opcional. Inputs normalizan a `r`, outputs a `=r`,
  y las formas explícitas permiten registros fijos y outputs de lectura-escritura. Las direcciones
  inválidas o estructuras mal formadas se rechazan antes del lowering.
- Assembly admite inmediatos integrales constantes con `"i"` y memoria direccionable con `"m"`,
  `"=m"` o `"+m"`. El modelo semántico distingue addressability de una property con setter y no
  sintetiza temporales para hacer válido un operando.
- `clobber("registro", ...)` declara registros destruidos por un bloque asm, conserva su orden
  canonicalizado y rechaza duplicados o conflictos con constraints de registro fijo.
- Los clobbers reservados `"cc"` y `"memory"` declaran efectos de condition codes y memoria y se
  conservan en el AST con categorías distintas de un registro físico.
- Assembly prohíbe usar o destruir el stack pointer y marca clobbers no volátiles de Windows x64
  para que la futura emisión preserve el ABI de la función.
- Labels y saltos directos locales se resuelven dentro de cada bloque asm; transferencias externas,
  indirectas y call/return no pueden evadir el control de flujo estructurado.
- `TargetInfo` publica baseline `x86-64-v1`, catálogo de registros y requisitos de extensiones asm;
  registros desconocidos e instrucciones AVX/AES/BMI/etc. sin habilitar fallan antes de HIR.
- Assembly validado atraviesa el pipeline de proyecto como nodos propios HIR/MIR. El schema MIR 1.1
  conserva texto, líneas y ubicaciones, operands tipados con valor o dirección, constraints,
  clobbers y marcas ABI sin reparsing ni pérdida de orden.
- El backend emite esos nodos mediante inline asm LLVM en dialecto Intel. Soporta inputs de
  registro/inmediato/memoria, outputs `=r`/`+r`/fijos/memoria, resultados múltiples, clobbers y
  control local con labels únicos; el target x86 inicializa también su parser de assembly.
- MIR 1.2 materializa el contrato de optimización de asm: side effects obligatorios y resúmenes
  explícitos de clobber de memoria/flags. O2 no elimina bloques sin operands ni mueve o fusiona
  stores a través de `clobber("memory")`.
- Los perfiles `-g` y `-gline-tables-only` publican en PDB una dirección de stepping distinta para
  cada instrucción física de un bloque asm mediante ubicaciones CodeView estándar, sin dividir el
  contrato asm en múltiples llamadas.
- Hosts independientes C11 y C++20 certifican en ejecución la ABI de exports con asm para inputs,
  outputs, memoria y preservación del registro no volátil `rbx` en Windows x64.
- El inlining usa un contrato único de cuatro políticas: automático, preferido (`inline`),
  obligatorio (`forceinline`) y prohibido (`noinline`). Ninguna política puede alterar efectos,
  ownership, cleanups, excepciones, dispatch, ABI o identidad pública; las palabras clave y su
  lowering se incorporan en las siguientes entregas de la tanda.
- El inlining automático queda desactivado en O0 y usa el modelo target-aware de LLVM con
  agresividad conservadora en O1, balanceada en O2 y agresiva en O3. La configuración y el pipeline
  provienen de un solo mapa comprobado.
- `inline` puede marcar un método concreto con cuerpo. La preferencia sobrevive AST, modelo de
  miembros, HIR y MIR, se muestra en dumps y llega a LLVM como `inlinehint`; no convierte una
  sugerencia en obligación ni cambia el símbolo fuera de línea.
- `forceinline` exige expandir llamadas estáticamente resueltas, llega a LLVM como `alwaysinline` y
  activa la expansión obligatoria incluso en O0. Declaraciones sin cuerpo, políticas contradictorias
  y ciclos recursivos compuestos sólo por métodos obligatorios producen un diagnóstico de fuente.
- `noinline` preserva una llamada fuera de línea aun con optimización agresiva. La política atraviesa
  los modelos semánticos y llega a LLVM como `noinline`; formas sin cuerpo, duplicadas o combinadas
  con otra política explícita se rechazan.
- Las especializaciones de métodos genéricos, explícitas o inferidas, conservan la política de
  inlining de la plantilla. En O0 una especialización `forceinline` se expande, una `inline` sigue
  siendo opcional y una `noinline` conserva el call site.
- Los métodos de struct usan las mismas políticas sin boxing. Una llamada mediante interfaz se
  devirtualiza sólo cuando el receptor es una construcción concreta fresca convertida explícitamente;
  receptores polimórficos conservan el dispatch por interface map.
- NSMETA 1.13 conserva la política y el cuerpo de métodos ordinarios `inline`/`forceinline` para
  analizarlos e inlinearlos en consumidores `LibraryReference`. Las copias usan ODR/COMDAT por
  símbolo; `noinline` cruza como restricción de la declaración sin duplicar el cuerpo.
- Los productos que contienen una obligación `forceinline` conservan temporalmente caller y callee
  en un módulo LLVM incluso en O0. Así la expansión ocurre de verdad y el PDB publica su
  `S_INLINESITE`, las líneas del cuerpo y la ubicación de llamada; `inline` opcional y `noinline`
  siguen apareciendo como procedimientos fuera de línea en O0.
- `static_assert(condición[, "mensaje"])` certifica constantes y layouts dentro de bloques sin
  generar código de ejecución.
- `offsetof(T, campo)` obtiene como `nuint` el desplazamiento físico de un campo de struct o union
  usando el mismo layout canónico que el ABI, los arreglos y los headers C.
- `align(N)` puede aplicarse a un campo de instancia para elevar su alineación, su offset y la
  alineación final del agregado; el contrato se conserva en NSMETA y headers C.
- `T[*]` declara una cola inline flexible como último campo de un struct; `sizeof` cubre sólo el
  encabezado, el indexado es raw y el header C publica `T field[]`.
- `readonly struct` declara un valor inmutable después de construcción: todos sus campos de
  instancia deben ser `readonly`, mientras que sus campos estáticos conservan reglas independientes.
- Los receptores readonly se aplican a parámetros `in`, campos readonly, miembros marcados
  `readonly` y todos los miembros de instancia de un `readonly struct`; permiten lectura pero no
  stores ni llamadas a miembros mutadores.
- Los aliases de tipo pueden declarar parámetros (`using Buffer<T> = T*;`). Se expanden antes de
  la monomorfización, aceptan composición, reordenamiento y repetición de argumentos, y no crean
  una identidad nominal ni un símbolo ABI distintos del tipo subyacente.
- Las llamadas a métodos genéricos pueden omitir `<...>` cuando cada parámetro de tipo se deduce
  de los argumentos locales. La inferencia es estructural y exacta sobre valores, genéricos
  anidados, arrays, punteros y argumentos `ref/out/in`; no usa el tipo de retorno para adivinar.
- Los arreglos multidimensionales fijos usan `T[d0, d1, ...]`, almacenamiento inline contiguo y
  orden row-major sin cabecera ni asignación de heap.
- `array[i0, i1, ...]` accede directamente a arreglos fijos multidimensionales y escala cada índice
  por el stride canónico de su dimensión.
- `T[length] local;` reserva un arreglo local de longitud evaluada en ejecución directamente en el
  frame de la función, sin `new`, `delete` ni cabecera de heap; los campos y firmas siguen exigiendo
  dimensiones contractuales.
- `T[rows, columns] local;` compone dimensiones de ejecución en una sola reserva row-major; cada
  extensión se evalúa una vez al declarar y conserva sus strides aunque cambien las variables fuente.
- Cada dimensión VLA se valida antes de convertirla a `nuint`; el producto se comprueba antes de
  multiplicar y una reserva individual no puede superar 1 MiB de stack.
- El almacenamiento de un VLA no puede escapar mediante retornos, campos o capturas de lambdas;
  los aliases locales conservan su procedencia para que la regla no se pierda al copiar el puntero.
- `const string` es una constante de compilación utilizable en APIs y metadata.
- Strings nativos conservan unión condicional, literales y préstamos en constructores.
- Enums, conversiones numéricas y conversiones de usuario conservan identidad lógica y física.
- Las clases parciales anidadas participan en la misma agregación nominal que las de nivel superior.
- Punteros, handles, delegates `extern C`, callbacks, structs y arrays siguen el ABI Windows x64.

## Bibliotecas y DLLs

- NSMETA avanzó a 1.14 y publica clausura nominal externa, templates transitivos, cuerpos inline
  ordinarios reutilizables y campos volatile sin perder su contrato observable.
- Clases estáticas, interfaces, eventos, arreglos de tipos referencia y especializaciones genéricas
  cerradas sobreviven el consumo mediante `LibraryReference`.
- Callables, constructores y destructores precompilados conservan nombres físicos enlazables.
- Las especializaciones reutilizables y helpers EH aplican reglas ODR/COMDAT por símbolo.
- Los helpers locales de closures, inicializadores estáticos y constantes precompiladas aplican
  ownership e identidad física coherentes entre unidades, bibliotecas estáticas y DLLs.
- Las bibliotecas estáticas no venden como propias las definiciones de sus dependencias.
- La selección de runtimes depende de símbolos efectivamente alcanzados y deduplica DLLs idénticas
  por contenido, no por ruta incidental.
- El verificador compara de manera temprana NSMETA, archivos COFF, DLL e import library.
- NSMETA omite miembros privados e internos de tipos públicos sin perder el layout físico requerido;
  tipos usados sólo por campos estáticos privados ya no contaminan la clausura pública.

## Depuración

- Los productos optimizados con debug completo y cualquier producto con obligaciones `forceinline`
  conservan temporalmente un módulo LLVM único para que caller, callee e inline sites permanezcan
  disponibles al generar CodeView/PDB. O0 sin `forceinline` y los perfiles no afectados conservan
  la partición paralela.
- Se formalizó el contrato CodeView/PDB para líneas, variables, tipos y funciones sintetizadas.
- Los checksums SHA-256 y `-fdebug-prefix-map` permiten identidad de fuente reproducible.
- Se documentaron y cubrieron `this`, structs, scopes, thunks, dispatch de interfaces, SEH,
  excepciones, closures y código optimizado.
- El verificador PDB reconoce helpers locales y nombres físicos largos sin confundirlos con símbolos
  públicos ausentes.
- La certificación PDB inspecciona todas las unidades de código particionadas y permite ejecutar su
  evidencia dirigida únicamente en O0 sin reducir la cobertura O0/O2 del baseline.

## Herramientas y desarrollo local

- Cada build muestra una identidad única y ahora pertenece a `1.0.1-dev`.
- NSC Builder ofrece limpieza manual y segura de árboles de construcción.
- Los productos regenerables de CMake/CTest y bytecode Python dejaron de contaminar el índice.
- La documentación actual se instala y verifica bajo `share/nativesharp/docs/1.0.1`.

## Estado

La línea permanece en desarrollo. Este changelog no sustituye un baseline ni afirma certificación.
La promoción a `1.0.1` final se registrará como una entrega separada cuando exista evidencia nueva.
