# Diseño en investigación: excepciones, objetos y vida entre lenguajes

Estado: **alternativas y obligaciones de diseño**, no comportamiento implementado. Este documento no decide que las excepciones de NS y C++ sean equivalentes ni que un puente existente preserve todos los casos. Aplica al target Windows x64 de la línea inicial, con bibliotecas estáticas y DLL; ambos lenguajes pueden ser dueño del EXE.

## Evidencia de partida

- NativeSharp crea `__nativesharp_throw` y utiliza `RaiseException` con código propio (`src/Runtime/WindowsRuntimeBuilder.cpp:27`, `:6121-6195`). Su filtro comprueba el código NS y rechaza otros (`:6315-6323`); el propio cuerpo del filtro transporta un puntero en el registro SEH y lo publica en TLS local (`:6216-6247`). Esta ruta explica un contrato concreto de NS, no todas las combinaciones del sistema.
- Clang distingue `__C_specific_handler` y `__CxxFrameHandler3` (`clang/lib/CodeGen/CGException.cpp:123-137`) y su ABI Microsoft genera `_CxxThrowException` con objeto e información de captura (`clang/lib/CodeGen/MicrosoftCXXABI.cpp:786-793`). Compartir SEH o el target no identifica automáticamente los objetos de excepción.
- NSC genera cadenas de limpieza y `invoke` en `src/LLVM/Lowering/OopLlvmCleanup.cpp:96` y `:285`; sus wrappers `export extern C` tienen una política propia frente a la salida de excepciones NS (`OopLlvmThunksAndWrappers.cpp:737-746`). No debe reutilizarse ciegamente esa frontera como contrato C++ bidireccional.
- Una clase NS tiene descriptor con entrada de destrucción y otra de destrucción solo del objeto (`Support/DispatchGlobals.cpp:431-440`). El wrapper que libera ejecuta destrucción y después dealloc (`OopLlvmThunksAndWrappers.cpp:1007-1025`); un allocator personalizado puede tener descriptor asociado al objeto (`:1061-1063`).

## Qué debe funcionar

El diseño y la validación deben separar: (1) propagación a través de frames del otro lenguaje; (2) ejecución de destructores y limpiezas durante esa propagación; (3) captura tipada, captura general y selección por herencia; (4) `rethrow` y preservación de identidad/objeto; (5) vida y liberación del objeto de excepción; (6) excepciones durante construcción o cleanup; (7) `noexcept`, termination y contratos de NS; (8) cruce de módulos, CRT y runtimes; (9) callback que vuelve a entrar al primer lenguaje. Un resultado correcto en un punto no demuestra los restantes.

### Puntos concretos de captura y efecto

`src/LLVM/Lowering/OopLlvmControlFlow.cpp:117-155` crea un `catchswitch` cuyo handler usa `ehFilterFunction_`; solo tras aceptar ese filtro obtiene el objeto NS actual. En `:158-195` la selección de cláusulas revisa `catchAll` o consulta RTTI NS por tipo. Por tanto, el `catchAll` de **esa ruta** ocurre después del filtro NS: no hay evidencia de que capture una excepción C++ que el filtro no acepta. Las líneas `:200-214,425-463` muestran el manejo de excepción NS sin cláusula coincidente y la intención de ejecutar cleanups y relanzar el objeto. Esto no demuestra ejecución correcta de cleanup ante una excepción C++ extranjera; hay que verificar tanto la generación de `invoke`/funclets como la personalidad y la ejecución real.

En el análisis de cuerpos, `src/Frontend/Bodies/CallAnalyzer.cpp:4666-4673` verifica el error declarado de un método seleccionado y trata de forma distinta funciones async; `:3817-3822` hace una comprobación análoga para delegates. Una función C++ potencialmente lanzadora no necesariamente declara un conjunto cerrado de tipos de excepción. La integración necesita un efecto/contrato que represente esa posibilidad y una regla explícita para su interacción con `throws` y capturas NS, en vez de marcarla como que no lanza o inventarle un único tipo NS. Una ampliación del modelo de efectos o de la semántica puede ser necesaria; la sintaxis y política concreta aún no están decididas.

| Origen y consumidor | Qué debe decidir y preservar el diseño | Evidencia actual |
|---|---|---|
| C++ lanza, C++ captura después de atravesar NS | Unwind a través de frames NS, limpieza NS y regreso del mismo objeto C++ al handler de origen. | `OopLlvmCleanup.cpp:112-175` genera cleanuppads NS; comportamiento ante excepción C++ no probado. |
| C++ lanza, NS captura | Detección de excepción extranjera, coincidencia de tipos/catch-all, objeto original, vida y rethrow; también efecto estático de llamada C++. | Filtro NS actual rechaza códigos distintos al suyo y selección usa RTTI NS. Requiere un mecanismo adicional o extensión; no se conoce aún una traducción semánticamente completa. |
| NS lanza, NS captura después de atravesar C++ | Unwind por frames C++, destructores C++ y preservación del objeto/código NS hasta su handler. | Se conocen los dos mecanismos de lanzamiento, no su recorrido combinado. El wrapper `export extern C` existente puede atrapar/trapear la excepción NS y no se debe reutilizar como si preservara este caso. |
| NS lanza, C++ captura | Proyección/identidad de tipo para handlers C++, herencia, `catch (...)`, ownership del objeto y rethrow. | La excepción NS usa payload y código SEH propios; no hay evidencia de que forme el objeto e información que `_CxxThrowException` y los handlers C++ esperan. |

Las cuatro filas son capacidades que forman parte del objetivo, no una decisión de excluir alguna. La tabla separa hechos del código de una necesidad de implementación todavía sin validar. Preservar captura tipada e identidad puede necesitar una representación compartida o adaptaciones con acceso al objeto original y soporte en ambos frontends; decidir cuál requiere contrastar su comportamiento con los contratos completos, no solo producir un mensaje equivalente.

**Arista excepcional concreta.** `OopLlvmCleanup.cpp:285-310` consulta `buildCurrentUnwindDestination`: cuando obtiene un destino construye `invoke` y cuando no lo obtiene llama a `buildNoUnwindCall`. A pesar del nombre del helper, su cuerpo en `:45-66` emite `LLVMBuildCall2` o una llamada con bundle de funclet; en ese cuerpo no se observa que añada el atributo LLVM `nounwind`. No se debe inferir por el nombre que impide propagar una excepción, ni que una llamada sin `invoke` ejecute cleanups locales que no tienen arista. Para una operación importada de C++, el binding y el lowering deben preservar si puede lanzar, el contexto de cleanup y la política de la frontera; verificar en el IR resultante que se emitan la arista y funclets necesarios para ese contexto.

Clang `clang/lib/CodeGen/CGCall.cpp:2133-2138` añade el atributo `NoUnwind` cuando la especificación de excepción de la función está resuelta y `isNothrow()` lo indica. El contrato `noexcept` y la terminación ante una excepción escapada son cuestiones distintas de si un handler NS reconoce el payload. La metadata NS contiene `ffiUnwindPolicy`, pero las rutas documentadas para `export extern C`/callbacks C aplican su propio contrato y no equivalen automáticamente al contrato mixto C++. La nueva frontera debe representarse explícitamente en los modelos y en el análisis de efectos, sin cambiar el comportamiento de los imports/exports C existentes.

## Alternativas a comparar

### A. Propagación nativa a través de frames

Permitir que un mecanismo de excepción de origen atraviese frames del otro frontend cuando las tablas y las personalidades correspondientes puedan ejecutar cleanup correctamente. Analizar `invoke`, `funclet`, filtros y el unwind real sin cambiar por defecto toda la personality de NS. **Pendiente:** demostrar que cada frame ejecuta sus limpiezas y que `noexcept` y los límites externos mantienen su contrato. Propagación sin pérdida no implica que el otro lenguaje pueda hacer captura tipada del objeto.

### B. Adaptación explícita generada en el borde

El frontend del lenguaje que conoce el objeto captura en una frontera generada y administra una representación de la excepción que el otro lado pueda transportar o inspeccionar; las operaciones de consulta, liberación y eventual relanzamiento permanecen ligadas al módulo y runtime propietarios. Para C++, estudiar una representación que conserve el objeto original y permita relanzarlo mediante operaciones emitidas por Clang, sin suponer que un puntero bruto o una `exception_ptr` entre CRT distintos sean seguros. Para NS, estudiar una proyección que conserve la identidad y propiedad de su objeto y su código/descriptor.

**Pendiente crítico:** si la proyección cambia qué capturas coinciden, el tipo observado, el orden de limpieza o la identidad al relanzar, se documenta como diferencia semántica; no cuenta como interoperabilidad idéntica. Requiere diseño para vida, múltiples hilos, excepciones anidadas, módulos descargados y terminación. El adaptador se genera automáticamente por el toolchain; no impone wrappers manuales por biblioteca.

### C. Extensión del contrato de NSC y/o Clang

Extender semántica y lowering para reconocer explícitamente excepciones extranjeras y conservar su operación de inspección/relanzamiento, o exponer un modelo de error interoperable que el usuario pueda nombrar. Cambios candidatos: filtro y tipos de payload NS, HIR/MIR y limpieza, metadata versionada, provider Clang y generación de catch/throw. Debe definirse qué excepciones se mapean a qué tipos, qué ocurre con capturas por base y cómo preservar compatibilidad de programas NS existentes. La autorización del usuario permite modificar la rama; no autoriza afirmar que esta alternativa ya sea suficiente o modificar el lenguaje sin un diseño trazable.

La elección puede combinar las alternativas por operación: propagación, captura y traducción no son la misma capacidad. Registrar qué conserva cada ruta y cuáles escenarios necesitan otro mecanismo. Una implementación que convierte todo a mensaje/estado o termina silenciosamente recorta el alcance.

## Ownership y construcción

Para cada objeto que cruza se debe determinar: identidad de tipo dinámico, representación de valor o referencia, quién asigna, quién destruye, quién libera, si la operación es transferida o prestada, durabilidad y módulo que ejecuta el deallocator. Para `MyClass m` y `new MyClass`, diseñar constructor/destructor C++ y copia/move sin confundir el almacenamiento de una proyección con el del objeto NS. Las entradas de destrucción de NS que liberan y las que solo destruyen no son intercambiables; un custom allocator requiere el descriptor efectivo de la instancia.

## Arranque, terminación y estado por módulo

**Evidencia:** `src/Linker/LldLinker.cpp:1209-1212` fija para la ruta examinada del ejecutable NS `/nodefaultlib` y `/entry:__nativesharp_start`. `src/Linker/WindowsExecutableBuilder.cpp:317-328` selecciona el runtime completo o el módulo de entrada por separado; `src/Runtime/WindowsRuntimeBuilder.cpp:6352-6390` construye la entrada. La implementación `buildStart` (`:261-309`) inicializa el soporte NS de excepciones, invoca el main NS y, si regresa, llama directamente a `ExitProcess`; sus bloques de error también salen por `ExitProcess` (`:223-257`). No se observa en esa función una llamada a arranque o terminación del CRT C++. Esto describe ese camino de código, no demuestra el comportamiento de todos los mecanismos posibles del enlazador o del cargador. La inicialización estática NS tiene sus propios inicializadores (`src/LLVM/Lowering/OopLlvmStaticState.cpp:597-606`).

En el lado C++, `clang/lib/CodeGen/CGDeclCXX.cpp:561-632` emite funciones de inicialización global y entradas de constructores; `MicrosoftCXXABI.cpp:2426-2441` registra por defecto destructores globales mediante `atexit` y usa otra ruta para TLS, mientras `:2560-2568` describe inicialización TLS dinámica bajo demanda. Estas rutas muestran dependencias de ciclo de vida que la integración debe preservar; no demuestran por sí solas cómo se enlazará o arrancará cada ejecutable mixto.

**Consecuencia de diseño:** para un EXE cuyo programa principal es NS, estudiar una entrada mixta que respete los contratos de arranque y terminación C++ y después invoque el programa NS, o una integración equivalente demostrada. No limitarse a llamar inicializadores a mano ni a enlazar `/MT`: el CRT puede tener obligaciones adicionales. Para un EXE cuyo principal es C++, identificar exactamente qué inicialización NS se requiere antes de usar la biblioteca y qué responsabilidades ya son perezosas. Definir quién termina cada subsistema y en qué orden, incluyendo fallos. Las DLL requieren ciclo de carga/descarga propio, TLS, objetos aún vivos y reglas de loader lock; no introducir trabajo inseguro en `DllMain`. Mantener la entrada NS actual para programas no mixtos.

El runtime NS ya reconoce un caso real de estado separado por módulo: `WindowsRuntimeBuilder.cpp:6216-6247` explica que una DLL y su consumidor pueden tener TLS e índices distintos, y publica en el TLS local el payload NS recibido por el registro SEH. Es un mecanismo específico de excepciones NS, no una prueba de que el resto del runtime o las excepciones C++ compartan estado correctamente. Registrar explícitamente quién posee registros, objetos, allocators y CRT por imagen antes de permitir transferir ownership entre DLL.

**Validación futura:** identificar `AddressOfEntryPoint` y símbolo con cabeceras PE y mapa del enlace; usar contadores o Win32 `WriteFile` para observar inicialización sin depender de un CRT aún no inicializado; comprobar constructores, destructores, `atexit` y TLS en EXE NS con C++, EXE C++ con NS y DLL, tanto con enlace estático como dinámico cuando aplique. Probar carga, descarga, fallo y reentrada; mantener todos los artefactos en A:. No se han ejecutado estas pruebas.

## Verificación requerida al implementar

Definir por contrato pruebas con EXE NS + biblioteca C++ y EXE C++ + biblioteca NS, cada una en modo estático y DLL: captura en origen tras atravesar el otro lenguaje; cleanup de objetos con contadores; excepción en constructor; rethrow; captura por base y catch-all; `noexcept`/termination con proceso aislado; callback anidado; TLS entre módulos y descarga de DLL; `owned` y custom allocator con destrucción/liberación exactamente una vez. Añadir controles equivalentes en cada lenguaje sin interoperabilidad y verificar optimizaciones y compilación separada. La salida esperada debe provenir de los contratos y observaciones, no del propio adaptador sometido a prueba.

## Pendiente de decisión

Hay evidencia para identificar las rutas y las opciones anteriores, pero durante la implementación hay que contrastar las reglas de tipos y manejo de errores de NS, los casos de catch/cleanup específicos del backend C++ y los contratos de exportación DLL antes de fijar el protocolo por operación. La elaboración del plan no equivale a resolver esas decisiones ni a validar el comportamiento mixto.
