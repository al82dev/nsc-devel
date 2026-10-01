# Tanda operativa 43 — funciones, callables y eventos

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Cerrar las limitaciones §26, §27, §35, §53, §70, §73 y §128 sin crear un segundo
modelo de llamadas ni alterar el ABI Win64:

- separar una declaración libre válida de un uso que realmente exige una definición;
- permitir que una función libre participe en el mismo contrato contextual de delegates que un
  método estático;
- permitir method groups como argumentos cuando la firma delegate esperada es única;
- ejecutar eventos en orden de suscripción y mover una suscripción quitada/reinsertada al final;
- llevar la política `inline` de una función libre hasta LLVM;
- admitir `catch (Type)` cuando el valor de la excepción no se usa;
- resolver de forma explícita el contrato público de argumentos del entry point.

## Declaraciones y definiciones de funciones libres

Una declaración sin cuerpo es ahora un prototipo, no un error global. Puede coexistir con `Main`
y no produce HIR, MIR ni símbolo físico si nunca se selecciona. Una definición compatible en otro
archivo del mismo proyecto se fusiona por la identidad de firma existente; no se duplica el
símbolo y el cuerpo puede aparecer antes o después del prototipo.

El contrato continúa siendo fail-closed en el punto donde existe una obligación real:

- una llamada seleccionada a un prototipo ordinario sin definición emite
  `MissingFunctionDefinition`;
- convertir ese prototipo en un delegate produce el mismo diagnóstico causal;
- `extern C` conserva su ruta de importación;
- una función precompilada conserva su ruta NSMETA;
- definiciones duplicadas y declaraciones incompatibles siguen rechazándose.

Así se permite mantener headers/API separados por unidades sin aceptar un ejecutable con un
símbolo faltante.

## Funciones libres y method groups como delegates

El índice de funciones de namespace registra ahora tanto llamadas como expresiones de nombre.
Después de locales, parámetros y miembros —que conservan precedencia— una referencia libre se
materializa como method group estático usando el mismo espacio ordinal unificado que las llamadas
libres.

La conversión contextual exige coincidencia exacta de retorno y parámetros. Funciona en:

- inicialización o asignación de un delegate;
- nombre libre calificado y no calificado;
- argumento directo de una función o método cuyo parámetro esperado es delegate;
- overload sets cuando queda un único tipo delegate compatible.

Si varios tipos delegate son igualmente compatibles, el compilador no elige por orden: emite
ambigüedad. Si ninguno coincide, emite conversión inválida. HIR reconoce los ordinales de namespace
y MIR obtiene la función física mediante el índice de funciones libres; LLVM continúa creando la
misma celda delegate Windows x64, con receiver nulo para un target libre.

## Orden de eventos

La lista física de suscriptores era LIFO porque `+=` insertaba cada nodo en la cabeza. La invocación
recorría correctamente la lista, pero por ello observaba el orden inverso al escrito.

`EventAdd` ahora inicializa el nodo con `next = null` y lo agrega al final de la lista. Se conservan
la representación de dos punteros, la comparación semántica code+receiver, la eliminación y el
recorrido existentes. El resultado observable es FIFO:

- `A`, luego `B` ejecuta `A`, luego `B`;
- quitar `A` y volverlo a agregar deja `B`, luego `A`;
- una lista vacía sigue siendo una escritura directa sin recorrido.

No cambia el layout ni la firma del delegate/evento.

## `inline` libre

`inline`, `forceinline` y `noinline` se aceptan como políticas mutuamente excluyentes sobre una
función libre con cuerpo. No se aceptan sobre estado/tipos de namespace ni sobre un prototipo sin
cuerpo. La política entra en `NamespaceFunctionSymbol`, pasa por el adaptador callable y HIR, y usa
los atributos LLVM existentes de métodos (`inlinehint`, `alwaysinline` o `noinline`).

Cuando un prototipo y una definición están separados, la política efectiva procede de la
definición, que es la unidad que posee el cuerpo.

## `catch (Type)`

El nombre local de un typed catch es opcional:

- `catch (Problem problem)` conserva el binding local histórico;
- `catch (Problem)` realiza la misma selección RTTI y no introduce una variable;
- `catch` continúa siendo catch-all.

La semántica ya estaba preparada para no crear el local cuando el texto estaba vacío; se eliminó
la exigencia artificial del parser. No se cambiaron llaves, orden de catches, RTTI ni cleanup EH.

## Decisión del entry point

NativeSharp 3.0 conserva como único entry point público `static int Main()` o `int Main()` libre.
No se agrega `Main(string[] args)`. Los argumentos ya tienen un contrato Windows x64 productivo y
probado mediante:

- `Environment.GetCommandLineArgCount()`;
- `Environment.GetCommandLineArgs(string[] destination)`.

Mantener una sola ruta evita dos políticas de ownership para el mismo array, una asignación oculta
antes de entrar a `Main` y una segunda forma ABI de entry point. Esta es una decisión terminal de
3.0, no una deuda silenciosa.

## Archivos modificados

- `src/Parser/Parser.cpp`: modifiers de inlining libres y nombre opcional en typed catch.
- `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h` y
  `src/Frontend/ProjectNamespaceFunctionModel.cpp`: política inline, prototipos no usados y
  referencias de función como expresiones.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`: method groups libres, conversión contextual en
  argumentos, diagnóstico del uso sin definición y selección unificada de callables.
- `src/Frontend/ProjectOopHirModel.cpp`: omite prototipos sin uso, propaga inline y resuelve
  ordinales libres al crear delegates.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: inserción FIFO de suscriptores.
- `tests/unit/NamespaceFunctionSignatureTests.cpp`: fija el nuevo contrato de prototipo no usado.
- `tests/unit/Stage45ExceptionsSyntaxTests.cpp`: fija `catch(Type)` válido.
- `CMakeLists.txt`: registra el verificador dirigido T43.

## Archivos nuevos y razón

- `tests/integration/ns3_t43_callables_events/Api.ns`, `Impl.ns`, `Main.ns` y
  `T43Callables.nsproj`: proyecto multiunidad que ejecuta enlace de prototipo/definición, delegates
  libres calificados/no calificados, method groups directos, FIFO/reinserción y typed catch sin
  variable.
- `tests/integration/ns3_t43_callables_events/negative/*`: proyectos independientes para llamada
  sin definición, ambigüedad contextual y delegate libre incompatible.
- `cmake/VerifyNativeSharpT43CallablesEvents.cmake`: compila/ejecuta sólo el positivo O0,
  comprueba `inlinehint` en LLVM y valida los tres diagnósticos negativos.
- `docs/3.0/TANDA-OPERATIVA-43-FUNCIONES-CALLABLES-EVENTOS.md`: este expediente de diseño,
  archivos, decisiones, mejora y evidencia.

## Validación dirigida

- Construcción: `nsc` Debug/O0 completó correctamente.
- `nsc.ns3.t43.callables-events`: positivo O0 exit `0`, dump LLVM con `inlinehint` y tres
  negativos causales.
- `nsc.namespace-function-signatures`: passed.
- `nsc.stage42.delegates-events`: passed.
- `nsc.stage45.exceptions-syntax`: passed.
- Las cuatro pruebas dirigidas terminaron en 1.32 segundos.

No se ejecutó CTest completo, baseline, O1/O2/O3 ni trabajo remoto; no se instaló ni modificó
`C:\native-sharp`.

## Mejora obtenida

El diseño de headers/implementaciones libres ya funciona entre archivos sin exigir cuerpos
duplicados. Funciones libres y métodos estáticos pueden usarse de forma uniforme en APIs delegate,
incluido el paso directo como argumento. Los eventos tienen orden observable predecible y estable.
Las dos simplificaciones sintácticas aprobadas (`inline` libre y `catch(Type)`) ya tienen efecto
semántico real. El entry point queda congelado sobre `Main()` y el API explícito de Environment,
sin asignación oculta ni un ABI alternativo.
