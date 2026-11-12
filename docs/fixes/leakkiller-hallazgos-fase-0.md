# LeakKiller — limitaciones y puntos por corroborar en fase 0

Fecha: 2026-11-09. Documento vivo, en curso. Origen: revisión estática de LeakKiller; detalles y cursor en A:/nsc-leak-killer/docs/fase-0/EVIDENCIA.md y ESTADO.md. El usuario solicitó registrar aquí todas las limitaciones identificadas. La revisión no ha terminado.

Esta copia de NativeSharp estaba en development-fixes-limitaciones, HEAD 581d0bb74e60abb6d1fd2ac8eee3f4e7388bde6d, con cambios previos y concurrentes. Los ejemplos examinados proceden de A:/nsc-debugger, revisión 007ea6809d705b998c9507cb39ee7ba4ac02d4b9: **no son prueba de que esta rama presente o conserve un defecto**. No se compiló ni ejecutó nada para este informe. Para defectos ya reproducidos y cambios en curso, consultar campaign-status.md y las fichas existentes; no se altera su estado aquí.

## Hallazgos confirmados por lectura de las sondas

- **LKF-001 — Sonda de eventos no ejercita la operación:** tests/corpus/documentation/the-complete-reference/ejemplos/t10/capitulo-043-eventos/sondas/RemoveNextDuringInvoke/Probe.ns registra dos handlers, pero Main no llama Raise. No demuestra comportamiento al eliminar el siguiente handler durante el recorrido. Pendiente: prueba posterior que dispare el evento, verifique orden, vida y reentrada, y cubra excepciones. Evidencia NSDBG-1085–1086; tracking LeakKiller LK-273.
- **LKF-002 — Sonda de copia de arreglos no ejecuta Wire:** tests/corpus/documentation/the-complete-reference/ejemplos/t11/capitulo-045-arrays-inline/sondas/OwningFixedArrayCopyAccepted/Probe.ns contiene la copia tras move dentro de Wire, pero Main no la llama. Puede servir para estudiar aceptación por compilación, no prueba destrucción o alias en ejecución. Pendiente: controles que llamen Wire y observen propiedad y limpieza. Evidencia NSDBG-1159–1160; LK-275.

## Candidatos pendientes de corroborar en esta rama

Los siguientes son riesgos o límites de cobertura, no fallos confirmados. Antes de abrir una corrección, comprobar semántica deseada, código actual, versión de compilador y una reproducción aislada; en esta fase sólo se documentan.

- **LKF-003 — Delegados y vida de receptor:** las sondas EscapingInstanceReceiverAccepted, MovedDelegateRuntimeFailure, NullInvocationTrap y PassModeMismatchAccepted plantean escape de receptor owned, move, invocación nula y compatibilidad ref/in. Los nombres no constituyen resultado. Referencias: NSDBG-1021–1028; LK-272.
- **LKF-004 — Límites de arreglos:** NegativeCount, NegativeIndex, NullLength, UpperBound, RuntimeUpperBound y RuntimeSecondDimension son entradas para comprobar tamaño negativo, null e índices fuera de rango en arrays dinámicos y fijos. Pendiente distinguir diagnóstico estático, protección dinámica y ausencia de comprobación; añadir overflow de dimensiones. NSDBG-1117–1128 y 1161–1164; LK-274.
- **LKF-005 — Propiedad en valores y colecciones:** verificar copias de structs y arreglos con campos owned; extracción y sustitución de elementos; getters/setters e indexadores owned; destructor y rutas excepcionales. Los ejemplos y casos negativos no bastan para inferir la semántica actual. NSDBG-577–588, 765–770, 1109–1116 y 1158–1160; LK-270, LK-271 y LK-275.

- **LKF-006 — Referencias, vistas y closures:** corroborar escape de ref local, referencia a elemento tras delete[], capturas borrowed/owned/raii y VLA; comparar análisis de cuerpo, lowering y ejecución antes de atribuir una fuga o uso posterior a liberación. NSDBG-514, 545, 1031–1042, 1165–1170; LK-240 y LK-272.
- **LKF-007 — Limpieza y despacho polimórfico:** contrastar destructores al eliminar por interfaz o base, return/break/continue, excepciones y finally, y cleanup de enumeradores. Los ejemplos expresan expectativas; no son una validación de esta rama. NSDBG-380, 408, 714, 750; LK-269.
- **LKF-008 — Conversión, tipos y control de flujo:** confirmar límites numéricos, conversiones peligrosas, promoción, overflow, división por cero y terminación; tratamiento de operadores sobrecargados, alias genéricos y argumentos con efectos. No inferir semántica del compilador a partir de una prueba negativa. NSDBG-112–141, 217–262, 292, 982–996; LK-267–268.

- **LKF-012 — VLA y extremos de pila (pendiente):** las sondas NegativeExtent y OversizeExtent declaran longitudes -1 y 262145, y el corpus contiene escape mediante retorno/captura. Contrastar límites, overflow, limpieza y diagnósticos en la rama actual; no se ejecutaron ni demuestran defecto. Evidencia NSDBG-1165–1191; LK-274.

- **LKF-013 — Sonda de lectura VLA no invocada:** UninitializedReadAccepted/Probe.ns contiene Read con lectura no inicializada, pero Main retorna 0 sin llamar Read. El nombre no prueba la seguridad ni el resultado en ejecución. Diseñar reproducción que alcance la lectura; NSDBG-1194–1195. La sonda ProductOverflow (NSDBG-1192–1193) requiere ejecución posterior y no demuestra por sí sola el resultado.

- **LKF-014 — Ejemplo de miembro flexible y precondición insuficiente:** en FlexibleTailTour/Program.ns, VerifyExternalExtent acepta count desde 0 hasta el máximo pero accede alias[2]; su único llamador visible pasa 3. Riesgo condicional al invocar con 0–2, sin ejecución ni afirmación sobre compilador. Sum además confía en Count sin capacidad de reserva asociada. Analizar y reproducir antes de corregir; NSDBG-1219–1220.

- **LKF-015 — Forma de structs con cola flexible (pendiente):** las sondas PrefixValueAccepted y SoleTailAccepted plantean paso por valor del prefijo y struct que sólo contiene miembro flexible. Sus nombres no prueban aceptación; decidir contrato y comprobar layout/ABI del objetivo antes de clasificarlas como defecto (NSDBG-1221–1224).

- **LKF-016 — Strings y vistas (pendiente):** sondas de índice negativo, Length sobre null y owned local; casos de escape de StringView y borrowed. Confirmar protección, duración y semántica de bytes UTF-8/NUL con compilador y SDK actuales; OwnedLocalAccepted no contiene Main y su nombre no prueba ejecución (NSDBG-1237–1251).

- **LKF-017 — Sondas de punteros sin ejecución (pendiente):** MisalignedCastAccepted, NullDereferenceAccepted, ReturnLocalPointerAccepted y ReturnStackallocAccepted definen funciones peligrosas sin Main; no prueban aceptación en la rama actual ni resultados de ejecución. Confirmar alineación base, escape de pila y controles alcanzables; NSDBG-1281–1288.

- **LKF-018 — Movimiento repetido de campo y reactivación (pendiente):** sondas SecondFieldMoveAccepted, ReassignMovedWithNew y ReassignMovedWithOwnedCall contienen funciones sin Main; sus nombres no acreditan aceptación ni ejecución. Verificar si el segundo move encuentra null o genera diagnóstico, y cómo se reactiva el slot con new o retorno owned; NSDBG-1343–1348.

- **LKF-019 — Receptor de hilo desprendido (pendiente):** DetachedReceiverUnchecked crea un delegado hacia un receptor raii, inicia un hilo, llama Detach y sale del ámbito sin Join. La vida del receptor y del delegado depende del contrato real del runtime y del orden de ejecución; riesgo potencial, no reproducido ni prueba de defecto actual (NSDBG-1437–1438). Contrastar con los casos unidos y políticas de concurrencia.

- **LKF-020 — Liberaciones incompatibles, duplicadas y fuga (pendiente):** sondas hacen delete sobre arena o stackalloc, dos deletes, asignación sin delete y tamaños negativos/desbordados; ninguna fue compilada ni ejecutada aquí. DeleteBorrowedAccepted define Release pero Main no la llama. NSDBG-2304, líneas 528–541, informa históricamente que una sonda equivalente pasó check y build, sin invocar el método: no confirma ni un error de ejecución ni el comportamiento de la rama actual. En código actual, `src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp:1167` retorna de consumeDeletedOwnedParameter sin consumir parámetros que no sean Owned; se requiere seguir analyzeExpression y demás fases antes de concluir si un borrowed se rechaza. Verificar contratos, diagnósticos y fallos con controles aislados; no considerar Accepted/Trap como resultados (NSDBG-1463–1476).

- **LKF-021 — Delete escalar y uso tras liberar (pendiente):** ScalarDeleteDynamicArrayAccepted usa delete sobre arreglo; UseAfterDeleteAccepted invoca método tras delete. Comprobar compilador y runtime actuales, variaciones de alias y controles con delete[]; los nombres no prueban aceptación, y no se ejecutaron (NSDBG-1477–1480).

- **LKF-022 — Stackalloc en bucle y límites dinámicos (pendiente):** sondas prueban en código count negativo, grande, cero y stackalloc por iteración. Ninguna fue ejecutada; verificar si el compilador diagnostica y si el runtime controla tamaño/overflow, acumulación de pila y escape. No inferir trap ni warning por el nombre (NSDBG-1498–1506).

- **LKF-023 — Vida de placement y almacenamiento externo (pendiente):** sondas plantean delete de objeto sobre pila, doble destroy, construcción sobre null, superposición sin destroy y almacenamiento de un byte para Item. Aclarar reglas de vida/reuso, capacidad y alineación y reproducir controles en rama actual; no se han ejecutado ni se asume resultado por sus nombres (NSDBG-1527–1536).

- **LKF-024 — Promoción a pila y noheap (pendiente):** el corpus distingue new promovible, identidad observable, llamadas y constructores; las sondas ConstantStringAccepted y NormalHeapFallback no prueban por su nombre aceptación ni ubicación final. Contrastar IR y binario de esta rama y considerar que las reservas promovidas pueden no aparecer en hooks de heap (NSDBG-1537–1578).

- **LKF-025 — Asignación parcial de out (pendiente):** OutNotAssigned asigna el parámetro sólo en la rama true y observa el valor previo tras false; contrastar si se diagnostica y cuál es el contrato en la rama actual. No se compiló ni ejecutó esta sonda (NSDBG-1621–1622). NSDBG-2334:126–141 y 597–608 atribuye al entorno histórico compilación y resultado de esa ruta; la fuente completa Program.ns SHA-256 069593d8085f27d7146a4ab9af0e9c2debd556750682b2338623061496c11c64 inicializa value a 9 y Main llama TryGet(false), cuyo camino false no escribe. No se ha comprobado el resultado en esta revisión. **Contraste parcial actual:** MethodBodyBuildCoordinator.cpp:904 invoca verifyOutParameterAssignments; MethodBodyCallableLifecycle.cpp:350–383 verifica pertenencia a assignedOutParameters_ y señala que no distingue caminos. AssignmentAnalyzer.cpp:443–476 registra la raíz también al escribir un campo, índice o desreferencia. Esto caracteriza esa comprobación concreta; todavía debe revisarse si otros análisis agregan garantías por ruta o campo. LK-370.
  - **Contraste adicional de flujo:** BodyScopeAndFlow.cpp:116–184 y :323–354, SHA-256 4add78e812b0816fc0a8b9c3fb7c37d16da02f053e779f8312a991ac2865440d, captura y une varios estados de propiedad y asignación local, pero assignedOutParameters no aparece entre los campos capturados por esa función. Verificar otros caminos y representación antes de concluir el alcance completo; sin ejecución. LK-550.

- **LKF-026 — Fronteras de excepción y terminación (pendiente):** sondas AbruptExit, ThrowingDestructor, UncaughtThread y UnsafeCExport plantean cleanup omitido, excepción en destructor, fallo de hilo y propagación por ABI C. Aclarar contratos y observar terminación/registro y cleanup en pruebas posteriores; aquí no se ejecutaron (NSDBG-1635–1641).

- **LKF-027 — Carrera de datos y vida de receptor (pendiente):** PlainDataRace incrementa campo ordinario desde dos hilos sin sincronización entre ellos; DetachedReceiverLifetime libera receptor tras Detach. Son entradas de riesgo para análisis, sin ejecución ni afirmación de detección del compilador. Contrastar contratos de Thread, cierre y sincronización en rama actual (NSDBG-1651–1654).

- **LKF-028 — AtomicPointer no conserva vida del objeto (por confirmar contrato):** sondas almacenan dirección de pila retornada o de arreglo ya liberado. No desreferencian esas direcciones, por lo que no demuestran una falla de acceso. Verificar reglas de propiedad/publicación y diseñar controles de vida, acceso y ABA sin confundir atomicidad del puntero con propiedad del destino (NSDBG-1672–1676). NSC-043 Atomics.ns:255–305 declara la ubicación de direcciones como no propietaria y no contiene destructor explícito de pointee; los métodos son marcadores de operaciones núcleo. Esto apoya la intención del contrato en fuente, sin probar por sí solo lowering o toda ruta de cleanup. NSDBG-2337, LK-375.

- **LKF-029 — Interbloqueo potencial (pendiente):** AbbaDeadlock toma mutex en órdenes opuestos; MonitorReentrantDeadlock entra dos veces en un monitor. El resultado depende de planificación y reentrancia real, no se ejecutó. Contrastar contrato y diseñar observación acotada antes de diagnosticar (NSDBG-1726–1731).

### LKF-030 — Comprobación duplicada en ejemplo de layout

En `nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t16/capitulo-068-layout-explicito/LayoutExplicit/Program.ns`, las líneas 66 y 67 repiten la misma comprobación de Alias. Esa repetición por sí sola no cubre una segunda condición; revisar si falta una comprobación distinta. Es una observación estática del ejemplo, no un defecto demostrado del compilador. No se ejecutó. Evidencia: NSDBG-1768.

### LKF-031 — Cobertura pendiente de layout de campos de bits y agregados anónimos

Los contratos revisados incluyen campos solapados, promoción anónima, propiedad en uniones como caso negativo y campos de bits con signos, barrera de ancho cero, empaquetado y volatile. No se ha validado que el compilador, los símbolos, el depurador y LeakKiller conserven la misma representación en cada combinación; la lectura aislada no demuestra un fallo. Requiere casos positivos y negativos por ABI y arquitectura y comparación de metadatos. Evidencia: NSDBG-1765–1791.

### LKF-032 — Precondiciones de puntero y callback en frontera ABI

La API de ejemplo `capitulo-071-abi-windows-x64/AbiContract/Api.ns` desreferencia `value[0]` en AddThroughPointer e invoca callback en Apply sin comprobaciones locales de nulidad o vigencia. La seguridad depende de los contratos y llamadores; la revisión no demuestra que los usos existentes sean inválidos. Evaluar contratos de entrada, procedencia y vida a través de DLL y llamadas externas, sin asumir que la ausencia de comprobación sea por sí misma un defecto del compilador. Evidencia: NSDBG-1818.

### LKF-033 — Correspondencia entre artefactos y versiones sin validar

El laboratorio de versionado contempla metadata corrupta o mezclada y cambios incompatibles de layout y firmas. No se ejecutó su verificación ni se comprobó que el depurador o LeakKiller detecten desajustes entre ejecutable, DLL, PDB y metadata. Debe exigirse identidad verificable de los artefactos, declarar la incertidumbre cuando falte y probar versiones aditivas e incompatibles. La existencia de escenarios no demuestra una falla del compilador. Evidencia: NSDBG-1840–1852.

### LKF-034 — Vida del contexto del hilo en una rama de error de espera

En `capitulo-074-delegates-c-y-punteros-de-funcion/Program.ns`, Main pasa al hilo la dirección de la variable local threadInput; si la espera no devuelve cero, cierra el handle y retorna sin demostrar que el hilo terminó. Si en esa rama el hilo sigue ejecutándose, puede acceder a un contexto cuya vida terminó. Es un riesgo condicionado por el resultado de la API, no una falla de compilación demostrada; no se ejecutó. Investigar sincronización y caminos de error, y utilizar el caso para análisis de escapes y vida entre hilos. Evidencia: NSDBG-1879, líneas 103–110.

### LKF-035 — Buffer de proveedor en salida temprana y puntero retenido

En `capitulo-076-strings-y-buffers-en-interop/Program.ns`, VerifyProviderOwnedBuffer retorna cuando environment es no nulo y su primer elemento es cero sin llamar a FreeEnvironmentStringsW; verificar si ese resultado es posible bajo el contrato de la API y considerar una fuga condicional, no observada. La sonda DanglingRetainedPointer registra una dirección perteneciente a CString local que después termina, pero no demuestra una desreferencia posterior. Son hallazgos del ejemplo/sonda, no defectos demostrados del compilador. Evidencia: NSDBG-1900 y NSDBG-1903.

### LKF-036 — Wrapper variádico sin capacidad explícita

FormatTwoIntegers recibe un puntero de salida sin capacidad y lo pasa a wsprintfA; el llamador revisado proporciona 64 bytes, pero la firma por sí misma no garantiza tamaño para otros llamadores. Revisar contratos y alternativas con límite. Sin ejecución ni fallo observado. Evidencia: NSDBG-1919.

### LKF-037 — Concurrencia, reentrada y descarga de plugin pendientes

El ejemplo PluginSdk guarda liveSessions y shuttingDown y campos de sesión sin sincronización visible, retiene un callback/contexto y llama al callback desde Add. Sus contratos de acceso concurrente, reentrada, vida del contexto y descarga requieren verificación; no se ha demostrado una carrera o uso posterior a descarga en los hosts. Documentar restricciones y modelar los límites de instrumentación de DLL. Evidencia: NSDBG-1922–1924.

### LKF-038 — Extensiones y aritmética de índices en buffers SIMD

NS80_AddBuffer recibe tres punteros y una longitud sin verificar localmente que los buffers tengan dicha extensión; su bucle usa index + 4 con enteros signados. La corrección para entradas extremas, overflow y punteros solapados o inválidos depende del contrato y semántica de aritmética. Investigar y definir precondiciones; el ejemplo no demuestra fallo con sus entradas. La sonda de carga alineada desplazada tampoco demuestra por sí sola la alineación efectiva ni un fallo en ejecución. Evidencia: NSDBG-1969 y NSDBG-1974.

### LKF-039 — Inlining y ubicación de variables sin validación cruzada

La sonda InlineDebug contiene funciones inline/forceinline/noinline y variables locales; el README describe certificación CodeView/PDB. Aquí solo se revisó el código: no se comprobó que el depurador reconstruya marcos inline, ubicación o disponibilidad de locales, especialmente con optimización o entre módulos. LeakKiller debe expresar información ausente y evitar presentar como completa la secuencia de llamadas. Evidencia: NSDBG-1995–2003.

### LKF-040 — Alcance real del proyecto y fuentes excluidas

ProjectTour.nsproj combina globs, Exclude, Remove y ProjectReference; Ignored.ns es deliberadamente inválido y está excluido. Un análisis por búsqueda indiscriminada de archivos puede producir diagnósticos de fuentes ajenas a la compilación o perder configuración. Queda por verificar y reutilizar la resolución efectiva de proyectos y sus variaciones; no se ha identificado un defecto del compilador. Evidencia: NSDBG-2014–2015.

### LKF-041 — Diagnósticos del compilador y reporte LeakKiller sin correlación validada

El corpus incluye diagnósticos léxicos, sintácticos, de tipos, propiedad, vida, ABI y enlace, además de salida humana y JSON descritas en el README. No se comprobaron formato, estabilidad, recuperación ni integración con LeakKiller; algunos casos son deliberadamente inválidos y la sonda de referencia escapada ni se invoca en Main. Definir deduplicación, procedencia, cascadas y comportamiento ante proyectos que no compilan; validar antes de prometer sugerencias o soluciones. Evidencia: NSDBG-2099–2118.

### LKF-042 — Rango decimal y estado ignorado en ejemplo de arena

En `capitulo-091-diseno-de-apis-nativesharp/ContractApp/Program.ns`, WriteDecimalInto acepta enteros no negativos pero calcula como máximo tres caracteres: con value 1000 escribe 58 como primer byte en lugar de un dígito decimal. TryParseDecimal acumula sin comprobar desbordamiento; su efecto depende de la semántica de enteros. FormatInArena ignora el estado de WriteDecimalInto, incluso para valor negativo o asignación fallida. Los ejemplos ejercitan 421 y 73, no estas condiciones; son observaciones estáticas del ejemplo, sin ejecución ni fallo atribuido al compilador. Evidencia: NSDBG-2136, líneas 64–131.

### LKF-043 — Posible uso tras liberar en salida de error del pool

En `capitulo-092-patrones-de-recursos/Program.ns`, VerifyBufferArenaAndPool elimina pool dentro del alcance de un PoolLease RAII si falla una aserción (línea 290). Si lease está activo y la segunda condición causa la salida, su destructor posterior llama source.Release() sobre pool ya eliminado. Esa rama requiere una condición anómala del ejemplo y no se ha observado en ejecución; revisar orden de destrucción y modelar RAII en salidas tempranas. Evidencia: NSDBG-2138, líneas 110–127 y 287–293.

### LKF-044 — Capacidad y ciclos en colecciones del ejemplo

En `capitulo-093-patrones-de-colecciones/Program.ns`, StableTable.Insert elige slot 1 si el 0 está ocupado, sin comprobar si el 1 también lo está: una tercera inserción consecutiva reemplaza el valor de ese slot sin incrementar generación. IntrusiveList.PushFront puede formar un ciclo si se inserta otra vez el mismo nodo; Sum no tiene detección de ciclos y podría no terminar. View/Take y crecimiento dependen de índices y tamaños válidos. Son límites estáticos del ejemplo; los usos revisados no ejercitan esas condiciones y no se ejecutaron. Evidencia: NSDBG-2142, líneas 24–55, 161–180 y 195–225.

### LKF-045 — Reclamación y etiquetas ABA del ejemplo no generalizadas

ReclamationDomain.TryReclaim solo consulta retired y activeReaders; Enter no impide lectores nuevos después de que se observa cero. La prueba espera a que el lector termine antes de retirar, por lo que no demuestra un protocolo seguro de reclamación concurrente. TaggedHead codifica generation * 1000 + slot sin restringir slot: (slot 1000, generación 0) y (slot 0, generación 1) colisionan. Tratar ambos como limitaciones del ejemplo y condiciones para investigar, sin afirmar fallo observado en ejecución ni defecto del compilador. Evidencia: NSDBG-2145, líneas 158–173, 195–213 y 313–332.

### LKF-046 — Reentrada y salida parcial en callback de interop

NS95_Transform asigna written=count antes del bucle; si el callback rechaza en una iteración, retorna con una salida potencialmente parcial y written todavía igual a count. La semántica pretendida de written debe aclararse. Además, si se permite que el callback destruya la misma sesión, el código vuelve a acceder a session después de la llamada, con riesgo condicionado de uso tras liberar. Falta comprobar contratos del host y reproducibilidad; no se observó ejecución ni se atribuye el problema al compilador. Evidencia: NSDBG-2147, líneas 70–76 y 88–120.

### LKF-047 — Recursos de hilo liberados antes de Join en error de espera

VerifySynchronization en `capitulo-096-anti-patrones/GoodPatterns/Program.ns` elimina worker y completed si Wait(-1) devuelve false, antes de llamar a Join. Si el hilo aún puede acceder a esos objetos en esa rama, existe riesgo condicionado de uso tras liberar. Verificar la semántica real de Wait y del destructor de Thread, y asegurar terminación antes de liberar. Revisión estática sin ejecución; no es una falla demostrada del compilador. Evidencia: NSDBG-2157, líneas 63–72 y 115–125.

### LKF-048 — Limpieza omitida en salidas de error del corpus de conversiones

En `capitulo-100-tabla-de-conversiones/ConversionCorpus/Program.ns`, derived y number se crean antes de las comprobaciones de las líneas 126–157, pero sus delete aparecen solo al final. Si alguna comprobación falla, el retorno anticipado no ejecuta esas liberaciones explícitas. Es un riesgo condicionado de limpieza en el ejemplo; falta verificar el comportamiento real de la terminación de proceso y no se atribuye al compilador. Evidencia: NSDBG-2197.

### LKF-049 — Soporte de partial struct y genéricos en referencia histórica

El capítulo 9 de The Complete NativeSharp Reference afirma que su distribución NativeSharp 2.0 rechaza partial struct y tipos genéricos parciales. Es evidencia documental asociada a esa versión; no se ha comprobado si sigue vigente en la rama actual ni se ejecutaron casos. Revisar parser y semántica actuales, y precisar efecto sobre resolución interarchivo, generadores y LeakKiller antes de abrir cualquier corrección. Evidencia: NSDBG-2279, líneas 70 y 187–208.


**Contraste NSC-429:** A:/nsc-devel/src/Frontend/Generics/TemplateCollector.cpp:198–215, SHA-256 6cefbca6b2ed799f6e3e9670dbc65c43cb8734b2deea23a2fbb19de73b1e5339, emite ExpectedDeclaration cuando el nodo de tipo genérico tiene flag de partial. Esto confirma el diagnóstico en esta ruta fuente; falta verificar parser, todos los tipos, rutas alternas y comportamiento ejecutado. No extiende la comprobación a partial struct no genérico. LK-625.

### LKF-050 — Cast floating a entero fuera de rango: divergencia histórica

El capítulo 12 de la referencia relata que, en su distribución NativeSharp 2.0, convertir dinámicamente 1.0e300 a int produjo resultados distintos entre O0 y O2 y que el guard de la ruta de archivo único no estaba integrado en el pipeline de proyecto. También afirma que opciones FP de lowering no estaban disponibles en build. Son resultados históricos del documento, no reproducidos ni verificados en la rama actual; investigar fuente y versión, incluir NaN, infinito, límites representables y ambas optimizaciones. Evitar sugerir un cast como validación de rango. Evidencia: NSDBG-2282, líneas 321–336, 417–446 y 684–708.

### LKF-051 — Constantes de enum ausentes en header histórico

El capítulo 14 describe que su generador NativeSharp 2.0 emite el typedef C del tipo subyacente y firmas, pero no constantes de enumeradores, por lo que los números requieren otra autoridad compartida. Es una limitación documental de esa distribución, no una verificación de la rama actual; contrastar código generador y artefactos antes de decidir corrección. Los casts desde enteros tampoco prueban que el valor pertenezca al conjunto nombrado. Evidencia: NSDBG-2284, líneas 323–345 y 660–679.

**Contraste actual parcial:** CHeaderGenerator.cpp:943–975 recorre enums alcanzables, obtiene su tipo subyacente mediante hostType y emite un typedef; esa función no emite nombres ni valores de enumeradores. El capítulo NSDBG-2349:211–221 describe la misma limitación en otro SDK histórico. Falta revisar el resto de las rutas de generación y el contenido de un artefacto de la versión vigente; no se generó ni ejecutó ninguno. Para LeakKiller, no adivinar el significado de status o éxito a partir del carrier; preservar contratos y valores de origen verificado. LK-390.

### LKF-052 — Asignación de out por rutas: evidencia histórica adicional

El capítulo 15 informa que su compilador NativeSharp 2.0 aceptaba una asignación de out en una sola rama y que el probe con la rama falsa observó cero en O0/O2. Esto complementa LKF-025: es una medición histórica de la guía, no una reproducción actual. Verificar el checker y los retornos normales/excepcionales de la rama actual antes de clasificar el defecto, y diseñar análisis por rutas sin confundir default físico con asignación lógica. Evidencia: NSDBG-2285, líneas 261–277 y 776–797.

### LKF-053 — Fronteras históricas de evaluadores constantes

El capítulo 16 informa que, en su distribución 2.0, static_assert no plegaba referencias a campos const, enum convertido o default, y que el evaluador de campos const no aceptaba sizeof. También distingue una local const que puede inicializarse en runtime de un campo const evaluado en compilación. Son observaciones históricas no reproducidas en esta fase; verificar la implementación vigente y no interpretar const local como prueba de valor constante ni ejecutar efectos dentro de sizeof/alignof. Evidencia: NSDBG-2286, líneas 252–296 y 607–635.

### LKF-054 — Limpieza omitida en ejemplo de conversiones de la guía

El programa reproducido en el capítulo 17 crea derived y number antes de varias comprobaciones que retornan con error; los deletes aparecen al final, por lo que esas rutas omiten la liberación explícita. Es un hallazgo condicionado del ejemplo, análogo a LKF-048, sin ejecución ni defecto atribuido al compilador. Verificar cleanup efectivo y la fuente original del ejemplo antes de modificarla. Evidencia: NSDBG-2287, líneas 904–947.

### LKF-055 — Array manual sin liberar en retorno temprano del ejemplo de expresiones

En el programa incluido en el capítulo 18, heap se asigna antes del bloque RAII y solo se libera después; si second.Value != 10, return 1 sale antes de delete[] heap. Es una ruta de error condicionada en el ejemplo, no observada ni atribuida al compilador. Comprobar la fuente real correspondiente, reglas de cleanup y alternativa de ownership antes de modificar. Evidencia: NSDBG-2288, líneas 604–637.

### LKF-056 — Discrepancias documentales sobre shifts y evaluación de propiedades

El capítulo 19 afirma que un shift constante fuera de rango se rechaza (líneas 287–291), mientras el capítulo 11 describe un shift constante en cuerpo ordinario aceptado y atrapado en runtime (NSDBG-2281, líneas 384–394). Además, capítulo 18 describe temporales para evaluación única de propiedad compuesta, y capítulo 19 limita receptores e índices a formas reevaluables. Pueden referirse a contextos o versiones distintas: cotejar fuente y corpus actuales antes de decidir si existe contradicción o actualizar documentación. No se ejecutó ni demuestra fallo del compilador. Evidencia: NSDBG-2288 y NSDBG-2289.

### LKF-057 — Iteración de colección mutable y soporte histórico de spans

La documentación histórica revisada como NSDBG-2291, líneas 284–318, afirma que el enumerador de List<T> consulta Count vivo y que Span/ReadOnlySpan carecen de GetEnumerator. Una colección que crece durante foreach podría no terminar. El código actual `A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns:34` consulta source.Count() en cada avance y `List.ns:48` devuelve count; esto confirma estáticamente el límite vivo. El compilador también tiene mecanismos de préstamo para foreach ref/ref-mut en `include/nativesharp/Frontend/Internal/MethodBody/ActiveIterationBorrow.h:3`; falta determinar qué mutaciones concretas admite o rechaza y validar casos en ejecución. La afirmación histórica sobre spans permanece pendiente de contraste. Evaluar diagnóstico de crecimiento, mutaciones indirectas y ciclos deliberadamente abiertos, además del soporte real de foreach en spans.

### LKF-058 — Retorno ausente en una ruta de método no void (antecedente histórico)

NSDBG-2293, líneas 222–232 y 654–679, informa que nsc 2.0.0+build.20260924T232419Z.fb554be63855016a aceptaba un camino alcanzable sin retorno y documenta O0 0x80000003 frente a O2 código 1. Es un resultado atribuido a documentación histórica: no se ha reproducido ni verificado en la rama vigente. El código actual contiene un chequeo NotAllPathsReturn en `src/Sema/Semantic/Declarations.cpp:468`, pero aún no se ha demostrado si ese camino procesa los métodos de clase del caso histórico. Contrastar frontend y lowering actuales; planear casos de rutas alcanzables, excepciones, bucles, finally, funciones no retornantes y optimizaciones, sin confundir ausencia de retorno con salida excepcional o no terminación.

### LKF-059 — Posible overflow en aritmética de arena

`A:/nsc-devel/lib/NativeSharp.Core/ArenaAllocator.ns:124` valida bytes + alineacion antes de redondear bytes a múltiplo de ocho; las sumas posteriores usan int. Por ejemplo bytes=2147483639 y alineación=8 pasan la comprobación, pero pedido+alineacion sería 2147483648. Además, usado+relleno+pedido puede superar el rango. Hallazgo de inspección estática: determinar semántica de overflow, si la ruta es alcanzable y qué pasa con asignación, bounds y excepciones; no se ejecutó ni se afirma corrupción.

### LKF-060 — Contador agregado de arena no identifica liberaciones

`A:/nsc-devel/lib/NativeSharp.Core/ArenaAllocator.ns:215` decrementa asignacionesActivas para cualquier ptr no nulo mientras el contador sea positivo, sin comprobar identidad, procedencia ni liberación previa; llamadas incorrectas pueden ocultar obligaciones distintas. El contrato exige emparejamiento válido, por lo que esto describe una limitación diagnóstica ante mal uso, no prueba de fallo de un llamador. Diseñar seguimiento opcional por identidad y generación, contemplando Resize, Reset y retención física; no prometer detección si la instrumentación no cubre el sitio.

### LKF-061 — Ambigüedad histórica de overloads ref/out/in

NSDBG-2295, líneas 258–270, informa que nsc 2.0 instalado clasificaba por el mismo carrier y consideraba ambigua una llamada entre sobrecargas que difieren sólo en ref/out/in del mismo tipo. Se trata de documentación histórica, sin reproducción ni comprobación de la rama actual. Investigar matching por modo, diagnóstico y casos positivos/negativos antes de proponer corrección.

### LKF-062 — Colisión histórica de overloads owned/borrowed al emitir LLVM

NSDBG-2295, líneas 297–307, informa que el check semántico aceptaba sobrecargas diferenciadas sólo por ownership, pero el build O0 fallaba por nombre LLVM compartido y múltiples attachments !dbg. Es evidencia documental de una distribución histórica, no validación actual. Contrastar identidad semántica, mangling, lowering y metadata de depuración; planear casos con y sin debug y consumidores binarios.

### LKF-063 — Alias de elemento usado tras delete[] (antecedente histórico)

NSDBG-2296, líneas 396–407 y 621–632, presenta un alias ref a un elemento de array, delete[] y lectura posterior; informa check y build O0 exitosos con nsc 2.0 histórico, y omite deliberadamente la ejecución. Esto indica un hueco diagnóstico documentado en esa distribución, no una reproducción ni prueba sobre la rama actual. NSDBG-2297, líneas 248–261 y 682–695, añade un caso histórico mediante retorno ref y el caso de campo tras destruir el receptor, también sin ejecución del uso inválido. Contrastar seguimiento de aliases, invalidación del contenedor y generación, incluyendo campos, referencias retornadas, dispatch virtual/interfaz y ramas; planear observación dinámica bajo cobertura explícita.

### LKF-064 — Traza de ejecución aparcada y alcance del sanitizador

`A:/nsc-devel/src/LLVM/Internal/LlvmInstrumentationSwitches.h:13` fija kRuntimeDebugTraceEnabled=false; el comentario describe funcionalidad aparcada, por lo que no debe anunciarse como grabación activa. `include/nativesharp/LLVM/NativeSanitizerKind.h:5` sólo enumera Address y `src/LLVM/LlvmNativeSanitizer.cpp:19` selecciona asan, sin demostrar cobertura integral, disponibilidad del runtime ni detección de fugas en cada plataforma. La lectura completa de NSC-009 muestra que, de activarse, el sink usa una ruta fija y append textual; los formatos examinados identifican código y campos, pero no incluyen identidad dinámica de asignación, valores de variables ni hilo. Falta revisar llamadas y concurrencia. Establecer contrato de habilitación, límites, coste y pruebas posteriores; no activar el switch durante fase 0.

### LKF-065 — Copia histórica de struct con ownership transitivo

NSDBG-2298, líneas 641–659 y 904–924, informa que una distribución histórica aceptaba copiar superficialmente un struct con campo owned; la sonda OwnedStructCopy se compiló sin ejecutar. No se ha identificado en este capítulo una versión exacta ni contrastado la rama actual; no afirmar falla vigente. El código actual contiene una consulta recursiva que clasifica como no copiable un struct con campo owned o destructor (`src/Frontend/MemberModel/OwnershipQueries.cpp:152`) y una comprobación para ciertos retornos de lugares move-only (`src/Frontend/Bodies/StatementAnalyzer.cpp:5743`). Esto muestra defensas posteriores; no demuestra cobertura total de la sonda ni confirma una falla vigente. Revisar asignaciones, anidación, arreglos, genéricos, argumentos y retornos, cleanup y diagnósticos.

### LKF-066 — Límites del verificador PDB y tamaños declarados

`A:/nsc-devel/src/Linker/PdbVerifier.cpp:163` calcula el número de bloques con suma uint32 de tamaño declarado y alineación; un tamaño cercano a UINT32_MAX podría envolver. `materializePdbStream` reserva el tamaño declarado antes de comprobar reconstrucción completa (192–203), aunque el archivo esté limitado a 1 GiB. Revisar posibilidad de reserva excesiva, excepción y aceptación de stream incompleto ante entrada corrupta; no se reprodujo ni se afirma explotabilidad. Por otra parte, la detección de checksums es una búsqueda heurística de patrón (44–51), y la presencia declarada de fuentes o símbolos no prueba su corrección, ni que el PDB corresponda a un binario. Investigar validación estructural, GUID/age frente a PE y límites de recursos.

### LKF-071 — Antecedente de delete this en método ordinario

NSDBG-2305, líneas 744–760, informa que una distribución histórica aceptaba `delete this` en un método ordinario; la sonda era sólo de compilación y no ejecutaba el método. La ruta actual `A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp:1049` contiene rechazo específico dentro de destructor; no se ha demostrado la regla completa para métodos ordinarios ni el estado actual de la sonda. Investigar semántica y aliases supervivientes, sin ejecutar ni afirmar una falla vigente.

### LKF-072 — Límites dinámicos en acceso raw y VLA

`A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp:742` describe fixed arrays y punteros raw como nivel sin comprobación dinámica de límites. La ruta VLA multidimensional observada comprueba aridad/tipo (688–730), y fragmentos de HIR calculan índice lineal y dirección raw sin mostrar chequeo de rango en ese fragmento. Es una frontera de análisis/cobertura y una hipótesis sobre la ruta completa, no prueba de error ni de ausencia global de validación. Revisar controles de dimensiones, multiplicación, índices negativos y one-past, distinguiendo intención raw del contrato de arreglo seguro; documentar antes de prometer detección.

### LKF-073 — Retornos tempranos del ejemplo histórico de interfaces

NSDBG-2306, líneas 935–953, muestra referencias de interfaz ordinarias inicializadas con `new`, y salidas de error anteriores al `delete` explícito correspondiente. Si esas ramas se alcanzan y rige el manejo manual descrito por el mismo documento, pueden quedar asignaciones sin liberar. Es inspección de un ejemplo histórico, no reproducción ni defecto demostrado del compilador. La fuente original ya leída como NSDBG-750 confirma esas ramas en `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t08/capitulo-032-interfaces/Program.ns:132` (hash y rangos en evidencia). Contrastar semántica actual y limpieza en rutas de error; no modificar el ejemplo durante fase 0.

### LKF-074 — Salidas previas a liberación en ejemplo histórico de propiedades

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t08/capitulo-033-properties/Program.ns:122` a 161, SHA-256 `83b2902490dd364f9a166af1f737c5c76083e28d4d3dce1ab1797461d6515548`. La reserva de `account` precede retornos de error y el `delete account` aparece después de ellos. El capítulo correlacionado se revisó como NSDBG-2307.

**Alcance:** posible fuga en rutas que alcancen un retorno anterior a la liberación bajo gestión manual; no se ejecutó el ejemplo, no se probó la alcanzabilidad de cada ruta y no se atribuye un defecto al compilador. Los recursos RAII internos tienen tiempos de vida independientes y no se consideran fugados por esta observación.

**Pendiente:** modelar rutas normales y excepcionales, limpieza implícita y propiedad de propiedades; reproducir de manera controlada en una fase autorizada, y registrar resultados sin confundir el ejemplo histórico con el comportamiento actual. Seguimiento: LK-312.

### LKF-075 — Indexador de matriz sin validación explícita de coordenadas

**Evidencia:** ejemplo histórico `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t08/capitulo-034-indexadores/Program.ns:31`–43, SHA-256 `0447f70d2e558dd0ee7637c89d64f7a71de06f41eaf39b658fe8ddd1e9c1ac16`. El getter y setter calculan `row * 2 + column` sin comprobaciones explícitas de cada coordenada en esos cuerpos. El capítulo correlacionado advierte de bounds y overflow.

**Alcance:** riesgo potencial en entradas fuera del contrato; un índice lineal en rango no demuestra coordenadas válidas. No se probó ejecución, aritmética concreta, ausencia de comprobaciones posteriores ni defecto del compilador. Los accesos mostrados en Main usan coordenadas aparentemente válidas.

**Pendiente:** analizar precondiciones y rangos por dimensión, overflow, propagación y comprobaciones efectivas antes de decidir diagnóstico; casos positivos y negativos futuros. Seguimiento: LK-313.

### LKF-076 — Salidas tempranas previas a delete en ejemplo de indexadores

**Evidencia:** la misma fuente, líneas 135–180, contiene reservas sucesivas y retornos de error anteriores a los `delete` al final. La cantidad de objetos pendientes depende de la ruta. Recursos declarados `raii` tienen limpieza distinta y deben analizarse separadamente.

**Alcance:** riesgo condicionado de fuga para reservas manuales si se alcanza una de esas salidas; no se estableció alcanzabilidad ni se ejecutó. No se atribuye un defecto al compilador.

**Pendiente:** comprobar rutas y cleanup normal/excepcional en una fase autorizada; seguir el modelo de obligaciones por instancia y alias de LK-314.

### LKF-077 — Incremento y decremento personalizados diferidos en documentación histórica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-06-tipos-definidos-por-el-usuario/capitulo-035-operadores-y-conversiones-personalizadas.md:229`–239 y 609; SHA-256 `d32b232e2957e01985806c3a2d3bba331330c56bf82969e43238826912cf95af`. El capítulo afirma rechazo histórico de operadores personalizados `++` y `--`, asociado a evaluación única del lvalue.

**Alcance:** afirmación documental histórica, no verificación actual; una búsqueda textual limitada en fuentes C++ del frontend no encontró la redacción literal y no demuestra ausencia ni presencia de soporte. No se compiló ni ejecutó.

**Pendiente:** localizar propietario real de validación y lowering, verificar versión vigente y cobertura mediante pruebas en fase autorizada; registrar diagnóstico y actualización de documentación si procede. LK-315.

### LKF-078 — Unions crudas sin discriminante automático según documentación histórica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-06-tipos-definidos-por-el-usuario/capitulo-036-unions.md:101`–134 y 238–317; SHA-256 `307843b7a716ae925d7285c75888bf08228df315b7b2d269e551154f592646bd`. La documentación describe almacenamiento solapado sin tag implícito y rechazo de campos owned directos o transitivos, borrowed/readonly union y arrays flexibles.

**Alcance:** límites y reglas de diseño descritos históricamente, no un defecto demostrado ni comprobación de implementación vigente. La variante activa no puede inferirse de bytes por sí solos; un tag explícito requiere demostrar la relación con el almacenamiento. La interpretación depende del target y contrato.

**Pendiente:** contrastar semántica, layout y rechazos actuales con fuentes y pruebas futuras autorizadas; registrar incertidumbre de alias, campos activos y propiedad en análisis. Seguimiento: LK-316.

### LKF-079 — sizeof de especialización cerrada en documentación histórica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-037-genericos.md:237`–246, SHA-256 `49c752a29f72ce5e16acfa5bfc86a4d6024ace9599f0a3df7c4bc14736b0320f`. El capítulo afirma que el parser histórico no acepta `sizeof(Box<int>)` y presenta una función genérica alternativa.

**Estado:** afirmación histórica no contrastada con el parser actual; no se compiló. La alternativa no autoriza cambiar APIs ni demuestra equivalencia en todos los contextos.

**Pendiente:** localizar parser y pruebas actuales; comprobar diagnóstico, alcance y alternativa en fase autorizada. LK-317.

### LKF-080 — Métodos genéricos en tipo genérico y export C genérico

**Evidencia:** mismo capítulo, líneas 444–464 y 538–564: documenta rechazo de export C genérico y de método genérico dentro de tipo genérico. Tipos genéricos partial ya consta en LKF-049 y se reitera en 566–577.

**Estado:** restricciones históricas no contrastadas con implementación vigente. La frontera C concreta y el ejemplo alternativo no prueban defectos ni autorizan cambios de API.

**Pendiente:** rastrear verificación y lowering actuales, variantes y cobertura negativa mediante pruebas futuras autorizadas. LK-317.

### LKF-081 — Primer elemento genérico sin precondición explícita local

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t09/capitulo-037-genericos/GenericLibrary/GenericTypes.ns:52`–55, SHA-256 `9ef8e39de0a3b0902e6de900c46ab32e727adb0e1625b11b47ff6e5bab360b04`, previamente inventariado NSDBG-904. `First<T>` lee `values[0]` sin verificar localmente longitud o nulidad.

**Alcance:** riesgo condicionado para entrada vacía o inválida; no se ha demostrado que ocurra ni la semántica o comprobaciones efectivas actuales. El ejemplo usa dos elementos. No se atribuye defecto al compilador.

**Pendiente:** analizar contratos y rutas alcanzables, caracterizar fallo o comprobación y diseñar casos positivos y negativos futuros. LK-318.

### LKF-082 — Restricciones genéricas descritas como no soportadas históricamente

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-038-constraints.md:42`–48, 180–182 y 284–304; SHA-256 `38b6d8e1c368ae837e2251ca4358b812d2063e4e8a94fee9419f5c03ce6374fc`. Se afirma ausencia de where en función global genérica, de structs que implementen interfaces, de relación `where T : U` y de constraint `struct`.

**Estado:** afirmaciones de la documentación histórica, no verificación de la rama actual; sin ejecución ni conclusión de defecto. Cada restricción puede tener propietario o cronología diferente.

**Pendiente:** localizar implementación y diagnósticos actuales de cada caso, verificar negativos y positivos en fase autorizada, conservar diferencias de versiones e impacto de metadata. LK-319.

### LKF-083 — Límites históricos de inferencia genérica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-039-inferencia-generica.md:249`–352 y 711–723, SHA-256 `f636b74912556f7a071f9d9e86e46874ca3fb186fcf7a07c3c6b4428937a5a4f`. La fuente indica que evidencia incompatible no busca tipo común, y que retorno, null aislado y lambda sin target no aportan inferencia.

**Estado:** límites documentales históricos; sin contraste con código actual ni prueba. Pueden ser decisiones de diseño en lugar de defectos.

**Pendiente:** identificar implementación y diagnósticos actuales, diferenciar inferencia, aplicabilidad y selección, y comprobar casos previstos en una fase autorizada. LK-320.

### LKF-084 — Ejemplo genérico accede primer elemento y puntero sin comprobación local

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t09/capitulo-039-inferencia-generica/Program.ns:29`–41, SHA-256 `00fc419a22e7a5af7f6a5dd170830a52ab9316b2436f31e984362a50b29f49fc`, previamente NSDBG-976. First y PointerFirst acceden índice cero sin comprobación local. Sus usos ejemplificados tienen storage preparado.

**Alcance:** riesgo condicionado ante entrada vacía, nula o puntero inválido; no se demostró ruta alcanzable incorrecta, ni ausencia de contrato externo o comprobaciones aplicables. No se ejecutó.

**Pendiente:** rastrear precondiciones, procedencia y longitud del puntero, separando desconocido de error comprobado. LK-320.

### LKF-085 — Constraints propios de alias genéricos en documentación histórica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-040-aliases-genericos.md:347`–361, SHA-256 `8502812fcb0d35c1ccd22632ff1ad30ec13e4d35f903aa48b6b82a2df5688ea2`. El capítulo indica que el alias no declara su propio `where`, aunque se aplican los constraints del tipo subyacente. La limitación histórica de sizeof figura en LKF-079.

**Estado:** afirmación histórica sin contraste de fuente vigente ni ejecución; no se establece un defecto y no se autoriza cambiar la API del consumidor.

**Pendiente:** localizar resolución y diagnósticos actuales, verificar ámbito, conflictos de qualifiers y constraints subyacentes con casos positivos y negativos en fase autorizada. LK-321.

### LKF-086 — Fronteras históricas de targets y retorno genérico de delegate

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-041-delegates.md:136`–139, 351–362 y 628–644; SHA-256 `739ed9f38a2cf0958faefa77e357867bb829919680e4b937ba816145b877ac12`. El capítulo documenta rechazo de métodos virtuales y receptores struct como targets, y un parámetro genérico como tipo de retorno delegate.

**Estado:** fronteras históricas con contraste parcial de fuente actual; no se ejecutó. Son fronteras separadas y no se atribuye una causa raíz general.

**Pendiente:** localizar validaciones, lowering y casos negativos de cada frontera, distinguir restricciones de diseño de defectos y comprobar en versión autorizada. LK-322.

**Contraste parcial actual:** A:/nsc-devel/src/Frontend/Bodies/LambdaDelegateBinder.cpp:1628–1677, SHA-256 da6f0e29a53806d470ba988d408f13edd2902f4c2b877dea408481060fe3ee78. validateDelegateMethodTarget rechaza métodos de instancia cuyo tipo declarante es struct y la explicación aplaza la captura del lifetime del receptor. La misma función contempla despacho virtual o por interfaz, por lo que la prohibición histórica de todo método virtual no puede trasladarse sin más a esta versión. También rechaza préstamo de receptor temporal/fresco en las condiciones visibles salvo transferencia explícita. No se ejecutó; faltan otras rutas y tipos de retorno genéricos. LK-322 y LK-436.


### Complemento de evidencia histórica para LKF-003 — Sondas de delegate

El capítulo 041, líneas 88–92, 281–299, 316–329 y 646–657, aporta resultados de una instalación histórica: check/build 0 para diferencias `in`/`ref` y escape de receptor, sin ejecución de ambos; atribuye a `MovedDelegateRuntimeFailure` salida `0xC0000374` en O0/O2 durante cleanup. NullInvocationTrap se describe compilada, no ejecutada, con guarda observada en LLVM. Son **resultados reportados por la fuente**, no reproducidos en esta fase ni prueba de vigencia en la rama actual. Correlacionar con NSDBG-1021–1028 y LK-322–324; no inferir causa raíz únicamente por código de salida.

### LKF-087 — Capturas y callbacks C: restricciones documentadas históricamente

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-042-lambdas-y-closures.md:286`–320 y 361–373; SHA-256 `ea529c14c2367db56b99049a2d75357be901bb5717e1258f5435c78671333d6b`. Describe rechazo de capturas de storage prestado, VLA, locals raii y parámetros owned, ausencia de sintaxis de transferencia explícita al entorno y rechazo de lambda con captura para delegate extern C.

**Estado:** restricciones históricas con contraste parcial en fuente actual; sigue pendiente su validación integral. Pueden expresar límites de seguridad de diseño; no se afirma defecto. El capítulo también atribuye a los contadores atómicos protección del lifetime, no del acceso concurrente al payload.

**Pendiente:** investigar binder, runtime y ABI vigentes; evaluar cada categoría y casos positivos/negativos en pruebas futuras autorizadas, preservando versiones. LK-325–327.

**Contraste adicional del frontend:** A:/nsc-devel/src/Frontend/Bodies/LambdaDelegateBinder.cpp:251–291, :305–390, :463–586, :615–705 y :751–794, SHA-256 da6f0e29a53806d470ba988d408f13edd2902f4c2b877dea408481060fe3ee78. La captura implícita usa modo referencia; la lista explícita reconoce copy/move/mutref. Para locales y parámetros, la ruta rechaza entre otros préstamos, vistas y referencias bajo las condiciones del código; permite analizar capturas move sujetas a validaciones de pinned, movimiento y préstamos. `this` se registra como receptor prestado, y la función local rechaza copy/move explícitos pero no demuestra aquí un análisis de escape del receptor. Las capturas anidadas se enrutan por padres y sus modos se combinan. Esto corrige la idea de ausencia general de representación move en el frontend actual, pero no prueba sintaxis aceptada de extremo a extremo ni seguridad en ejecución; falta seguir emisores, validaciones de escape y ABI. LK-240, LK-326 y LK-437.


### Contraste NSC-037 — Capturas y despacho de delegates requieren trazado completo

**Fuente actual inspeccionada:** `A:/nsc-devel/src/LLVM/Lowering/OopLlvmClosuresDelegatesEvents.cpp`, SHA-256 `771fd2275e0431d01d0427cc6afd6797863e626332788fdd7a75afe682fc2939`, revisión del archivo contrastada con inventario. Líneas 314–344 contemplan destrucción de captura `Move` propietaria; 700–758 contemplan modos `Copy` y `Move`, incluida anulación de origen bajo condición. Líneas 766–858 contemplan target dinámico mediante ranuras virtuales o interfaz y un receptor marcado owned. Esto **no demuestra** que la sintaxis o todos los casos sean aceptados desde el frontend, que sean alcanzables ni que funcionen en ejecución.

**Relación histórica:** LKF-086 y LKF-087 proceden del libro histórico, que describe restricciones diferentes. Existe una diferencia de capas y posiblemente de revisiones; no se resuelve silenciosamente como contradicción demostrada ni como corrección completada. Contrastar binder, AST/HIR/MIR, metadata, pruebas existentes y versiones para cada ruta. LK-322, LK-325–327.

**Riesgo por verificar:** en líneas 47–84 el cálculo de layout de array fijo multiplica tamaño y cantidad sin comprobación explícita en ese cuerpo. No se han trazado límites o validaciones previas; no se afirma overflow alcanzable ni defecto. Investigar tamaño máximo, aritmética y guardas aguas arriba antes de decidir; LK-328.

### LKF-088 — Divergencia de orden documentado frente a lowering de eventos

**Documentación histórica:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-07-genericos-y-comportamiento/capitulo-043-eventos.md:5`, 109–117 y 159–173, SHA-256 `cf154353ad8cf3ec6d174d7f052294137b29d51e056e35a61ccc375fc2dfb66d`, describe inserción al frente, LIFO y O(1). Su ejemplo histórico `.../ejemplos/t10/capitulo-043-eventos/Program.ns:121`–124, SHA-256 `2057a0d59f8fac3471b154b665daf1c953c9dce0a0f942b1918cb4b645ccbdf0`, espera 21.

**Código inspeccionado:** `A:/nsc-devel/src/LLVM/Lowering/OopLlvmClosuresDelegatesEvents.cpp:1214`–1265, SHA-256 `771fd2275e0431d01d0427cc6afd6797863e626332788fdd7a75afe682fc2939`, reserva nodo, recorre al final y lo agrega, con comentario de orden de suscripción FIFO. Esto implica recorrido lineal para agregar si la ruta se usa y la lista tiene nodos.

**Estado:** discrepancia entre fuentes y posibles versiones o ramas, no una regresión demostrada. No se ejecutó, no se contrastó la revisión exacta que produjo el libro ni se demostraron todas las rutas actuales. El orden afecta selección de primera coincidencia, excepciones y casos reentrantes; no extrapolar sin trazado.

**Pendiente:** identificar contrato vigente, historia de cambio, frontend/MIR y consumidores, diseñar prueba autorizada de orden, duplicados, adición durante despacho y excepción, y actualizar documentación tras decisión. LK-329.

### LKF-089 — Retirada reentrante del sucesor de un evento

**Evidencia:** la fuente LLVM anterior, líneas 1300–1311 y 1366–1389, emite liberación del nodo al desuscribir y carga `next` antes de llamar el handler; la documentación histórica, líneas 288–316, advierte del riesgo. Si durante el callback se retira y libera exactamente ese siguiente nodo, el recorrido conserva un puntero que puede quedar colgante.

**Estado:** inferencia condicionada por alcance del callback, orden y vida efectiva; no se ejecutó ni se demuestra explotación o falla actual. La sonda histórica registrada como LKF-001 no invoca Raise. Además, su orden de alta fue escrito para la semántica histórica LIFO y no demuestra esa misma ruta bajo FIFO.

**Pendiente:** verificar cómo se llega desde la sintaxis al lowering, efectos de reentrancia y sincronización, diseñar reproducción controlada con orden vigente en fase autorizada; incluir auto-remoción, retirada del sucesor y adición. LK-330.

### LKF-090 — Elementos de referencia y liberación de arrays históricos

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-08-memoria-y-ownership/capitulo-044-arrays-dinamicos.md:287`–330, SHA-256 `7fe302bb4700f4729a1975957dd2863bd9834a3fc647075525bbbc7cdf0ce07d`. Describe slots con transferencias de ownership, pero sostiene que `delete[]` libera el buffer sin recorrer objetos referenciados, incluso para buffer owner.

**Estado:** contrato y limitación descritos históricamente; no se verificó la rama actual ni todos los tipos de elementos y rutas de cleanup. No se asume falla ni que el buffer posea automáticamente todos los objetos.

**Pendiente:** contrastar la generación actual, cleanup normal/excepcional, slots movidos y estructuras anidadas; decidir diagnósticos por ruta sin duplicar obligaciones. LK-332–333. El capítulo 050, líneas 382–411 y 737–743, reitera la distinción de buffer y elementos y describe vaciado manual; sus resultados O0/O2 son históricos, no reproducidos aquí. NSDBG-2324; LK-353.

### LKF-091 — Retornos previos al cleanup en ejemplo histórico de arrays

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/ejemplos/t11/capitulo-044-arrays-dinamicos/Program.ns:96`–147, SHA-256 `dbf87299cbffb43afc66c4a7d0c4027451d3045cf8a771ddb5f1ad2973664b69`, previamente NSDBG-1116. Hay retornos antes de liberar referencias manuales y antes de destruir el segundo objeto transferido a slots.

**Alcance:** posibles recursos pendientes si esas rutas ocurren y si la semántica histórica de cleanup indicada en LKF-090 aplica. No se estableció alcanzabilidad, cleanup implícito vigente ni fuga en ejecución; buffer raii y objetos son obligaciones distintas.

**Pendiente:** trazar cada salida y estado de cada slot, verificar comportamiento de la revisión autorizada y diseñar casos futuros de éxito, fallo y excepción. LK-333.

### LKF-092 — Lifetime de alias al mover un owner de array

**Evidencia:** el capítulo 044, líneas 133–149, advierte que un alias no debe sobrevivir al movimiento del owner, sin precisar si la regla es estática, invalidación física del bloque o precondición de préstamo.

**Estado:** no se ha comprobado ese detalle; transferir un handle no demuestra por sí solo que se liberó o reubicó el buffer. Tratar como incertidumbre de contrato y no como use-after-free demostrado.

**Pendiente:** contrastar el análisis vigente de préstamos y validez de alias con movimiento, nueva propiedad, posterior destrucción y escape. LK-334.

### Contraste parcial de bitmap de vida en arrays — LKF-090

**Fuente inspeccionada parcialmente:** `A:/nsc-devel/src/LLVM/Lowering/OopLlvmArraysAndStores.cpp:1094`–1230, SHA-256 `fec0429337961ff15509ea89fe7ecfe25f5a04967e6a6a17ea59da3325be1e40`. El lowering limita count por tamaño de datos y `0x7fffffff` en 1163–1169; en 1174–1180 añade bitmap para ciertos structs con destructor, y al inicializar marca bits en 1207–1221. La función de inicialización tiene una ruta separada para segmentos y otra para expresiones ordinarias.

**Alcance:** evidencia de emisión de bitmap, no demostración de destrucción completa ni prueba de que el contrato histórico de referencias owned haya cambiado. El ejemplo de header+payload del capítulo 044 es insuficiente para describir todos los bloques de la rama observada. La suma de bytes de bitmap aparece después del límite calculado para datos; no se han trazado las cotas ni probado overflow alcanzable. No se leyó aún el archivo completo ni se ejecutó.

**Pendiente:** seguir clasificación `arrayElementLivenessTracked`, cálculo y límites de bitmap, creación parcial, stores, delete[] y cleanup normal/excepcional; verificar evaluación y lifetimes de segmentos spread. LK-332–333, LK-335.

### LKF-093 — Parámetros y retornos directos de array inline en fuente histórica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-08-memoria-y-ownership/capitulo-045-arrays-inline-de-tamano-fijo.md:297`–313, SHA-256 `38ea3ad4c96b3242c32247b66941e6aa1390116b487e1aaec4bb7f1736162bce`, describe rechazo de parámetros y retornos directos de array fijo.

**Estado:** limitación histórica no contrastada con el frontend actual; no se ejecutó ni se considera por sí sola defecto. Las alternativas descritas por la fuente no autorizan cambiar firmas públicas.

**Pendiente:** rastrear firma, ABI y diagnósticos vigentes, con variantes por valor y referencia y pruebas futuras autorizadas. LK-338.

### LKF-094 — Slots de referencia en array fijo y salida temprana histórica

**Evidencia:** capítulo 045, líneas 362–400 y `.../ejemplos/t11/capitulo-045-arrays-inline/Program.ns:107`–126, SHA-256 `899aa200f14c438ea49aa6a57056545cc4ea1ce4d464065d3c994f4cafecb1a4`. Se transfieren dos owners a slots; una ruta retorna antes de `delete slots[1]`. La fuente histórica indica que el storage inline no destruye automáticamente objetos referenciados.

**Estado:** riesgo condicionado a alcanzabilidad y semántica vigente del slot; no se ejecutó ni se probó la fuga. No generalizar esta descripción a arrays de structs con destructor o a otros tipos de cleanup.

**Pendiente:** contrastar tipo y datos de propiedad a través de stores, moves, scope y unwind; verificar cada salida en fase autorizada. LK-337.

### Complemento histórico para LKF-002 y LKF-004

El capítulo 045, líneas 192–210, 396–400 y 633–641, reporta dos sondas O0/O2 con acceso variable fuera de rango y ausencia observada de trap, y una sonda de copia de slots owners aceptada para compilación sin ejecución de su cuerpo. Las fuentes son `RuntimeUpperBound/Probe.ns` SHA-256 `cb4d4355919136f002428c716cdc54a677074ed5353b9e366acb3fdb32466cf2`, `RuntimeSecondDimension/Probe.ns` SHA-256 `10dc7c64f39f2fbcafe7763862d56f49a0cde1bb4301977170205c02bcb3d845` y `OwningFixedArrayCopyAccepted/Probe.ns` SHA-256 `55d3e969ad9556ac04a5fce47d50f10be153cac05da4ddeacb72892554efe440`. El último Main no llama Wire. Son resultados históricos reportados, no reproducidos ni certificación de versión vigente. Contrastar NSC-036 y LK-336–337.

### LKF-095 — Arrays rectangulares: diferencia entre documentación y lowering

**Documento histórico:** capítulo 044, líneas 353–357, SHA-256 `7fe302bb4700f4729a1975957dd2863bd9834a3fc647075525bbbc7cdf0ce07d`, dice que sólo existe rango dinámico uno y no hay arrays rectangulares heap.

**Código inspeccionado:** `A:/nsc-devel/src/LLVM/Lowering/OopLlvmArraysAndStores.cpp:1233`–1424, SHA-256 `fec0429337961ff15509ea89fe7ecfe25f5a04967e6a6a17ea59da3325be1e40`, contiene rutas `lowerRangeArrayAllocate` y `lowerRangeArrayElementAddress` con rank >= 2, dimensiones, stride, guards y referencias explícitas a 3.0.

**Estado:** diferencia de revisión/capa; la existencia del lowering no demuestra qué sintaxis acepta la rama, su disponibilidad pública ni ejecución correcta. No se ejecutó. No sustituir una fuente histórica por la otra.

**Pendiente:** contrastar versiones, sintaxis, frontend/HIR/MIR, consumidores y pruebas; documentar rango, layout y límites sólo para revisiones verificadas. LK-339.

### LKF-096 — Fuente spread nullable antes de lectura de longitud

**Evidencia:** misma fuente LLVM, líneas 1041–1077: al procesar un segmento Spread, `collectArrayAllocationCount` emite carga de longitud desde `value` sin una llamada explícita a `checkedReceiver` en ese bloque.

**Estado:** riesgo condicionado; no se sabe si el frontend, `lowerExpression`, contrato de colección u otra guarda excluye null, ni si la ruta es alcanzable con esa entrada. Una ausencia de guard local no demuestra defecto. No se ejecutó.

**Pendiente:** rastrear construcción del MIR, precondiciones, conversión y guards previos, así como fallo y cleanup de segmentos; diseñar casos nulo/válido en fase autorizada. LK-340.

### LKF-097 — Autoincremento de slot VLA rechazado en documentación histórica

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-08-memoria-y-ownership/capitulo-046-arrays-locales-de-longitud-variable.md:121`–131 y 319–335, SHA-256 `3b638d30a0babbb9438adaa9e419e315dd1e7159811954b40947dd9b34047b9d`. El capítulo describe que `values[i] = values[i] + 1` se diagnostica como escape y clasifica VlaSelfUpdate como rechazo.

**Estado:** reporte histórico sin ejecución ni contraste de la rama actual; no atribuir causa raíz a una fase específica hasta rastrearla. La alternativa sugerida por el libro no es autorización para modificar consumidores.

**Pendiente:** localizar sonda y diagnóstico, seguir escape por frontend/HIR, investigar falsos positivos, alias y operaciones equivalentes y validar en fase autorizada. LK-341.

### Complemento de LKF-012–013 y LKF-072 — límites históricos y lowering actual de VLA

El capítulo 046 líneas 38–117 describe evaluación única de extensiones, un límite histórico de 1 MiB por reserva, ausencia de memset y resultados de sondas; indica lectura no inicializada compile-only. NSC-038, `A:/nsc-devel/src/LLVM/Lowering/OopLlvmArraysAndStores.cpp:716`–778, SHA-256 `fec0429337961ff15509ea89fe7ecfe25f5a04967e6a6a17ea59da3325be1e40`, emite ese límite por operación y verifica factores antes de `alloca`. No establece un límite acumulado para iteraciones ni prueba funcionamiento en ejecución. La sonda UninitializedReadAccepted no llama Read desde Main, como ya registra LKF-013; no se ejecutó aquí. La ruta de acceso raw y las rutas de dimensiones deben seguirse hasta sus llamadores antes de extrapolar ausencia total de guardas. LK-341–343.

### LKF-098 — Bounds condicional de miembros flexibles requiere trazado de metadata

**Fuente histórica:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-08-memoria-y-ownership/capitulo-047-miembros-flexibles.md:166`–210, SHA-256 `3f13985f558f26e59734bff3f21e1467206c2020b197701a37a6ea068f175efb`, indica que la longitud es externa y describe una ruta sin bounds automático.

**Fuente actual:** `A:/nsc-devel/src/LLVM/Lowering/OopLlvmArraysAndStores.cpp:89`–116, SHA-256 `fec0429337961ff15509ea89fe7ecfe25f5a04967e6a6a17ea59da3325be1e40`, emite comparación y guard cuando `hasTrailingLengthBounds` es verdadero y luego calcula el desplazamiento raw.

**Estado:** diferencia potencial de revisión y de condiciones de metadata; no se ha comprobado qué declaraciones activan la marca ni si una longitud declarada corresponde a la capacidad real. La existencia de la guarda no valida por sí sola la asignación física y su ausencia local no demuestra un defecto.

**Pendiente:** rastrear productor de metadata, offsets y signedness, accesos, corrupción del count, overflow y procedencia de capacidad real; pruebas futuras en fase autorizada. LK-344.

### Complemento histórico de LKF-015 — copia de prefijo y cola sin miembro previo

Capítulo 047, líneas 242–252 y 333–346, describe sondas compile-only para copia por valor del prefijo y para struct con sólo cola, junto a su riesgo contractual y de portabilidad C. No demuestra que una cola completa se copie, que cada ABI acepte la forma o que un defecto siga vigente. Reconciliar layout, aceptación y comportamiento con versión/target y sondas NSDBG-1221–1224; LK-345.

### Trazado parcial de TrailingStorage — complemento de LKF-098

En `A:/nsc-devel/src/Frontend/HIR/HirExpressionMachine.cpp:1497`–1553 (SHA-256 `0b1dcebc7358688df55f3ba5f2c23eaf1d9229a7cafd14ad52e2aecdc7dda502`) y `.../HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp:501`–544 (SHA-256 `29a4036d83d7c514ea0358d59c4a4ca760f07c3440b2ba1ccefd09049e1d2c5e`), la marca se configura para un acceso flexible cuando encuentra atributo `TrailingStorage`, argumento y layouts del campo de longitud y de datos; calcula offset relativo firmado. `.../MIR/Internal/MirNodeMachine.cpp:456`–460 (SHA-256 `0714765b7f5a540065f71b72033f4e4a51e5d3ac3fb4228c55b36db482374fb2`) propaga bandera y offset a MIR. El consumidor LLVM de NSC-038 lee longitud de 32 bits y compara índice.

**Alcance:** lecturas parciales; todavía no se rastreó validación de atributo, tipos de length, forma sintáctica o correspondencia con capacidad real; no demuestra defecto ni cobertura de todos los accesos. El comentario de `ProjectOopHirNode.h:88`–94 menciona tanto `secondaryIntegerValue` como el campo firmado dedicado; distinguir comentario histórico de campo efectivamente propagado. No se ejecutó. LK-344.

### LKF-099 — NUL interior y codificación al cruzar frontera CString

**Evidencia:** `A:/nsc-debugger/tests/corpus/documentation/the-complete-reference/libro-i-el-lenguaje/parte-08-memoria-y-ownership/capitulo-048-strings.md:353`–382 y 415–426, SHA-256 `ab2040beb96ce67f49fb0ea361cb5d0d9305dcae7b1d027a1cc789a7969b0d6c`. Describe que CString copia bytes sin rechazar NUL interiores ni convertir codificación, y que ciertas operaciones de casing/orden son ASCII.

**Estado:** límites históricos documentados y contrastados parcialmente con CString vigente en NSC-039, descrito más abajo; no se ha probado una llamada externa. La truncación depende del contrato C consumidor y no implica por sí misma un defecto de memoria.

**Pendiente:** completar contraste de StringView y UTF-8, contratos de host y lifetime del buffer; planear casos de NUL, encoding y retención posterior. NSDBG-2350:436–450 describe una sonda histórica que compila y entrega un puntero de CString a un proveedor que lo retiene después de que termina su owner; la sonda se declara compile-only, no prueba ejecución actual ni un use-after-free observado. Seguir procedencia y efectos conocidos/desconocidos de FFI; no suponer retención por el solo tipo. LK-346 y LK-388.

### LKF-100 — Puntero mutable hacia representación inmutable de string

**Evidencia:** mismo capítulo, líneas 92–110: describe que CharacterPointer expone un puntero físicamente escribible aunque la cadena sea inmutable y podría apuntar a storage constante.

**Estado:** riesgo condicionado de violación de contrato o escritura en memoria de sólo lectura, no se ha revisado la firma o implementación vigentes ni observado la escritura. Un puntero usado sólo para lectura durante el lifetime válido no demuestra error.

**Pendiente:** trazar procedencia de literal/heap, offset y extensión, usos FFI, escape y mutabilidad efectiva; diseñar pruebas futuras distinguiendo lecturas válidas y escrituras indebidas. LK-347.

**Contraste adicional de fuente:** String.ns:60–79 (SHA-256 72fb8ae33aab4b82104bd0ca90692ea2d70f7326bd84681f13fe13433043fe74) declara CharacterPointer como char* y advierte que escribir puede corromper una cadena; el cuerpo es placeholder de operación core según comentario. Falta trazar lowering efectivo, secciones y usos; no se observó escritura.

### LKF-101 — Tamaño de CString en extremo de int

**Evidencia estática actual:** `A:/nsc-devel/lib/NativeSharp.Core/CString.ns:44`–56 y 76–80, SHA-256 `6e16130f2797e29523c40f0298dcddb9e1a15ea11f9be7d965835b59e721f1cf`. El constructor obtiene `texto.Length` como int, calcula `longitud + 1` para el array y escribe el terminador; SizeWithNul vuelve a sumar uno.

**Estado:** límite/riesgo condicionado a una longitud máxima alcanzable y a la semántica real de overflow y reserva. El cuerpo no muestra una comprobación específica de la suma, pero no se ha demostrado que una entrada con longitud problemática pueda construirse ni un fallo observable. No se ejecutó.

**Pendiente:** trazar límites de productores de string, overflow signed, allocation y fallos, definir precondición y caso futuro con oráculo seguro; no modificar contrato sin autorización. LK-349.

### Contraste NSC-039 — CString, complemento de LKF-099

La fuente anterior, líneas 24–33 y 44–73, copia cada byte, agrega NUL, conserva longitud y libera el array en destructor; `Pointer()` expone una dirección hacia ese array. Esto respalda estáticamente el contrato por bytes y lifetime en esta revisión, no una prueba de ejecución ni garantiza que todas las APIs externas compartan encoding o procesen NUL interior de la misma manera. Verificar resolución de owner, fallo de constructor y uso tras destrucción. LK-346 y LK-349.

## Limitaciones de integración observadas en otros repositorios

### LKF-070 — Rutas PDB explícitas no ejercidas por un test del proveedor

`A:/nsc-debugger/tests/SymbolProviderTests.cpp:36` recibe rutas de imagen/PDB, pero la variable pdb local termina descartada en línea 182 y el bucle de pares opcionales usa la imagen sin consumir la segunda ruta (129–135). La prueba sí propone escenarios de búsqueda por proveedor, identidades ausente/distinta/corrupta y sesión real. No demuestra por sí sola carga explícita de un PDB arbitrario; no implica que el producto carezca de esa capacidad ni que otras pruebas no la cubran. Investigar interfaces y demás tests.



### LKF-069 — El gate de handoff revisado no invoca directamente NSDBG

`A:/nslink-devel/tests/handoff/RunNsdbgHandoffGate.cmake` produce variantes O0–O3 y usa DbgHelp y una sonda de salida, pero el script leído no invoca directamente el depurador NSDBG. Esto limita lo que esa prueba aislada demuestra; no implica que no haya otras pruebas de integración. El consumidor NSDBG sí tiene pruebas separadas: `A:/nsc-debugger/tests/NslinkDllIntegrationTests.cpp:38` cubre por diseño líneas, breakpoint y stack entre EXE y DLL, y `NsasmIntegrationTests.cpp:35` cubre NSASM/NSLink. Fueron leídas, no ejecutadas; falta contrastar cobertura de variantes y recording. Diseñar la integración restante sin ejecutar el gate durante fase 0.



### LKF-068 — Alcance de una prueba de identidad PE/PDB de NSLink

`A:/nslink-devel/tests/pdb/PdbIdentityTests.cpp:125` comprueba RSDS y edad en el PE y busca el GUID en cualquier lugar del PDB. Esa aserción aislada no analiza el information stream ni compara su age; tampoco demuestra rechazo de un PDB equivocado. Es una limitación del alcance de esa prueba, no afirmación de un defecto del escritor ni de toda la suite. La prueba separada `A:/nsc-debugger/tests/PdbIdentityTests.cpp:16` compara identidades leídas de PE/PDB y contempla PDB equivocado y par NSLink O0 opcional; no se ejecutó. La implementación revisada como NSDBG-009, `A:/nsc-debugger/src/core/PdbIdentity.cpp`, compara GUID y age; su lectura de tamaños declarados aún requiere auditoría. Contrastar las demás pruebas y casos límite antes de concluir cobertura, y completar los negativos faltantes. No se ejecutó.



### LKF-067 — Referencia de pool selecciona raíces antes de aplicar flags

En `A:/nsc-memory-tools-reference/valgrind-3.28.0/memcheck/tests/leak-pool.c:101`, cells se selecciona usando static_roots antes de set_flags(arg) en línea 108. Como static_roots inicia en cero y no se reasigna cells, el argumento posterior no cambia esa selección. Es una observación estática de una referencia externa, no un defecto de NativeSharp ni un resultado ejecutado; no usar esa sonda como evidencia de cobertura de raíces estáticas sin contrastar intención, versión y resultados.



- **LKF-009 — NSASM: datos CodeView incompletos para consumidores:** la lectura de A:/nsasm-devel/src/debug/CodeViewBuilder.cpp mostró checksum ausente, ausencia de columnas y sección de tipos mínima. No se afirma qué transforma NSLink en el PDB final. Implicación para LeakKiller: preservar información desconocida y comprobar identidad, columnas y tipos sin inventarlos. Evidencia NSASM-007/008 y 029; LK-187–188.
- **LKF-010 — Depurador: captura y alcance de proceso:** en A:/nsc-debugger/src/core/DebugEngine.cpp se observó DEBUG_ONLY_THIS_PROCESS en el inicio y la consulta de variables requiere parada. La cobertura real de procesos hijos y valores históricos está pendiente; una búsqueda acotada no encontró grabación/reproducción, pero no demuestra ausencia definitiva. Evidencia en la sección Depurador de EVIDENCIA.md; LK-096 y tareas de integración correspondientes.
- **LKF-011 — Instrumentación de asignación incompleta si se intercepta una sola ruta:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp contiene rutas directas de heap además de wrappers generales. Esto es un límite de una estrategia de integración propuesta, no un defecto del runtime. Investigar procedencia de asignadores, arena/reset y rutas de fallo antes de fijar cobertura; LK-147–150.

## Mantenimiento

Actualizar este documento con cada limitación nueva, enlazar evidencia, versión y tareas; separar defecto reproducido, hecho de lectura, sonda no ejecutada y decisión de lenguaje. No cambiar el estado de la campaña existente sin evidencia actual. Para código negativo del corpus, registrar que la clasificación es del corpus y no presumir que el compilador actual lo acepta o rechaza. No implementar ni ejecutar pruebas durante fase 0.


- **LKF-102 — Propiedades de punteros raw sin demostrar (pendiente):** el capítulo histórico 049 describe punteros sin información inherente de límites, vida, alineación ni propiedad; sus ejemplos de null y alineación son funciones sin Main y no se ejecutaron aquí. El lowering NSC-038 incorpora una guarda condicional cuando existe metadata de longitud de cola; su presencia o ausencia local no prueba cobertura global ni capacidad física. Verificar rutas vigentes, alcanzabilidad y diagnósticos por target y optimización; no se afirma defecto reproducido. NSDBG-2323, LKF-017.

- **LKF-103 — Movimiento y reasignación: fronteras históricas (pendiente):** el capítulo 051, líneas 219–250 y 803–817, describe restricciones históricas de reasignar directamente new o retorno owned a local RAII y una segunda extracción desde campo documentada solo como compile-only. Esto no demuestra semántica runtime ni estado vigente. Contrastar checker, lowering, aliases y vaciado efectivo; revisar impacto en API y documentar alternativa únicamente si sigue aplicando. NSDBG-2325, LK-355.

### LKF-104 — Composición y límites históricos de préstamos

**Evidencia:** NSDBG-2326, capítulo 052, líneas 195–214, 263–285, 345–415, 464–500 y 821–840, describe lifetimes léxicos, excepción especializada de temporales string, rechazo histórico de binding local desde retorno borrowed y ausencia de relaciones generales de lifetime para campos/receptor. Sus afirmaciones de ejecución O0/O2 y diagnósticos son históricas; no se han reproducido.

**Pendiente:** contrastar con análisis semántico vigente, distinguir capacidad no implementada, restricción intencional y defecto, verificar variantes de retorno, reborrow, raíz, mutabilidad, FFI y alias. No proponer como solución eludir el contrato mediante punteros. LK-356–357.

### LKF-105 — Contrato IDisposable histórico y distribución vigente

**Evidencia:** NSDBG-2327, líneas 527–533, afirma que el SDK histórico no incluía la interfaz y que el ejemplo la declaraba localmente. NSC-040, fuente actual lib/NativeSharp.Core/IDisposable.ns, sí declara NativeSharp.Core.IDisposable con void Dispose y describe dos formas de using: objeto nuevo y objeto existente. La presencia en fuente contradice extrapolar la carencia histórica a todo NativeSharp actual; no prueba su inclusión en el SDK instalado, ni el lowering o comportamiento runtime.

**Pendiente:** comprobar empaquetado y versiones, conflictos por declaración local histórica, semántica real por forma de inicializador, excepciones anidadas y cleanup parcial. Clasificar como problema vigente solo tras contraste autorizado. LK-358–359.

### LKF-106 — Thread: fallos, excepciones y concurrencia del control

**Evidencia:** NSC-041, Thread.ns:133–185, constructor delega a ThreadCreate, Join/Detach comprueban y modifican joined_ ordinario y destructor llama Join solo si continúa falso. El archivo documenta resultado cero ante fallo, y advierte de excepción no capturada en worker y posible terminación de proceso (92–119); son afirmaciones de comentarios aún no contrastadas con runtime. El flag visible no tiene sincronización explícita en esta clase: si llamadas concurrentes fueran permitidas y alcanzables, habría que investigar carrera y consumo duplicado; no se demuestra que esa situación esté soportada ni que ocurra.

**Contraste parcial posterior:** WindowsRuntimeBuilder.cpp:3661–3912, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a: create entrega directamente la celda a CreateThread y retorna el handle; Join omite llamadas para handle cero y, para handle no cero, emite espera y cierre sin inspeccionar sus retornos; Detach emite cierre sin espera. El trampoline carga código/receptor y usa llamadas LLVM ordinarias. Si la espera falla, el wrapper no comunica el resultado y Thread.Join marca joined_ igualmente: la conclusión de terminación requiere esperar con éxito, no solo observar retorno de Join. La alcanzabilidad y consecuencias no se han reproducido. La vida de celda/delegate queda sin resolver.

**Pendiente:** seguir wrappers y códigos de error Win32, retención del delegate, orden parcial de eventos y sincronización; distinguir cierre del handle de terminación y cubrir fallos de creación/espera y excepción de worker. No se ejecutó ni se afirma defecto reproducido. LKF-019, NSDBG-2328, LK-360–361.

### LKF-107 — Cleanup de construcción fallida sin contraste completo

**Evidencia:** el capítulo histórico NSDBG-2329, líneas 741–753, afirma que el lowering limpia el storage si falla un constructor tras la reserva. No describe todas las rutas ni demuestra comportamiento vigente; su resultado O0/O2 corresponde a otro ejemplo y no constituye prueba específica de este caso.

**Contraste parcial:** OopLlvmTypesAbiAttributes.cpp:23–255, SHA-256 73662e8fdf82706cd897f2a241ba3add393ef18c8cbbe24f441aa60ce21f4c5d, define cleanup de fallo de construcción asociado a sidecar de allocator: carga thunk y contexto, libera objeto y sidecar, y continúa unwind. La ruta de fallo de reserva del sidecar llama el thunk y trap. Eso demuestra emisión local de esta ruta, no que todas las construcciones la invoquen ni el cleanup de campos parciales.

**Pendiente:** seguir reserva, constructores base/derivados y campos inicializados, inicio de obligación de owner, EH y liberador por clase/allocator; distinguir qué destructores aplican a subobjetos parcialmente construidos, excepción durante cleanup, fallo de reserva y excepciones concurrentes. Confirmar semántica prevista contra código actual antes de clasificar garantía o defecto; no se ejecutó. LK-362.

### LKF-108 — Advertencia de stackalloc en bucle y visibilidad

**Evidencia:** NSDBG-2330:233–246 informa que una CLI histórica no mostró el warning. Fuente vigente ConstructionAndLifetimeExpressions.cpp:862–870 emite Warning si la asignación es válida y loopStack_ no está vacío; DiagnosticEngine.cpp:44–65 almacena y cuenta el warning; DriverProjectActions.cpp:280–287 llama renderAll solo si hasErrors en validateProjectCompilationResult. El flujo de acciones de proyecto llama esa validación después de lowerProjectLlvm en líneas 487–499; en esa función, un conjunto solo de warnings no provoca renderAll; no se ha demostrado que todas las acciones CLI pasen exclusivamente por esa ruta ni que otro consumidor no muestre la advertencia.

**Pendiente:** seguir cada acción build/check, cache y LanguageService hasta el usuario; determinar duplicación, severidad, código compartido InvalidStackalloc y si el aviso afecta salida/código de estado. Diseñar comprobaciones futuras con y sin errores y con caché. No se ejecutó ni se afirma problema universal. LK-364.

### LKF-109 — Cleanup de inicializadores posteriores a placement con allocator

**Evidencia parcial:** OopLlvmConstruction.cpp:619–653 instala sidecar solo cuando constan las tres piezas de procedencia de allocator; 665–704 invoca el constructor de clase con cleanup de fallo para ese caso. Las líneas 738–772 llaman después a runPlacementInitializers, fuera de ese bloque de invoke explícito. Esto no demuestra que una excepción en un inicializador posterior omita cleanup: puede existir protección en otra capa o la ruta no ser alcanzable con inicializador que lance.

**Pendiente:** seguir construcción del MIR, clases de inicializador y propagación EH, cleanup activo después del constructor, ownership de argumentos y estados parciales; comprobar también struct, raw prestado, allocator y cleanup que lanza. Determinar alcanzabilidad y contrato antes de clasificar defecto. No se compiló ni ejecutó. NSDBG-2331, LKF-107, LK-365.

### LKF-110 — Cobertura de promoción y contrato noheap por versión

**Evidencia:** NSDBG-2332:393–408 enumera restricciones históricas de promoción para un local RAII. HirBuildCoordinator.cpp:1451–1519 muestra actualmente esa ruta conservadora; 1522–1553 define otra para locales scope, con criterios distintos y comentario de que sema rechaza escapes. Por ello la enumeración histórica no debe generalizarse a toda promoción vigente. LibraryMetadataConstruction.cpp:442 copia isNoHeap a una bandera de miembro: esto no demuestra por sí solo serialización, compatibilidad de versiones, lectura ni validez del cuerpo externo.

**Pendiente:** seguir análisis de escape scope, llamada de ambas rutas, inicialización/destrucción, límites por frame, metadata de productor y consumidor y confianza de extern/dispatch; comprobar si reglas y diagnósticos históricos cambiaron. No se ejecutó ni se afirma defecto. LKF-024, LK-366–367.

### LKF-111 — Estado y ownership de excepciones anidadas y entre módulos

**Evidencia parcial:** WindowsRuntimeBuilder.cpp:6039–6062 consulta valor TLS; 6068–6109 limpia el slot a null; 6112–6158 publica objeto, lo incluye en registro SEH y termina con 253 si falla inicialización o publicación; 6198–6260 importa payload en TLS del módulo consumidor. NSDBG-2333:423–435 declara que el ownership después del handler no está documentado. Estas funciones por sí solas no muestran toda la restauración en handlers anidados, rethrow, destrucción del objeto ni qué sucede si falla limpiar TLS.

**Contraste adicional:** OopLlvmControlFlow.cpp:300–348 registra ownerSlot y, al completar normalmente un catch, emite clear seguido de emitCaughtExceptionRelease; OopLlvmCleanup.cpp:1522–1540 recorre frames de catch abandonados por salida estructurada. Esto aporta evidencia de liberación en esas rutas concretas, no demuestra restauración TLS ni cobertura de todos los paths.

**Pendiente:** seguir lowering completo de catch/finally/rethrow, save/restore de estado, ownership y destrucción por rutas normales/excepcionales y por módulo; comprobar efectos de fallo TLS y excepciones durante cleanup. No se infiere pérdida de excepción ni fuga y no se ejecutó. LK-368–369.

### LKF-112 — Ambigüedad y espera de Process.Run

**Evidencia:** NSC-042 Process.ns:40–65 documenta retorno -1 para fallo de lanzamiento y para exit code hijo 0xFFFFFFFF, sin timeout; el cuerpo fuente devuelve -1 como marcador de operación núcleo. NSDBG-2335:323–344 describe la misma limitación históricamente. El texto fuente no demuestra por sí solo comportamiento runtime, ni que el lanzamiento realmente ocurra en toda ruta de compilación.

**Contraste parcial runtime:** WindowsRuntimeBuilder.cpp:2249–2325 y 2328–2395 emite cierre del handle de salida tras CreateProcess, espera INFINITE sin inspeccionar su retorno, inicializa exit code a -1, invoca GetExitCodeProcess sin inspeccionar su retorno y cierra handles de hilo y proceso. El resultado -1 también puede representar fallo al obtener el código. La ausencia de comprobación de espera no demuestra que un proceso aún activo haya sido reportado como terminado; esa ruta debe analizarse.

**Pendiente:** contrastar reconocimiento de firma, lowering y wrappers: creación, espera, códigos de error, captura de salida, cierre de handles, posible truncamiento y propagación de terminación; distinguir hijo que retorna -1 de fallo al crear si no hay información adicional. No se ejecutó ni se propone atribuir certeza inexistente. LK-372.

### LKF-113 — Lectura ordinaria de atomic y precisión del orden entre hilos

**Evidencia parcial:** Atomics.ns:68–110 declara AtomicInt32.Value como getter que retorna value_ ordinariamente y documenta Load/Store como operaciones atómicas sustituidas por compilador; esos cuerpos son marcadores y no prueban lowering final. NSDBG-2336:169–195 resume release/acquire en la misma ubicación; esa formulación aislada no identifica cuál escritura leyó acquire y no basta para concluir una sincronización concreta. La guía también presenta Join como frontera de finalización; LKF-106 registra que el wrapper no inspecciona el resultado de espera.

**Contraste adicional:** NSC-044 AtomicOrdering.cpp:106–183 valida orden de load/store/RMW vía validMemoryOrder, mapea relaxed a monotonic, usa LLVMBuildAtomicRMW y LLVMBuildAtomicCmpXchg y retorna el valor anterior del par CAS. La función CAS recibe los dos órdenes. NSC-045 MemoryModel.cpp:12–46 implementa la matriz por operación y pareja CAS; aún deben seguirse los sitios que invocan el validador y su integración de extremo a extremo.

**Pendiente:** contrastar modelo formal de NativeSharp, lowering por target y versiones, casos de lectura desde release o secuencia pertinente, operaciones fallidas, flags reutilizados y mezclas de accesos; trazar Value y accesos concurrentes, sin asegurar semántica global por comentarios ni ejecutar. LKF-027, LK-374.

### LKF-114 — Alcance de volatile y fences por procedencia

**Evidencia:** NSDBG-2339 describe el contrato histórico de fields volatile NativeSharp y fences; NSC-045 declara volatile sin sincronización en el manifiesto NativeSharp. En cambio NSC-038 OopLlvmArraysAndStores.cpp:172–185 y 229–233 tiene una ruta específica de C bool volatile con cppMicrosoftVolatileSemantics=true que marca carga acquire y store release, con condiciones adicionales. No debe generalizarse ninguna de estas rutas a todos los volatile importados o a toda la semántica de C/Clang. OopLlvmExpressionDispatch.cpp:873–885 emite LLVMBuildFence con indicador compilerOnly para singlethread.

**Pendiente:** trazar procedencia de metadata, target, todos los anchos, flags y rutas de carga/store y compuestos; verificar si cada fence está vinculado a un protocolo y cómo se observa. Evitar que la instrumentación agregue lecturas o escrituras MMIO y explicitar cobertura ausente. No se ejecutó ni se afirma defecto universal. LK-377.

### LKF-115 — Wait false no distingue timeout de fallo

**Evidencia:** WindowsRuntimeBuilder.cpp:4308–4347 emite SleepConditionVariableSRW y retorna si su BOOL es no cero, sin capturar código de error en esa ruta. Las líneas 4353–4412 emiten WaitForSingleObject para Event/Semaphore y retornan true solo para resultado cero; handle cero, timeout y otros resultados se convierten en false. Por ello la afirmación histórica NSDBG-2340:504–515 que identifica false como timeout no cubre todos los casos representados por estas funciones. No se han seguido todas las capas ni ejecutado fallos.

**Pendiente:** contrastar reconocimiento de APIs, creación/fallo, códigos nativos y comportamiento de wait/close, estado del lock en errores, contrato público y necesidades de compatibilidad. Para reporte, conservar resultado desconocido cuando solo se observa false; no declarar timeout sin evidencia adicional. LK-378.

### LKF-116 — Tabla histórica de layout y tipos de 128 bits

**Evidencia:** NSDBG-2341:22–37 presenta máximo natural de ocho bytes para su target y tabla histórica sin i128/u128. ProjectObjectLayoutModel.cpp:283–302 en la fuente actual devuelve tamaño y alineación 16 para builtin:i128/u128. La regla histórica no debe extrapolarse a todos los tipos actuales; se requiere verificar alcance de tipos, directivas, target y consumidor. No demuestra por sí sola incompatibilidad ni comportamiento ejecutado. LK-379.

### LKF-117 — Producto de layout de array inline por contrastar

**Evidencia:** ProjectObjectLayoutModel.cpp:310–318 obtiene el layout de elemento y count, y retorna elementLayout->first * count sin comprobación visible en ese bloque. El mismo archivo, líneas 473–493, sí usa targetLayout_.layoutArray y diagnostica error cuando no hay layout para arrays de campo; todavía no se ha demostrado que esa guarda cubra todos los consumidores de valueLayoutForTypeKey. La ausencia local no demuestra overflow alcanzable ni defecto.

**Pendiente:** seguir representación de count, constantes, especializaciones, máximos por target, límites de agregado y operaciones de layout en todas las rutas; diseñar casos futuros de límites sin intentar reservar objetos enormes ahora. NSDBG-2341, LK-380.

### LKF-118 — Publicación C de offsets explícitos

**Evidencia:** NSDBG-2342:441–447 documenta el rechazo histórico de header para un struct con offsets explícitos. CHeaderGenerator.cpp:465–470 rechaza hasExplicitFieldOffsets en agregado anónimo y :637–657 lo rechaza en la ruta de struct host. El rechazo de publicación no prueba que el layout NativeSharp sea inválido ni cubre todos los tipos o rutas del generador.

**Pendiente:** seguir propagación de layout, metadata y tipos anidados, comprobar alcance de union, campos disjuntos, pack/align, diagnóstico al usuario y alternativas ABI; validar por compiladores host solo en fase autorizada. No se ejecutó ni se afirma compatibilidad C general. LK-381.

### LKF-119 — Publicación y reconstrucción de campos promovidos

**Evidencia:** NSDBG-2343 describe carriers anónimos y aliases con offsets acumulados. ProjectObjectLayoutModel.cpp:1461–1532 crea layouts derivados separados de fields físicos, suma offsets con checkedAdd y conserva tamaño, alineación mínima y datos de bitfield del miembro fuente. Esa fuente confirma la separación local, no prueba que NSMETA, header, PDB y depurador reconstruyan siempre la misma ruta ni que un consumidor de binario distinga ausencia de metadata.

**Pendiente:** seguir resolución de colisiones/accesibilidad, serialización y consumo de carriers anidados, pack, bitfields, origen C/NativeSharp y target; verificar overflow de rutas y diagnóstico frente a símbolos incompletos. No se ejecutó ni se afirma defecto reproducido. LKF-030–031, LK-382.

### LKF-120 — Ancho soportado y metadata de bitfields

**Evidencia parcial:** OopLlvmFieldsAndConversions.cpp:97–152 usa máscaras uint64 y rechaza logicalWidth > 64 en insertBitField; el modelo general reconoce i128/u128 (LKF-116), pero no se ha determinado si el frontend admite esos tipos como base de bitfield o si la ruta es alcanzable. No afirmar defecto ni soporte de bitfields de 128 bits a partir de una sola función. OopLlvmExpressionMachine.cpp:508–535 carga la unidad y marca volatile si corresponde; no demuestra todas las rutas de escritura ni metadata PDB.

**Pendiente:** seguir validación de tipo/anchura, layout y variantes de extracción/inserción, sign/bool/enum y C/Clang; verificar representación en NSMETA y PDB, y evitar lecturas adicionales de MMIO. Diseñar casos futuros por target y optimización. NSDBG-2344, LK-383.

### LKF-121 — Recuperación de argumentos y retornos en ABI Win64 por verificar

**Evidencia:** NSDBG-2345:32–60 y 208–321 describe posiciones ABI compartidas por tipo y el desplazamiento causado por sret en ejemplos extern C. OopLlvmImplementation.h:3086–3149 contiene declaraciones y comentarios de clasificación indirecta y código que agrega atributos sret/noalias/align; no se han seguido todos los invocadores ni validado qué información conserva PDB o recupera el depurador. La disponibilidad de registros al entrar no garantiza recuperación más tarde y el shadow space no prueba que los argumentos se hayan guardado. La documentación histórica refiere pruebas propias que no ejecutamos.

**Pendiente:** contrastar compilador, CodeView/PDB y depurador por dirección y configuración; caracterizar firmas internas, extern C, Clang, thunks, callbacks, varargs, excepciones y optimización. Reportar procedencia y disponibilidad de cada valor; no confundir copia indirecta o sret con asignación, ownership ni fuga. Diseñar validación futura de entrada y salida con símbolos ausentes o parciales. LK-384–385.

### LKF-122 — Huellas de contrato no identifican por sí solas la build

**Evidencia:** NSDBG-2346:165–215 distingue checksum de metadata, fingerprint ABI e interpretación de compatibilidad; :337–351 advierte sobre mezcla de piezas. RuntimeAbiContract.cpp:140–176 construye manifiesto con target, symbol set, convención y símbolos tipados y calcula una huella de 64 bits sobre ese texto. Esta observación parcial no establece la política vigente de todos los verificadores ni prueba que esa huella identifique PE, PDB, fuente o build. El documento describe una certificación histórica no reproducida.

**Pendiente:** comprobar productores y verificadores actuales de NSMETA, contratos y artefactos, sus ventanas y cobertura; registrar por separado integridad, compatibilidad y asociación PE/PDB/metadata/fuente. Una huella ABI diferente no demuestra ruptura, y una igual no demuestra identidad binaria, autenticidad ni compatibilidad de ownership de una API. Considerar símbolos ausentes, versiones futuras y módulos recargados. LK-386.

### LKF-123 — Posible ambigüedad en nombres internos de imports C

**Evidencia parcial:** HirBuildCoordinator.cpp:108–118 sustituye caracteres no alfanuméricos distintos de guion bajo por guion bajo; :134–142 forma el nombre interno de import combinando las versiones saneadas de DLL y símbolo. :985–1013 utiliza ese nombre cuando importRequiresRuntimeBinding está activo y conserva por separado importDllName y cAbiName. Por ejemplo, dos nombres de DLL distintos como a-b.dll y a_b.dll producen el mismo fragmento saneado; esto no demuestra que ambos sean aceptados, alcancen ese camino, colisionen tras las comprobaciones posteriores o tengan consecuencias en el binario.

**Contraste adicional:** BuildSupport.cpp:228–263 marca binding diferido cuando un símbolo aparece en varias bibliotecas y valida el caso sin DLL; OopLlvmThunksAndWrappers.cpp:15–40 usa un conjunto de mangledName para emitir una sola definición. Estas rutas aumentan la relevancia del saneamiento, pero no se siguió collectImportedCLibraries ni la declaración LLVM, de modo que aún no se demuestra que el caso propuesto pase todas las guardas o cause un fallo observable.

**Contraste adicional 2:** BuildAndDeclaration.cpp:693–729 agrupa declaraciones por biblioteca convertida a mayúsculas y símbolo, y registra bibliotecas diferentes para un mismo símbolo; no aplica el saneamiento de HirBuildCoordinator en ese bloque. OopLlvmDeclarations.cpp:115–125 reutiliza una declaración importada cuando functions_ ya contiene el mismo mangledName. Junto con la emisión de una sola definición por ese nombre, esto delimita un mecanismo plausible de conflación, sujeto a confirmar aceptación del literal, alcance, llamadas y otras comprobaciones. No se ha compilado ni reproducido.

**Pendiente:** seguir validación, normalización, deduplicación y mapas desde declaraciones hasta HIR, LLVM, resolución diferida y enlazador; estudiar colisiones entre bibliotecas, símbolos y separadores, casos y firmas. Diseñar prueba futura con proveedores reales y mismo símbolo, manteniendo separados nombre interno, identidad declarada y módulo efectivamente cargado. No se ejecutó ni se afirma defecto reproducido. NSDBG-2347, LK-387.

### LKF-124 — Contrato de memoria de imports externos desconocido

**Evidencia parcial:** HirBuildCoordinator.cpp:993–1021 conserva biblioteca, símbolo, tipos y propiedad declarada de parámetros, pero fija returnOwnership en None en la construcción observada. Esto no prueba que otras capas omitan contratos ni que None signifique necesariamente borrowed; el capítulo histórico NSDBG-2347:317–365 distingue punteros prestados, bloques con liberador específico y firmas ABI sin semántica de vida.

**Pendiente:** seguir contratos y anotaciones disponibles por ruta, incluidas bibliotecas estáticas, imports PE y bindings diferidos; identificar cómo representar dueño, liberador, duración, condición de éxito, capacidad, callbacks y dato desconocido. No inferir fuga ni liberación válida solo desde tipo puntero, nombre o coincidencia de dirección. Coordinar LK-253 y LK-388.

### LKF-125 — Lifetime y cobertura de callbacks externos

**Evidencia:** NSDBG-2348:338–373 separa vida de módulo, contexto y operación, y distingue desregistro de espera de invocaciones activas; :461–479 describe barrera fatal histórica. OopLlvmThunksAndWrappers.cpp:56–74 evita generar thunk de guard para función C directa identificada; :110–151 construye dos catchpads para filtros de excepción y panic y emite trap CallbackExceptionEscape. Esta lectura no acredita que todos los callbacks NativeSharp alcancen esa ruta ni el comportamiento de callbacks extranjeros, APIs arbitrarias o ejecuciones optimizadas.

**Pendiente:** seguir selección y emisión del thunk, conversión desde dirección cruda y ABI, propagación de excepciones y semántica de cada proveedor; modelar registro reentrante, cancelación, drain, contexto, módulo, hilos externos y handles como vidas distintas. No afirmar que unregister o CloseHandle prueba finalización, ni que un puntero de función acredita firma o mantiene cargada la DLL. Diseñar casos futuros de carreras, reuso de direcciones y trap, sin ejecutar en fase 0. LK-389, LK-253, LK-368.

### LKF-126 — Alcance de agregados C en argumentos variádicos

**Evidencia:** NSDBG-2351:233–264 describe una restricción histórica que excluía agregados en la cola de varargs. En la fuente actual, ConversionAndPassKindChecks.cpp:130–136 reconoce records C importados completos sin error diferido; :139–208 los acepta por identidad en classifyCVariadicPromotion, además de promover float a double y enteros estrechos a int. La aceptación en esta función no prueba que todas las rutas semánticas y de lowering admitan todos los records, que structs nativos no C se acepten ni que el transporte Win64 sea correcto en ejecución.

**Pendiente:** seguir aplicabilidad a llamadas directas y punteros de función, typedefs, unions, nested records, clasificación indirecta, alineación, metadata y validaciones adicionales; actualizar diagnósticos y documentación cuando se confirme el alcance. Para LeakKiller conservar tipo fuente, carrier promovido, firma fija/variádica, target y contrato de format/count/sentinel; no inferir tipos de la cola desde el PDB ni el format si falta evidencia. Diseñar casos futuros C/Clang O0/O2 sin ejecutarlos ahora. LK-391, LK-384.

### LKF-127 — Eventos de módulo no prueban protocolo de shutdown

**Evidencia:** DebugEngine.cpp:300–329 registra módulo, solicita símbolos, emite evento Loaded y, en unregisterImage, elimina breakpoints del rango, descarga símbolos y emite Unloaded si existía registro; :919–935 atiende eventos Windows LOAD_DLL y UNLOAD_DLL y continúa. Esta lectura parcial demuestra manejo local de eventos, no captura de cada LoadLibrary/FreeLibrary, conteo del loader, operaciones en vuelo, ownership de handles ni retención de tablas/callbacks. NSDBG-2352:447–453 distingue liberación de una referencia de desmapeo inmediato.

**Pendiente:** verificar ciclo completo y consumidores del depurador, identidad/generación de módulos, PDB, reuso de base y eventos perdidos o sesión adjunta tarde. Modelar dependencias de tablas, callbacks, handles, hilos y destructor como obligaciones del protocolo, no referencias implícitas que mantengan cargada la DLL. Diferenciar llamada FreeLibrary, resultado, liberación de referencia y evento observable de descarga; conservar desconocidos. Planear pruebas futuras con referencias múltiples, cancelación/drain y recarga. LK-392, LK-389.

### LKF-128 — Antecedente de output asm +r sin reproducir

**Evidencia:** NSDBG-2353:471–486 documenta una distribución histórica que aceptaba +r hasta LLVM, pero reporta resultado incorrecto en O0 y terminación del compilador en O2. OopLlvmInlineAssembly.cpp:61–88, fuente actual, trata outputs de memoria por separado y, para otros constraints iniciados por +, conserva el constraint y carga el valor anterior como argumento. Esa lectura no demuestra que el resultado histórico siga presente ni que esa función sea su causa; no se compiló ni ejecutó la sonda.

**Pendiente:** seguir construcción completa de constraints, numeración, inputs tied, verificador y backend LLVM en las versiones activas; diseñar reproducción futura aislada con distintas anchuras, outputs múltiples y O0/O2. Diferenciar aceptación sintáctica, IR válida y ejecución correcta. LK-394.

### LKF-129 — Efectos asm y correlación CodeView por verificar

**Evidencia parcial:** OopLlvmInlineAssembly.cpp:11–47 y 124–143 recopila registros y comprueba coherencia de resúmenes flags/memory con los clobbers; :200–217 inserta .cv_loc si existe debug, source manager y rango válido. Son contratos declarados por el bloque: la lectura no prueba que sus instrucciones no accedan memoria adicional ni que cada instrucción final tenga una ubicación única o se pueda grabar sin pérdidas. El capítulo histórico NSDBG-2353 documenta stepping de su ejemplo, sin ejecución actual.

**Pendiente:** trazar validación y lowering completos, efectos ocultos, instrucciones con memoria implícita, clobbers incorrectos, labels/branches y alcance de símbolos por optimización; modelar memoria no descrita como efecto desconocido, no como inexistente. Separar localización PDB, stepping e historial ejecutado; planear validación futura sin alterar el bloque ni sus constraints. LK-393.

### LKF-130 — Precondiciones de acceso SIMD y prueba de alineación

**Evidencia parcial:** OopLlvmExpressionMachine.cpp:3771–3801 construye carga <4 x i32> y fija alineación 16 para CoreSimdLoadAligned128 o 1 para la variante unaligned; :3813–3858 hace lo correspondiente para store. En esos fragmentos no se observa una comparación dinámica de alineación ni de extensión; esto no descarta guardas previas en otras capas. NSDBG-2354:167–179 describe una sonda histórica desalineada compile-only y un fallo observado en un laboratorio anterior, no reproducidos aquí.

**Pendiente:** seguir reconocimiento exacto de intrinsics, contratos de punteros y todas las guardas previas; contrastar longitud disponible, offsets, overflow, alias, alineación efectiva de allocator/campo y diferencias entre fuente, LLVM y máquina. Para LeakKiller distinguir alineación desconocida de violación probada, acceso vectorial completo de carriles válidos y operación no alineada de acceso sin bounds. Diseñar casos futuros de tails, guard pages, O0/O2 y targets sin ejecutar ahora. LK-395.

### LKF-131 — Completitud y columna de frames inline

**Evidencia parcial:** DbgHelpSymbolProvider.cpp:508–556 limpia error, solicita cantidad de frames, limita el bucle a 64 y termina silenciosamente ante fallo de consulta, símbolo o módulo; no devuelve una señal explícita de truncamiento en esa interfaz. En :550–553 asigna lineDisplacement + 1 al último campo de SourceLocation, cuyo nombre es column en Symbols.h:50–56. Falta verificar semántica exacta del desplazamiento de DbgHelp y los consumidores antes de concluir que la columna sea incorrecta. DebugEngine.cpp:2011–2028 inserta frames lógicos asociados a la misma identidad de módulo y los marca Optimized. No se probó con PDB ni sesión real aquí.

**Pendiente:** documentar completitud y motivo de parada, límite de 64, ausencia de inline trace frente a error, procedencia de línea y columna y estado de PDB; contrastar documentación de API, adaptadores y clientes antes de atribuir defecto. En reportes preservar frame físico y cadena lógica como información diferente de eventos de ejecución, y marcar desconocida una columna no demostrada. Diseñar validación futura con inline anidado, símbolos parciales y O0/O2. LK-396 y LK-224.

### LKF-132 — Alcance del clasificador de efectos MIR por establecer

**Evidencia:** MirEffectClassifier.cpp:45–94 clasifica división, resto, shifts y bounds como mayTrap y conversiones float→entero según tipo; :97–137 registra lecturas, escrituras, llamadas, asignación y deallocación; :141–170 trata opcode no reconocido como estructural y sólo permite eliminar resultado sin uso si carece de efectos, trampa, lectura, escritura y condición estructural. Ésta es una implementación concreta de mir::MirInstruction: no se han trazado sus invocadores ni demostrado que cubra el MIR OOP, todo LLVM o todas las construcciones del lenguaje. NSDBG-2356 describe un laboratorio histórico no reproducido.

**Contraste adicional:** DeadCodeEliminationPass.cpp:19–37 cuenta usos por operandos y entradas PHI; :56–106 itera funciones y bloques de mir::MirModule y elimina un resultado sin usos sólo si removableWhenUnused lo permite. PassManager.cpp:159–165 agrega ese pase a addDefaultPipeline junto con simplificación PHI, propagaciones y simplificación de control. Esto identifica composición y consumidor concretos, pero no prueba que el pase procese ProjectOopMirModel ni que todos los builds llamen ese pipeline.

**Pendiente:** identificar rutas reales, tipos de IR, pases consumidores, opcode nuevos, excepciones, atomics/volatile y contratos de llamadas; comparar con efectos del lenguaje y lowering en cada ruta sin atribuir una clasificación a otra. Para LeakKiller separar evidencia de semántica fuente, IR previo y código final, y clasificar observación dinámica como parcial según optimización e instrumentación. Diseñar casos futuros de traps, cleanup y ausencia de hooks. LK-397.

### LKF-133 — Esquema histórico de proyecto y configuración efectiva

**Evidencia:** NSDBG-2357:58–77 dice que sólo Name y OutputType son propiedades de proyecto en la distribución histórica. ProjectLoader.cpp:275–307 y 353–393 en la fuente actual procesa, entre otras, OptimizationLevel, LibraryReferenceRoot y CompilerJobs; :447–485 conecta esas funciones con PropertyGroup. En :1214–1235, opciones explícitas tienen prioridad sobre el root del descriptor; si no hay root se usa archivo local y eventualmente fallback. Es evidencia estática de esta rama, no una lista completa de propiedades ni validación de un proyecto ejecutado.

**Pendiente:** inventariar schema y precedencia efectivos, globs, fuentes, grafo, referencias, SDK, opción CLI/perfil/configuración y cachés desde las mismas APIs del compilador; detectar archivos y opciones ausentes o ambiguos, capturar versiones y hashes. No asumir que enumerar .ns del directorio reproduce el proyecto ni que los ejemplos históricos describen la rama actual. LK-398.

### LKF-134 — Alcance de verificación conjunta DLL/PDB

**Evidencia parcial:** DriverLibraryCommands.cpp:374–419 omite la comprobación si no se proporciona PDB; si se proporciona, exige DLL, registro RSDS, PDB aceptado por PdbVerifier, GUID/age coincidentes, basename portátil coincidente y presencia de módulos, públicos, fuentes y checksums. PdbVerifier.cpp fue leído completo previamente para LKF-066; su hash se cotejó de nuevo y no cambió. En esa lectura la presencia de checksums se detecta mediante búsqueda heurística de patrón, y quedan pendientes las demás comprobaciones del conjunto y los riesgos de límites allí descritos. La presencia de checksums no demuestra que archivos fuente disponibles coincidan, y GUID/age no es una prueba de integridad criptográfica de binario, PDB y fuente. NSDBG-2358 describe una certificación histórica no reproducida.

**Pendiente:** seguir verificadores y diferencias entre builds, PDB parciales, símbolos externos de Clang y política del depurador; decidir si una validación de publicación estricta se aplica, se informa o no corresponde a análisis de binario sin fuente. Separar no suministrado, inválido, identidad discordante y superficie incompleta; registrar hashes y checksum de fuente efectivamente verificados. LK-399, LK-230, LK-290.

### LKF-135 — Posible asociación errónea de runtime en paquetes mixtos

**Evidencia parcial:** ProjectCommands.cpp:518–528 agrega library y metadata para cada entrada del manifiesto, pero sólo agrega runtime cuando está presente. En :794–827 makeGeneratedProject separa esos artifacts en tres vectores compactos y asigna runtimes[index] a libraries[index]. Si el orden conservado de un paquete válido coloca una biblioteca sin runtime antes de otra con runtime, esa lógica parece asociar la DLL posterior a la primera referencia. Todavía falta seguir orden/deduplicación de artifacts, validadores, lectura del lock y flujo completo para establecer alcanzabilidad y consecuencias; no se compiló ni ejecutó una reproducción.

**Contraste adicional:** ProjectCommands.cpp:580–603 serializa artifacts en orden y :1137–1176 reconstruye la lista del lock en el mismo orden; :1688–1716 obtiene el plan de restore/build y llama makeGeneratedProject antes de cargar el grafo. Esto acota un camino plausible desde el plan hasta el descriptor generado. La aceptación de un manifiesto mixto, variaciones de restore y el diagnóstico posterior siguen sin verificarse en ejecución.

**Pendiente:** verificar contrato y orden del manifiesto, materialización/lock, asociación explícita de cada terna y rechazo o diagnóstico posterior; diseñar casos futuros de estática→DLL, DLL→estática, múltiples DLL y runtimes opcionales. Para LeakKiller no inferir la pertenencia de módulos de listas compactas por índice sin verificar las asociaciones efectivas. NSDBG-2359, LK-400.

### LKF-136 — Sidecars adicionales de paquetes por inventariar

**Evidencia parcial:** ProjectCommands.cpp:393–421 contempla un sidecar opcional de productos precompilados asociado a metadata y :424–453 otro opcional ThinLTO para bibliotecas estáticas; :540–548 los agrega desde el manifiesto. NSDBG-2359:1–50 y 285 describe categorías históricas library/metadata/runtime/file, por lo que no basta esa lista para reconstruir la resolución de la rama actual. No se inspeccionó el codec ni se ejecutó restore.

**Pendiente:** seguir rutas convencionales, lectura de lock, hashes, compatibilidad, cache y uso real de cada sidecar, incluyendo LTO; registrar qué evidencia y qué optimizaciones dependen del archivo y versión. LK-400.

### LKF-137 — Publicación Content: rollback y origen ausente (ver también LKF-267; mismo riesgo de rollback)

**Evidencia parcial:** ProjectCommands.cpp:910–945 renombra el destino a backup y el staging al destino; si falla el segundo rename, intenta restaurar el backup pero no examina rollbackError antes de devolver el fallo original. Si no puede borrar el backup después de publicar, informa fallo aunque el nuevo Content ya fue instalado. En :948–960, si el directorio fuente Content no existe, la función retorna éxito sin tocar el destino anterior. Estas rutas son observaciones estáticas; no se ha provocado un fallo de filesystem ni determinado si el comportamiento de origen ausente es el contrato deseado.

**Pendiente:** definir política de origen ausente y salida previa, informar claramente publicación parcial, fallo de rollback y ruta de backup preservada; comprobar concurrencia, interrupción y estado binario/Content en fases separadas. Para grabaciones, identificar archivos y configuración realmente consumidos, no inferir que binario y Content son una transacción conjunta ni que un fallo del build preservó todo el bundle. Diseñar pruebas futuras de fallos de rename, rollback y eliminación, sin ejecutarlas ahora. NSDBG-2360, LK-401.

### LKF-138 — Argumento vacío en línea de comandos del depurador

**Evidencia estática:** DebugEngine.cpp:64–66 devuelve el argumento sin comillas si no contiene espacio, tabulador ni comillas; una cadena vacía cumple esa condición y se devuelve vacía. :93–101 inserta un espacio antes de cada argumento y concatena el resultado; por tanto, no emite un token entrecomillado para ese elemento. :679–709 envía esa línea a CreateProcessW. No se ejecutó una sesión ni se verificaron posibles normalizaciones en consola, DAP o el parser del programa destino, así que no se afirma aquí un fallo reproducido en todas las rutas.

**Contraste adicional:** DapSession.cpp:1591–1619 acepta cada entrada string de launch.args y la añade a LaunchOptions; Utf.cpp:11–15 devuelve cadena vacía para entrada vacía. Esto demuestra que una cadena JSON vacía puede llegar vacía al constructor de línea en esa ruta, sujeto a las condiciones del lanzamiento. Aún no se ejecutó el programa receptor ni se siguieron todas las variantes de cliente.

**Pendiente:** seguir LaunchOptions desde consola y comparar reglas de quoting y parseo reales del programa; preparar casos futuros con argumento vacío aislado y entre otros, espacios, comillas, barras finales, Unicode y programas con parsers distintos. Para reproducción registrar vector solicitado y forma efectiva sin exponer secretos por defecto; detectar divergencia en lugar de asumir equivalencia. NSDBG-2361, LK-402.

### LKF-139 — Alcance de redacción y cobertura de NSDIAG

**Evidencia:** DiagnosticBundle.cpp:37–44 redacta únicamente argumentos que empiezan exactamente por --api-key=, --password=, --secret= o --token=; :83–90 y 143–144 aplican esa función a acción y argumentos. :146–148 incorpora stdout y stderr completos escapados, no redactados en este archivo. :46–80 recopila grafo/proyectos/sources y referencias library/metadata/runtime; ante error de grafo registra sólo proyecto raíz cuando existe. Esto no demuestra que otras capas no redacten datos antes de entregarlos, pero el bundle no debe considerarse anonimizado por defecto. No se generó un bundle.

**Pendiente:** trazar entradas, streams, flags separados, mayúsculas, rutas, variables, PDB y paquetes/sidecars; definir política explícita de captura, redacción, permisos y consentimiento, manteniendo evidencia de lo omitido. Evitar copiar NSDIAG a reporte portable sin revisar su contenido; comprobar errores de hash/graph e inputs realmente incluidos. Diseñar validación futura con datos sintéticos. NSDBG-2362, NSC-048, LK-403.

### LKF-140 — Vistas procedurales y MIR OOP no equivalentes

**Evidencia:** NSDBG-2363:7–24 y 412–428 distingue dumps procedural single-file de vistas OOP de proyecto en la distribución histórica. ProjectOopMirFunction.h:33–101, fuente actual, conserva efectos, ABI, ownership de parámetros/retorno, capturas, regiones cleanup, flujo de excepción y body de nodos propios. ProjectOopMirNode.h:30–71 y 100–104 muestra syntax, tipos, regiones, borrow y procedencia de allocator en nodos. Estas estructuras son distintas de mir::MirModule del clasificador NSC-046/047; la lectura no demuestra qué CFG público existe para OOP ni que todas las rutas tengan metadata completa.

**Pendiente:** inventariar APIs de análisis, construcción de flujo normal/excepcional/suspensión, datos de source, vida y contratos por versión; comprobar persistencia, invalidación y acceso sin depender de dump textual. Diseñar análisis de ciclos/progreso con límites, llamadas indirectas y conclusiones desconocidas; no aplicar directamente CFG/SSA procedural a OOP ni prometer completitud interprocedural. LK-404, LK-284.

### LKF-141 — Conteo de frames no proporciona stack simbolizable

**Evidencia:** NativeSharp.Core.Diagnostics/Debug.ns:10–19 y 33–39 declara que CaptureStackFrameCount expone sólo cantidad y descarta las direcciones del buffer interno. Es la API de esta clase revisada, no prueba que ninguna otra ruta del runtime o depurador pueda capturar direcciones. NSDBG-2364:185–204 describe la misma limitación histórica; su sonda y el minidump mencionados no se ejecutaron aquí.

**Contraste adicional:** WindowsRuntimeBuilder.cpp:5208–5239 emite buffer local de 62 punteros, llama RtlCaptureStackBackTrace con máximo 62 y retorna sólo el conteo; no publica direcciones ni un indicador explícito de que pudo alcanzarse el límite. No se ejecutó ni se ha establecido la semántica de todas las capas de unwind.

**Pendiente:** seguir opciones del depurador para stack físico, unwind e inline, y posible API adicional de captura con buffer caller-owned, señalando plataforma, contexto, truncamiento, fallos y seguridad en contextos de crash. Un conteo no debe serializarse como stack trace ni inventar direcciones; distinguir minidump de crash y snapshot viva, con cobertura de memoria explícita. LK-405.

### LKF-142 — Aritmética de capacidad de List por contrastar

**Evidencia parcial:** List.ns:98–112 calcula items.Length * 2, selecciona capacity si el resultado es menor, reserva, mueve elementos y reemplaza el buffer; :118–133 calcula (items.Length * 9) / 10 para TrimExcess; :136–138 llama EnsureCapacity(count + 1). No se observa una guarda explícita de overflow en esos bloques. El fallback de EnsureCapacity puede compensar algunos resultados de la duplicación, y no se han establecido los rangos realmente alcanzables, semántica exacta de overflow, límites de array ni efectos de fallo; no se afirma un defecto reproducido.

**Pendiente:** seguir invariantes count/capacity, límites del runtime y operaciones Add/Insert/AddRange, política de fallo y movimiento parcial, invalidación de vistas/iteradores y semántica signed por target. Diseñar pruebas futuras con modelos pequeños o límites artificiales y fallos controlados, evitando reservas enormes; registrar posible versus probado. NSDBG-2367, LK-407.

### LKF-143 — Acuse DAP cancel sin cancelación de trabajo

**Evidencia estática:** A:/nsc-debugger/src/dap/DapSession.cpp:2375 responde éxito y objeto vacío al comando cancel sin aplicar cancelación en esa rama. A:/nsc-debugger/tests/DapTests.cpp:224–225 y 292–294 espera que no se anuncie capacidad de cancelación concurrente. El archivo de implementación no se leyó completo en este contraste; no se ejecutó la prueba ni se concluye que otras vías de interrupción como pause/terminate sean equivalentes. SHA-256 DapSession.cpp: 549697a1d2a7eb0dada1649a2060100b34719ebbafa1af7961bdcdc892ef6913.

**Pendiente:** seguir dispatch y ejecución de solicitudes, bloqueo y concurrencia, y definir estados aceptada, en curso, cancelada, completada y no cancelable; distinguir acuse de cancelación efectiva. No anunciar ni inferir cancelación real para análisis o grabación desde el éxito de esta respuesta. Diseñar pruebas futuras con operación activa y carreras de finalización. LK-237, NSDBG-2392.

### LKF-144 — Capacidad y alineación de watchpoints del motor AMD64

**Evidencia estática:** A:/nsc-debugger/src/core/DebugEngine.cpp:1619–1625 acepta para data watchpoints tamaños 1, 2, 4 u 8 alineados a su tamaño; :1640–1647 rechaza si los cuatro slots están ocupados. Tests/DebugEngineTests.cpp:353–368 espera rechazo del quinto y de una dirección desalineada. Esto limita puntos simultáneos y rangos observables por este mecanismo; no implica ausencia de otros mecanismos de instrumentación. No se ejecutó la prueba ni se releyó completo DebugEngine.cpp en este contraste. SHA-256 de DebugEngine.cpp: ab3955e3dd8b86f37901375926d6bc1b7a37b211bd98976db51e59e88bb7c3d6.

**Pendiente:** trazar programación y restauración por hilo, tipos de acceso, nuevos hilos y errores de instalación; investigar política compartida con breakpoints del usuario y pérdida de cobertura cuando se agotan slots. Diseñar selección y degradación explícita, jamás presentar watchpoints finitos como cobertura completa de memoria. Validación futura sin interferir con trabajo de fase 0. NSDBG-2393, LK-419.

### LKF-145 — Límites del ejecutor JSON de escenarios de prueba

**Evidencia estática:** A:/nsc-debugger/tests/DebuggerScenarioRunner.cpp:831–835 espera waitForStop y sólo después calcula el tiempo; :299–304 compara maxMilliseconds tras retornar, por lo que esa comprobación por sí sola no interrumpe una espera bloqueada. :254 compara línea y nombre final de archivo, no ruta completa. :335–446 y :449–481 permiten normalizar duplicados o variables ausentes declaradas como problemas del productor antes de comparar nombres exactos; :576–619 acepta valores conocidos alternativos explícitos. Estas son limitaciones del ejecutor de pruebas, no prueba de defectos vigentes del depurador o productor. No se ejecutó. SHA-256: 87a4a05dcaa050d6588a31f1fab4c2cf9d5e1463600c9f8ed36c5c5955f77a92.

**Pendiente:** revisar escenarios concretos y la implementación del motor para identificar causas y estado actual de cada excepción; preservar resultados crudos frente a normalizados. Para instrucciones y grabación de LeakKiller definir un límite de espera realmente interrumpible, identidad de fuente y validación de checksum, y no inferir fidelidad de una aprobación normalizada. Preparar casos futuros de espera indefinida, nombres de archivo homónimos y variables ausentes/duplicadas. NSDBG-2394, LK-422.

### LKF-146 — Límites de snapshots del cliente de pruebas profundas

**Evidencia estática:** tests/deep/nsdbg_deep.py:341–344 solicita hasta 64 frames; :397–415 guarda snapshots puntuales con árbol de cuatro niveles y sólo los primeros 12 frames en el reporte. :360–370 indexa variables por nombre en un diccionario, por lo que nombres repetidos se sobrescriben en esa vista. :1380–1386 conserva los últimos 200 renglones del log del adaptador. El formato mostrado no agrega un indicador de truncamiento para esos límites. Son características del harness de prueba, no una limitación demostrada de toda la API del depurador. No se ejecutó. SHA-256: 8b1cff7cd8e4a1bd51595bdc397ac048af0f39c22844b0adebd7d19e36da8201.

**Pendiente:** para LeakKiller diseñar snapshots con identidad única y multiconjunto de símbolos, profundidad y presupuesto declarados, número real cuando esté disponible, truncamiento y fallos explícitos; diferenciar estado en una parada de historial completo. Evitar que una vista de prueba o nombres duplicados oculten datos. Preparar casos futuros con recursión, árboles profundos y símbolos homónimos. NSDBG-2395, LK-423.

### LKF-147 — Alcance del script auxiliar de líneas PDB

**Evidencia estática:** tests/deep/pdblines.py:10–17 usa ruta fija de llvm-pdbutil, consume stdout sin comprobar returncode y reconoce procedimientos mediante texto con segmento 0001; :20–27 filtra fuente por subcadena y extrae sólo el patrón textual indicado; :29–34 asigna propietario por primer rango coincidente y muestra línea anterior por orden de dirección. No comprueba identidad PE/PDB ni modela explícitamente segmentos múltiples, inline o procedencia inequívoca. Son límites de este auxiliar, no del proveedor PDB completo. No se ejecutó. SHA-256: 632a477c21977a95af3b63b47b1d588ed42ffada374af17f8b1f2f1de176cbd8.

**Pendiente:** para LeakKiller contrastar identidad y mapeo de segmentos/RVA con APIs reales, conservar ambigüedad de fuentes y frames inline, errores de herramienta, rangos y checksum; no interpretar línea anterior ordenada por dirección como ejecución previa. Diseñar casos futuros con fuentes homónimas, segmentos múltiples, fallos y símbolos parciales. NSDBG-2396, LK-424.

### LKF-148 — Caché y alcance del auxiliar de consultas PDB

**Evidencia estática:** tests/deep/pdbq.py:18–27 usa una ruta fija de llvm-pdbutil, reutiliza archivos de texto según fecha de modificación del PDB y escribe stdout sin verificar returncode ni stderr. :30–58 parsea patrones textuales y recorre referencias hasta profundidad configurable (predeterminada tres en :66), con conjunto seen y supresión de índices simples; :68–88 imprime resultados sin validar identidad del PE. Una caché puede no reflejar cambio de herramienta o contenido con fecha conservada y una falla podría producir salida reutilizable; son posibilidades deducidas de este auxiliar, no reproducidas ni atribuibles al proveedor PDB de producto. No se ejecutó. SHA-256: 963bc8736cb6090de15c35b821ce050b74238977dde754cf7290d2ee44e174f2.

**Pendiente:** si LeakKiller usa caché, incorporar identidad y hash de artefacto, versión/opciones del lector, validación de errores y escritura atómica; marcar truncamiento, tipos faltantes y compatibilidad. No tratar salida textual o fecha como identidad PE/PDB. Diseñar casos futuros de PDB sustituido con misma fecha, error de herramienta y ciclos de tipos. NSDBG-2397, LK-425.

### LKF-149 — Sonda de pausa desalineada con la firma del harness

**Evidencia estática:** tests/deep/probe_pause.py:5 construye d.Harness con dos argumentos posicionales, mientras tests/deep/nsdbg_deep.py:548 define Harness.__init__(self, adapter, output, pdbutil) sin valor predeterminado para pdbutil. La llamada mostrada no satisface la firma actual, antes de iniciar la sesión. No se ejecutó la sonda y no se extrapola este desajuste a la capacidad pause del depurador. SHA-256 de probe_pause.py: 56e631a8ece1fae6768b1093f3eb69fb47d2f557a10ab8dff27255ab7042abc6.

**Contraste adicional:** tests/deep/probe_vtable.py:5 repite la construcción de Harness con dos argumentos frente a los tres exigidos. SHA-256 de probe_vtable.py: 91a5ffcee67b748d4496f44bfaf99f309670d448788bcacef0c374c6a5c80f97. No se ejecutó.

**Pendiente:** corregir o retirar la sonda sólo en una tarea autorizada de mantenimiento, y verificar con configuración y artefactos reales antes de usarla como evidencia; no incorporar sus comentarios o finalidad como resultado observado. NSDBG-2398, NSDBG-2395.

### LKF-150 — Pérdida de detalle en adaptador de archivos del corpus

**Evidencia estática:** tests/fixtures/KrSystemIo.ns:113–137 devuelve cero si falla la apertura de directorio y termina el conteo cuando TryNext devuelve false sin distinguir en ese bloque fin de enumeración de error. :140–166 vuelve a abrir el directorio y limita resultados a destination.Length; entre la consulta de conteo y la segunda enumeración puede cambiar el contenido. :46–82 devuelve null ante varias fallas de lectura y admite lectura corta al recibir cero. Son decisiones observadas del adaptador de pruebas; la semántica detallada de TryNext/TryRead y sus códigos no se ha contrastado, y no se afirma un defecto en NativeSharp.System ni se ejecutó el corpus. SHA-256: 7b89bc3d2f957c9034627624541c7f1522c1f2363c4b98565967f74e0c7a0db0.

**Pendiente:** trazar contratos de NativeFile y DirectoryIterator, distinguir fin, fallo y lectura parcial; registrar entradas externas y cobertura incompleta, sin afirmar reproducción determinista de una enumeración mutable. Preparar casos futuros de cambios entre llamadas, errores y cierre de recursos. NSDBG-2403, LK-401 y LK-429.

### LKF-151 — Antecedente de ubicación CodeView de parámetro out

**Evidencia documental:** tests/scenarios/kr/02.07-atoi.json:36–45 declara para number un valor esperado 28, una alternativa conocida 0 y atribuye la diferencia a NSC 2.0 CodeView out-parameter location. El runner NSDBG-2394:576–619 acepta el valor alternativo declarado como problema conocido. Esta configuración no acredita que el fallo siga presente ni demuestra su causa: no se ejecutó ni se inspeccionó el PDB correspondiente aquí. SHA-256 del escenario: 2b708c4b7c93b3d063a99ac10bfba452f664358ef7972499faccb710ccab8214.

**Antecedentes adicionales:** 04.02-atof-console.json:34–43 declara esperado 3.13 y alternativa 0; 04.02-running-total-console.json:19–29, 41–51 y 72–82 declara 3.5 o 2 frente a alternativa 0, atribuyendo la ubicación del local del caller después de out. Hashes SHA-256: 04.02-atof-console.json 9319d59149358988736fd000218037d9cf2ea2642b4a55004484ef8a60013993, 04.02-running-total-console.json 2f30f0bb93e3f0826270cabb243d68b12bbf0cd0086f4aab4c4df9498967b8fc. Son configuraciones no ejecutadas aquí.

**Caso relacionado distinto:** 05.02-getint.json:95–105 declara esperado 42 y alternativa 0 para un local del caller después de escritura por puntero; atribuye el antecedente a CodeView. Es una vía distinta de escritura indirecta y no se ejecutó ni se ha demostrado causa común. SHA-256: a820cf5e73e53a599a17b8a4b7ae2e824b23aedf3669a9a9fe494851e2d9b0b9.

**Pendiente:** reproducir con herramientas y permisos de validación futuros, identificar productor y versión, comparar instrucciones, almacenamiento y rangos CodeView antes/después de la llamada, y retirar la excepción sólo tras evidencia. Los reportes de LeakKiller deben conservar valor crudo, disponibilidad y procedencia, no sustituirlo por el esperado. NSDBG-2427, LK-421, LK-422.

### LKF-152 — Antecedentes de argumentos no disponibles en CodeView

**Evidencia documental:** tests/scenarios/kr/05.07-calendar.json:43–50, 98–105 y 118–123 declara valores esperados para argumentos literales o locales y alternativa conocida <optimized out>, atribuida a ubicaciones CodeView de NSC 2.0. 05.11-delegate-qsort.json:40–54 declara igual alternativa para left/right y la atribuye a ubicaciones en llamada con delegate. El runner acepta esos valores alternativos declarados; son antecedentes de escenarios, no reproducción actual ni demostración de una única causa. No se ejecutaron ni se inspeccionaron sus PDB. SHA-256: 05.07-calendar.json f4b7c9372390d025d9c67a78d409d4dadcfe5062482bf47032726f35d479ba2f, 05.11-delegate-qsort.json 50353e9541902992a4a74063dfacbbc5900b825248e5813b5d7d364eac383c71.

**Pendiente:** contrastar configuraciones, PDB y rangos de ubicación con registros/stack en los puntos y frames correspondientes; distinguir ausencia real, optimización y ubicación no reconocida. Conservar disponibilidad y procedencia sin sustituir por valor esperado. Preparar validación futura separada para literal, caller-local y delegate. NSDBG-2446, NSDBG-2451, LK-421, LK-422.

### LKF-153 — Antecedente de locales duplicadas en rangos CodeView

**Evidencia documental:** tests/scenarios/kr/06.03-struct-array.json:24–35 y 60–71 declara una aparición esperada y dos observadas para i, atribuidas a rangos solapados de for en CodeView NSC 2.0. El runner NSDBG-2394:335–393 normaliza el conteo conocido antes de comparar el scope; eso no establece el estado actual ni la causa real. No se ejecutó ni se revisó el PDB del fixture. SHA-256: e8c162d2143e1a06fa82e22e57c5760a676f28b137eeeb5a2105d7c544903efa.

**Pendiente:** contrastar rangos de scope y location records con instrucciones, iteraciones y nombres de símbolo, verificar si son dos variables distintas o duplicación, y reproducir con versión exacta cuando se autoricen pruebas. En reportes conservar multiplicidad e identidad individual, evitando eliminar entradas por coincidencia de nombre. NSDBG-2453, LK-421, LK-423.

### LKF-154 — Diagnósticos suprimidos al alcanzar el límite de errores

**Evidencia estática:** include/nativesharp/Diagnostics/DiagnosticEngine.h declara límite predeterminado de 100 errores; src/Diagnostics/DiagnosticEngine.cpp:11–12 convierte un límite cero a uno, :16–103 contabiliza errores y :254–263 suprime cualquier severidad después de alcanzar el límite, incrementando suppressedDiagnosticCount. La fusión :158–221 incorpora supresiones de fragmentos además de las que ocurren al agregar entradas. SHA-256: 99b7bccf86210583671cd676409c9e64b9441409f1d94d0a851c782a9498e892. No se ha comprobado qué configuración y fases usan cada motor ni qué metadatos llegan a cada salida.

**Impacto y pendiente:** una lista de diagnósticos puede ser parcial, incluso en advertencias y notas. Trazar configuración, shards, propagación y serialización de conteo y límite; LeakKiller debe indicar cobertura desconocida o incompleta cuando corresponda, sin considerar la ausencia de un aviso como prueba de que no existe. Preparar validación futura de límites y fusión. NSC-060, NSC-065 y LK-433.

### LKF-155 — Vida útil no garantizada por lectura de accesores owned

**Evidencia declarativa:** include/nativesharp/Frontend/ClassMemberSymbol.h:153–188 documenta como diseño que el setter de un accesor owned transfiere propiedad, mientras el getter entrega una referencia simple no propietaria sin garantía formal de lifetime; el alias puede quedar colgante si sobrevive al contenedor. Esto es un límite documentado del modelo, no evidencia de una fuga o defecto particular. SHA-256: 755075cc8ecce0cdde5443cbc09463fc611bef693d2b92dbdb4e6355be6187ed. Falta contrastar construcción semántica, lowering, metadata y casos reales.

**Impacto y pendiente:** distinguir alias simple de borrowed con lifetime probado; seguir vida del objeto, transferencias, invalidación y rutas excepcionales, conservando incertidumbre cuando falte procedencia. Diseñar detección estática y dinámica futura de uso tras destrucción sin asumir que todo getter genera una fuga. NSC-064, LK-404 y LK-432.

### LKF-156 — Parámetros sin contrato explícito de lifetime

**Evidencia declarativa:** include/nativesharp/Frontend/Internal/MethodBody/ExpressionResult.h:36–51 distingue un owner recién creado de una transferencia y comenta que un parámetro sin calificador no rastrea la vida del valor. El mismo archivo :63–82 declara banderas para préstamos de temporales conocidos. SHA-256 actual: 52fbe1834902fc76b6df2188221dae94cdd826dc5cafee96901d8c862cd71904. El comentario no demuestra la cobertura efectiva de todos los llamadores: falta contrastar classifyOwnershipArgument, el análisis de llamada, metadata y lowering.

**Impacto y pendiente:** no inferir transferencia ni lifetime seguro por aceptar una llamada; modelar fuente fresh, alias sin calificar y temporales por separado, y conservar riesgo desconocido cuando falte contrato. Preparar casos futuros con escape y limpieza. NSC-072, LK-404, LK-432 y LK-433.


**Contraste NSC-404:** OwnershipArgumentClassification.cpp:91–175, SHA-256 3f0089ffcff4902e3efbd34ff1491c8424cc0146c7ee8aa7f1696751ac57d1a0, devuelve Ok para tipo no ownable y acepta owner recién creado en el caso sin calificador, pero rechaza transferencias y borrowed en ese caso; su propio comentario :152–170 indica que ese parámetro no rastrea lifetime. Esto describe solo la clasificación local de compatibilidad: un diagnóstico separado puede modificar el resultado de la llamada (LKF-166). Contrastar llamadas, constructores y escape antes de decidir el alcance efectivo; no se ejecutaron pruebas.

### LKF-157 — Progreso detallado de genéricos condicionado en código fuente

**Evidencia estática:** include/nativesharp/Frontend/GenericTelemetry.h:3–5, 29–40 y 78–90 coloca CompilationEvent, ProgressSink y reportProgress bajo #if 0; :18–25 y :33–35 describen medición opcional y detalle separado. SHA-256: 14422e0069a759e0593544b43a92e5dd9c91fc429a83787e8f60a0611f825877. Esto sólo establece el estado de esta interfaz en la revisión: no demuestra ausencia de otras fuentes de eventos ni comportamiento de binarios preexistentes.

**Impacto y pendiente:** no depender de esos eventos para reconstruir o grabar ejecución; inventariar fuentes activas y disponibilidad por versión, proceso y configuración; distinguir telemetría de compilación de eventos de ejecución. NSC-071 y LK-408.

### LKF-158 — Accesos crudos sin comprobaciones declaradas

**Evidencia declarativa:** include/nativesharp/Frontend/MethodCoreOperationKind.h:44–62 distingue ArrayElement de RawElement; su comentario describe RawElement para arreglos fijos y punteros crudos sin comprobación de límites ni nulidad, a diferencia del arreglo nativo con cabecera. :135–146 declara operaciones separadas para stackalloc, reserva escalar cruda y aritmética de punteros. SHA-256: de5a9f8c922a29eb6081e552d4802d0c81bda14599478e7b4f4a2c3ad5957952. La enumeración y comentarios no prueban la ruta efectiva de todos los programas ni el estado de cada backend; contrastar análisis, HIR/MIR y emisión.

**Impacto y pendiente:** LeakKiller debe diferenciar almacenamiento fijo, stack, heap y puntero sin extensión conocida; comprobar límites y nulidad sólo donde existan datos suficientes e indicar desconocido cuando falte procedencia o capacidad. Diseñar casos futuros de uno-past-end, índices negativos y overflow sin tratar la ausencia de diagnóstico del compilador como seguridad. NSC-080, LK-404 y LK-435.

### LKF-159 — Alcance declarado de exhaustividad de catch

**Evidencia declarativa:** include/nativesharp/Frontend/MethodExceptionBinding.h:51–58 conserva filtros, tipos de error coincidentes y un indicador exhaustive; su comentario define la exhaustividad respecto a los errores declarados que aún pueden alcanzar el límite del try. SHA-256: b776641c59bcef914254fd30c399b9376cbe4084c0c6f8687f34ac30df90fca3. No se ha contrastado la construcción de esa lista ni el tratamiento completo de FFI o fallos ajenos al contrato.

**Impacto y pendiente:** no presentar ese indicador como prueba de captura universal ni de limpieza en todos los fallos. Trazar filtros, propagación, errores no declarados, FFI y rutas de cleanup; conservar el dominio de exhaustividad y la incertidumbre en el reporte. Diseñar validación futura. NSC-086, LK-436.

### LKF-160 — Aproximaciones declaradas de origen de punteros y vistas

**Evidencia declarativa:** include/nativesharp/Frontend/MethodLocalSymbol.h:82–90 describe pointerValueOrigin como unión sin sensibilidad de flujo del origen menos seguro de las asignaciones. :183–193 describe viewRootLocalName como análisis sintáctico de nombres, no como seguimiento exacto de punteros en ejecución. SHA-256: 0f50dd5ffc6421a6a262791a92cc1fe02bd5e578ca86f50b26868c9bb62f4360. Son declaraciones y comentarios de una estructura; la cobertura actual, precisión y consumidores requieren contraste. No se ha demostrado un falso positivo ni falso negativo particular.

**Impacto y pendiente:** preservar procedencia y grado de aproximación en reportes; no convertir un nombre ausente, una unión conservadora o un origen desconocido en prueba de seguridad. Trazar asignaciones, alias, joins y escapes con casos futuros que distingan análisis sintáctico, flujo y observación dinámica. NSC-096, LK-437–438.

**Contraste parcial adicional:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:1066–1086, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385, sólo registra viewRootLocalName cuando el campo está vacío; investigar reasignaciones, joins, invalidaciones y consumidores antes de afirmar que una raíz anterior se utiliza indebidamente.

### LKF-161 — Identidad de fuente y disponibilidad por etapa

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectCompilation.h:154–156 admite un proveedor de texto en memoria cuyo nullopt recurre al archivo de disco; :187–214 expone etapas y etapas completadas, y :246–260 devuelve punteros a modelos. El índice ProjectCallableBindingIndex.h:11–16 y :22–34 describe claves por puntero de sintaxis y callable, ignorar sintaxis nula y conservar sólo el binding más reciente por clave. Hashes SHA-256: ProjectCompilation.h 27c2d4b77e54bc53312c81cd1f586616ebb1335eb4b75638b1214894a6d72e1b; ProjectCallableBindingIndex.h 4d5cca6b988db32f530c3eeeed18748e2c9d0088ca4011b362aa0500fa228df9. Son contratos de interfaz; falta contrastar puntos de llamada y ejecución.

**Impacto y pendiente:** la ruta en disco no identifica necesariamente el texto analizado; los punteros y ordinales son internos y el índice no es historial. LeakKiller debe registrar contenido efectivo por hash y etapas realmente completas, con procedencia y cobertura parcial; seguir vida útil, reemplazos y remapeos antes de diseñar persistencia. NSC-127, NSC-129 y LK-441.

### LKF-162 — Consulta preliminar de constructor con resultado vacío ambiguo

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectConstructionModel.h:79–99 describe un resolver preliminar que no emite diagnósticos; resultado vacío puede significar constructor implícito, ausencia de candidato viable o ambigüedad. :259–274 describe un diagnóstico específico de propiedad sólo cuando existe un único candidato de aridad correcta. SHA-256: b892b0ccf50c7828ff185832e9a736c5fac10a939f042bbb36130d93570c0542. Es el contrato comentado de la interfaz; no se ejecutó ni contrastó aquí toda la implementación.

**Impacto y pendiente:** no convertir nullopt ni un diagnóstico genérico en una selección positiva o en causa única. Contrastar resolución completa, candidatos y diagnósticos; expresar desconocido o múltiples causas en reportes y preparar validación futura. NSC-132, LK-440 y LK-442.

### LKF-163 — Identidad abreviada en helper de tipos genéricos

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectGenericModel.h:83–92 comenta que closedTypeKeyForTemplate devuelve vacío si el template falta o no es unario y que, para tipos de usuario, limita la identidad al spelling terminal; para builtins describe coincidencia con la identidad real. SHA-256: 0e8bf80904bae2810e64a10029938770276fa2c00a9530b942ce28ae8e8842ae. No se ha trazado el helper ni demostrado una colisión efectiva o alcance general de todos los genéricos; sólo se revisaron las líneas 1–180 del archivo.

**Impacto y pendiente:** contrastar implementación, consumidores y especializaciones de igual nombre en distintos módulos; no usar una clave abreviada como identidad global sin procedencia verificada, ni tratar vacío como tipo válido. Mantener identidades y confianza en reporte. LK-443.

### LKF-164 — Resultados de resolución y reserva que requieren contexto

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectGenericModel.h:1173–1186 define estados missing, unique y ambiguous y comenta que algunos consumidores tratan conjuntamente los dos no únicos. :1201–1209 explica que una lista vacía de templatesNamedBy no demuestra ausencia de plantillas, pues el receptor puede ser un valor. :1320–1329 documenta SIZE_MAX tanto para un acierto de caché como para un error, que el llamador debe distinguir. SHA-256: 0e8bf80904bae2810e64a10029938770276fa2c00a9530b942ce28ae8e8842ae. Se trata de contratos documentados; falta contrastar usos y propagación, sin ejecución.

**Impacto y pendiente:** conservar estado y causa cuando estén disponibles, no inferir ausencia o éxito desde estos valores aislados, y mostrar incertidumbre si el contexto falta. Trazar consumidores, errores, caché y límites; preparar casos futuros de homónimos, receptor valor y caché fallida. NSC-138 y LK-443.

### LKF-165 — Catálogo de metaprogramación declarado para compilación

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectMetaprogrammingModel.h:35–40 describe el catálogo tipado como exclusivo de compilación y dice que no publica por sí mismo descriptor, registro, vtable ni símbolo visible al enlazador; :58–65 también expone runtimeAttributes y emitsRuntimeMetadata, cuyos consumidores no se han contrastado aquí. SHA-256: f31647f7db998dad50133d478dfaa87630631519548a0ac99b7aeebe240b0c50. No se concluye que otras rutas no puedan emitir metadata o atributos.

**Impacto y pendiente:** no suponer que todo dato disponible al análisis de fuente existe en DLL, EXE o PDB; inventariar qué productor serializa cada atributo y bajo qué opciones, con cobertura desconocida cuando falte evidencia. NSC-141 y LK-446.

### LKF-166 — Comentarios contradictorios y diagnóstico de new a parámetro ordinario

**Evidencia estática:** ExpressionResult.h:44–50, SHA-256 52fbe1834902fc76b6df2188221dae94cdd826dc5cafee96901d8c862cd71904, afirma en un comentario que un parámetro sin calificador acepta un owner recién creado. En cambio ProjectMethodBodyModel.h:372–383 describe una regla que identifica pérdida de propiedad; StatementAnalyzer.cpp:3720–3754, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907, implementa una condición para parámetro por valor sin ownership, sin pack, sintaxis ObjectCreationExpression, freshOwner, sin transferencia y tipo ownable, y llama a reportLostOwnership salvo nombres __nsc_. CallAnalyzer.cpp:896–915, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd, y ConstructionBinding.cpp:495–515, SHA-256 3bd433f94485511f269c25822448e08e0258ec6d0bbac63b9720a63e01342ae4, muestran usos en las rutas allí definidas. Estas lecturas son parciales y no demuestran si esas rutas se alcanzan para todos los casos ni comportamiento ejecutado.

**Impacto y pendiente:** reconciliar comentarios y contrato del lenguaje con el comportamiento real y versiones; reproducir casos con y sin parámetros owned, objetos nombrados, desugaring y constructores cuando se autoricen pruebas. No asumir que el comentario de NSC-072 o LKF-156 establece aceptación universal ni que el diagnóstico cubre todas las fugas. Correlacionar por ID y contexto sin duplicados. LKF-156, LK-433–434 y LK-447.

### LKF-167 — Cobertura declarada selectiva de cuerpos genéricos

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectMethodBodyModel.h:417–426 describe análisis de cuerpos genéricos alcanzados desde fuente en ejecutables y, adicionalmente, cuerpos públicos/protegidos de tipos cerrados exportados en bibliotecas; constructores de tipos genéricos cerrados no seleccionados se describen como diferidos y no bajados. Expone callableBodyAnalyzed y constructorBodyDeferred. SHA-256: 42fab05a96430ebcced261f1c2eb01866293dadf68d2f0964c745bded11892fe; sólo se revisaron líneas 1–480 de este archivo, y falta contrastar implementación, políticas y opciones.

**Impacto y pendiente:** no inferir que disponer de la firma implica cuerpo analizado ni que ausencia de diagnóstico acredita seguridad. Trazar selección por artefacto, especialización, visibilidad y configuración; reportar cuerpos omitidos y cobertura desconocida. Diseñar casos futuros para ejecutable, biblioteca y constructor diferido. LK-441 y LK-448.


**Contraste NSC-424:** A:/nsc-devel/src/Frontend/Generics/GenericNodeProcessor.cpp:236–268 y :397–430, SHA-256 4418f4b25017f108a967541b4410ea41b6555b9f3dd6baca7affdb9add537568, en este recorrido temprano procesa restricciones y omite el descenso en nodos de template abiertos; para template de método procesa antes el tipo del primer parámetro. Esto describe únicamente la recolección temprana de demandas, no acredita por sí solo qué cuerpos analiza ProjectMethodBodyModel ni que otras pasadas los omitan. Contrastar materialización cerrada, recorridos posteriores y diagnósticos por versión. LK-448.

### LKF-168 — Origen desconocido en await síncrono

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectMethodBodyModel.h:4974–4991 describe que un await síncrono hace un poll y que se rechaza un productor conocido como suspendible; si el origen es desconocido, indica aceptación y pánico si sigue pendiente tras ese poll. SHA-256: 42fab05a96430ebcced261f1c2eb01866293dadf68d2f0964c745bded11892fe. Es un comentario de interfaz; faltan contraste de implementación, versiones y validación ejecutada.

**Impacto y pendiente:** representar suspensión y procedencia como conocidas o desconocidas; no afirmar seguridad o bloqueo a partir de aceptación estática. Trazar productores, propagación, runtime y reporte de grabación; preparar casos futuros con callable precompilado, parámetros y future pendiente. LK-430 y LK-449.

**Contraste de implementación:** A:/nsc-devel/src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp:482–548 y :588–696, SHA-256 3fb75ec3992fc787c1baad5b9256db2d88daa8f2b77bca6be741ef0c4c913ff3. La búsqueda local agrega inicializadores y asignaciones por nombre; el grafo expande llamadas resueltas a productores async fuente analizados y considera suspensión positiva al alcanzar un suspend. Productores que no puede expandir no aportan suspensión positiva; si no encuentra una, esta comprobación no emite SynchronousAwaitCanSuspend. No equivale a demostrar que el future termine sin suspender ni a verificar el pánico descrito en runtime. Investigar shadowing, asignaciones indirectas y demás fases; LK-430 y LK-449.


### LKF-169 — Alcance declarado de asignación definitiva para agregados

**Evidencia declarativa:** ProjectMethodBodyModel.h:5004–5012 describe exclusión de agregados del reporte de asignación definitiva y la atribuye a almacenamiento predeterminado cero en el backend. Mismo SHA-256 42fab05a96430ebcced261f1c2eb01866293dadf68d2f0964c745bded11892fe. No se han contrastado aquí implementación, tipos concretos, todos los backends ni ejecución; no se demuestra lectura insegura en un caso particular.

**Impacto y pendiente:** distinguir bytes inicializados de invariantes lógicos y obligaciones de ownership; trazar reglas y backend por tipo y configuración y reportar cobertura limitada. Preparar casos futuros de campos parcialmente asignados, excepciones y layouts externos. LK-445 y LK-449.

### LKF-170 — Restricción declarada de reevaluación en asignación compuesta

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectMethodBodyModel.h:4599–4631 describe que una propiedad compuesta baja receptor e índices para getter y setter por separado y que isAccessorDestinationReevaluable restringe las expresiones aceptadas; el comentario explica por qué un campo puede cambiar entre evaluaciones. SHA-256: 42fab05a96430ebcced261f1c2eb01866293dadf68d2f0964c745bded11892fe. La documentación histórica ya registró formulaciones divergentes sobre evaluación única; la implementación y el estado actual requieren contraste. No se ejecutó un caso.

**Impacto y pendiente:** rastrear validación, lowering y orden de efectos para propiedades e indexadores, evitando proponer reescrituras o sondas que evalúen el receptor más veces. Distinguir rechazo declarado, alcance real y posible documentación desactualizada. NSC-142, LK-435 y LK-450.

**Contraste parcial adicional:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:1621–1658 y :1671–1683, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385, llama la comprobación para asignación compuesta y diagnostica que receptor e índices se evalúan para getter y setter por separado. Aún falta contrastar la implementación del predicado y lowering actual; sin ejecución.


**Contraste NSC-444:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAssignmentFrames.cpp:245–319 y :364–417, SHA-256 6081e57a9dc6b6f55c64f1852042f7548aa41b660523cb9d52184218a647e6ff, para setter compuesto almacena el valor previo, procesa el lado derecho y después invoca prepareSetterReceiver para la llamada setter; el frame programa receptor e índices cuando corresponden. Es evidencia de etapas separadas en HIR, no prueba por sí sola cuántas veces se ejecuta cada subexpresión ni de que una forma con efectos sea aceptada. Seguir selección inicial, binding de getter, predicado de reevaluación y MIR. LK-450 y LK-565.

### LKF-171 — Layout de una referencia no es tamaño de objeto

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectObjectLayoutModel.h:66–75 indica que valueLayoutForTypeKey entrega tamaño y alineación del valor y usa ancho de puntero para formas de referencia, mientras :60–65 expone layouts y planes de asignación por separado; :249 inicializa TargetDataLayout::WindowsX64. Devuelve nullopt para tipos desconocidos según el comentario. SHA-256: 808514ac807514842e08e8ff687e3dd395b27a3453a8f03eaf00b5afb1fcd46d. No se ha confirmado que esta inicialización sea la única configuración ni todos los consumidores.

**Impacto y pendiente:** distinguir bytes de referencia, extensión de objeto, carga útil, padding y overhead del allocator; no tratar nullopt como tamaño cero ni generalizar el target. Trazar productores y versionado de layout, comparar con PDB y eventos observados; verificar overflow según hallazgo previo. NSC-144, LK-440, LK-445 y LK-451.

### LKF-172 — Ordinal HIR compartido y cuerpos externos

**Evidencia declarativa:** include/nativesharp/Frontend/ProjectOopHirFunction.h:31–42 dice que la función poll async comparte memberOrdinal con la función original, como una lambda con su dueño; :45–54 describe imports C y NativeSharp cuyo cuerpo no está en el proyecto. SHA-256: 4effac434f4dfd0b972eb8804100c921d298398002706485affab717b248b0c1. Faltan contraste de lowering y mapeo real a símbolos/PDB; no se ha ejecutado una sesión.

**Impacto y pendiente:** no identificar de forma persistente una función sólo por ese ordinal ni inferir cuerpo de un import; seguir identidad física y especialización, vincular símbolos por build y declarar desconocidos los datos ausentes. NSC-145 y LK-452.

### LKF-173 — Rango aproximado de errores durante lowering HIR

**Evidencia declarativa:** A:/nsc-devel/include/nativesharp/Frontend/ProjectOopHirModel.h:331–345 describe que lowerExpression adjunta el rango del nodo interno disponible al propagar un error; precisa que la ubicación es aproximada cuando el problema se detecta un nivel más arriba. SHA-256 actual 71a204e023b5d7cb042a5cd2a3beec8f95fa8bb10bab26cf243c42ea635fcbe4. Se revisaron sólo las líneas 1–580 del archivo; falta contrastar implementación, otros emisores y presentación. No se ejecutó.

**Impacto y pendiente:** conservar el rango y su procedencia como ubicación aproximada cuando corresponda, sin atribuir precisión de subexpresión ni aplicar una corrección automática sin validación. Trazar propagación y diagnósticos anidados por fase y diseñar casos futuros. LK-453.


**Precisión adicional de LKF-168:** ProjectOopHirModel.h:4100–4109, SHA-256 71a204e023b5d7cb042a5cd2a3beec8f95fa8bb10bab26cf243c42ea635fcbe4, declara que el wrapper sintetizado de Main async conduce su future hasta completarlo y lo describe como excepción al await síncrono de un solo poll. Es comentario de interfaz; contrastar rutas y runtime antes de extrapolar. NSC-146, LK-430 y LK-449.

### LKF-174 — Observación limitada del optimizador LLVM

**Estado:** limitación documentada en fuente; contraste de implementación y reproducción pendientes. **Evidencia:** A:/nsc-devel/include/nativesharp/LLVM/LlvmOptimizationObservation.h:13–31, :58–98 y :140–149, SHA-256 9d0b22982850062f33753253d9fbb4ea584fe0ec0a8cac53de01b8f0e5c564d5. El comentario describe LLVMRunPasses como una llamada opaca mediante la API C: el orden o último pase capturado no demuestra causalidad de un fallo, y la captura del propio proceso o de stderr de LLVM puede faltar o diferir. La repetición es sospecha, no prueba de falta de convergencia; los umbrales declarados son observacionales y no detienen la llamada. El ejemplo de diferencia O0/O2 es histórico, no un defecto actual reproducido. **Pendiente:** contrastar implementación, versión de LLVM cargada y plataformas; registrar si la captura es incompleta y no presentar el último pase como culpable. Referencia LeakKiller LK-460.

### LKF-175 — Escritura de volatile del pointee restringida a fuente de metadatos

**Estado:** restricción documentada en una declaración del parser; implementación y reproducción pendientes. **Evidencia:** A:/nsc-devel/include/nativesharp/Parser/Parser.h:47–53, SHA-256 7a6eaf31988ee569cf790c99faad85058579c2f35cc46e1377cd8c4cd183d0dc. El comentario de allowMetadataSpellings indica que las formas de tipo T volatile* y T const volatile* se aceptan para fuente de API materializada desde metadatos C, y que NativeSharp no puede escribir esa calificación directamente. No se ha comprobado aquí el alcance exacto de gramática, diagnóstico ni si existen otras formas equivalentes. **Pendiente:** contrastar parser, materializador y binding en la versión efectiva y diseñar reproducción futura; no sustituir el contrato importado por un puntero ordinario. Referencia LeakKiller LK-462.

### LKF-176 — Inferencia de tipo en llamadas genéricas estáticas: antecedente

**Estado:** antecedente en comentario de biblioteca; vigencia y alcance no verificados. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/Algorithm.ns:18–21, SHA-256 94ff2b08b3f9c4a2789f689c66cea30130782afff649770bfa87c9207e738c59. El comentario afirma que llamadas como Algorithm.Find requieren argumento de tipo explícito y describe un diagnóstico poco esclarecedor. Puede corresponder a una versión anterior: no se ha contrastado el compilador actual ni ejecutado el ejemplo. **Pendiente:** revisar resolución e inferencia por versión y preparar reproducción de sobrecargas, inferencia parcial y diagnóstico. LK-463.

### LKF-177 — Grupo de métodos como argumento delegado: antecedente

**Estado:** antecedente en comentario de biblioteca; vigencia y alcance no verificados. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/Algorithm.ns:23–30, SHA-256 94ff2b08b3f9c4a2789f689c66cea30130782afff649770bfa87c9207e738c59. El comentario describe rechazo al pasar directamente un grupo de métodos como predicado y propone asignarlo antes a una variable delegada. No demuestra comportamiento actual ni cubre todos los contextos o firmas. **Pendiente:** contrastar resolución y conversiones de delegates y preparar reproducción futura con métodos estáticos, receptores y sobrecargas. LK-463.

### LKF-178 — Riesgos por verificar en algoritmos de colección

**Componente:** biblioteca NativeSharp.Core.Collections.Generic; no es un defecto atribuido al compilador. **Estado:** hipótesis de revisión estática, sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/Algorithm.ns:369–423 y :433–505, SHA-256 94ff2b08b3f9c4a2789f689c66cea30130782afff649770bfa87c9207e738c59. Swap elige accesos long* o int* según divisibilidad del tamaño de T; falta verificar alineación efectiva y semántica del backend para tipos empaquetados y destinos admitidos. SiftDown calcula 2 * current + 1 en int antes de comprobar el límite; debe examinarse overflow y alcanzabilidad con tamaños reales. No se demostró comportamiento incorrecto. **Pendiente:** analizar contratos de layout, límites y overflow, y preparar reproducción futura para tipos empaquetados y longitudes extremas. LK-464.

### LKF-179 — ABI de structs grandes en delegates: antecedente

**Estado:** comentario histórico; vigencia y alcance sin verificar. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/Delegates.ns:52–58, SHA-256 16b066c7f6e8fa3c50655427c3398c9c76f203ba886e608465025c6aa6594b7f. El comentario atribuye a un compilador Windows x64 de una etapa anterior una restricción para structs mayores de ocho bytes en locales, argumentos y retornos, y explica un delegate de dos parámetros como adaptación. No demuestra vigencia ni identifica aquí build exacto. **Pendiente:** contrastar ABI y compiladores actuales con tamaños y calificaciones diferentes. LK-465.


**Contraste documental adicional:** Dictionary.ns:41–45 (SHA-256 11614e989679755a15fd25497273131c53f9a2b7a012685acdd6ce933862c0dd) afirma que NativeSharp 3.0 ya baja structs no directos mediante sret Win64 y permite un KeyValuePair mayor de ocho bytes como Current. Esta afirmación posterior contradice el alcance general del comentario histórico; aún falta contrastar emisor y versión y no demuestra todos los contextos.

### LKF-180 — Contrato throws en accessor: compilador oficial citado

**Estado:** antecedente documental de compilador oficial; no reproducido ni extrapolado al código actual. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/DICTIONARY-INDEXER-OFFICIAL-NSC.md:3–26, SHA-256 35719e7123f32a926ff1ea1aa4326ea67918ad9ff6dbfdfd287a3e31834d2365. El documento atribuye rechazo de throws en accessor al build eb9582e921fd9757 y describe una adaptación; la verificación menciona por separado el build 2a44eedad51a7a9b, por lo que no debe confundirse cuál acredita qué. Reporta compilación O0, no ejecución. **Pendiente:** identificar versiones, contrastar parser y semántica y preparar reproducción futura. LK-465.

**Fuentes documentales adicionales:** SORTED-DICTIONARY-INDEXER-OFFICIAL-NSC.md (SHA-256 9eca95fb1d7ed112f30b4bb32c3e4e4a0d7002177d03f642d025e62700455e6d) y SORTED-LIST-INDEXER-OFFICIAL-NSC.md (SHA-256 179b3bf03fabfcde8d23282a92689a32dc389c54ae4f8e7decd265dad3a5514f), en A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/, describen la misma adaptación y los mismos dos hashes distintos. Son antecedentes relacionados, no reproducciones independientes verificadas aquí.

### LKF-181 — Contratos y riesgos por verificar en Dictionary

**Componente:** biblioteca NativeSharp.Core.Collections.Generic; no atribuido al compilador. **Estado:** revisión estática, sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/Dictionary.ns:177–202, :220–230, :273–304 y :491–516; SHA-256 11614e989679755a15fd25497273131c53f9a2b7a012685acdd6ce933862c0dd. TryGetValue documenta un arreglo de salida de al menos un elemento y escribe result[0] cuando encuentra la clave, sin guarda explícita visible en el método: verificar comprobación efectiva de índice/null y contrato del llamador. El crecimiento calcula capacidad entera y reserva cinco arreglos: contrastar overflow, fallo parcial y cleanup antes de afirmar fuga. Los snapshots contienen aliases no propietarios: verificar duración ante borrado o reemplazo de elementos y no asumir que mantener vivo el diccionario conserva cada elemento. Son casos pendientes, no defectos reproducidos. LK-466.

### LKF-182 — Propiedad compartida entre HashSet con elementos de referencia

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** limitación descrita por el código y ruta de inserción observada estáticamente; vigencia y efectos dinámicos pendientes de verificar. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/HashSet.ns:92–113 y :146–164, SHA-256 49abf5a8c7284020b3355af489e6fa809b94e11564e72b1ef4924687af8ee2ae. UnionWith y SymmetricExceptWith toman elementos de un snapshot y los pasan al diccionario receptor; el comentario advierte que para tipos de referencia ambos conjuntos pueden referenciar el mismo elemento sin copias independientes. Debe contrastarse la semántica efectiva de transferencia y destrucción, incluidos alias, autounión, excepciones y orden de liberación, antes de afirmar doble liberación concreta. El enumerador posee el arreglo snapshot pero comenta que sus elementos son aliases prestados (HashSetEnumerator.ns:9–12 y :35–38, SHA-256 7c96a060f002c89316ced8ea894905043af1cfe46d9346b921a803c3e6cf0647). LK-467.

**Alcance adicional:** SortedSet.ns:209–223 y :252–269 (SHA-256 720f360f71f6b740934d2d18e52058f7d8f76942c23858d7887f5fd68b773757) declara la misma salvedad de propiedad entre conjuntos y transfiere elementos obtenidos del snapshot a SortedList. Su comportamiento efectivo requiere contraste separado.

### LKF-184 — Transferencia a campo owned: antecedente de colección

**Estado:** comentario histórico de biblioteca; vigencia y alcance no verificados. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/LinkedListNode.ns:10–16, SHA-256 2ed3d9e6bfd9abfc5adcdceda34c18aaee713a60e825f5d2232787f784fb0448. Describe el uso de arreglos owned de un elemento como alternativa ante una supuesta brecha al asignar propiedad a campos e indexadores. El comentario no identifica build ni demuestra el comportamiento actual; el compilador ya tiene otras rutas y mecanismos de ownership que deben contrastarse. **Pendiente:** seguir análisis y lowering de asignación a campo, setter e índice con transferencias, reemplazo y cleanup y preparar reproducción futura por versión. LK-468 y LKF-005.

**Contraste documental adicional:** SortedList.ns:361–363 (SHA-256 9d631b0d7cac49f1e57e6940065c9de71e4db2857602da7d57ecc529c318d32f) dice que setters de indexador no reciben owned; List.ns:17–22 y :72–82 (SHA-256 41bb760efdd8d7fe10965259d4185f1830bce8d09c83f98336b4a9a4aa605df2) describe y declara un indexador owned con setter. La discrepancia exige contrastar diferencias entre listas y diccionarios y versiones; no prueba que ambas formas sean equivalentes.

### LKF-183 — Pertenencia de nodos en lista enlazada

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** precondición sin validación documentada en fuente; comportamiento adverso no reproducido. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/LinkedList.ns:133–170 y :199–233, SHA-256 f5f313d798c5b528a4e9cbd7bae2f87a3ce439727b79720386080a4382f5cad1. AddAfter y Remove documentan que el nodo debe pertenecer a la lista y que esa pertenencia no se valida; AddBefore utiliza referencias del nodo. Hay que verificar efectos de nodo ajeno, nulo, separado o ya liberado y preservar la precondición en reportes. La limpieza ordinaria es iterativa; el destructor de un nodo que conserva Next puede recorrer la cadena recursivamente según LinkedListNode.ns:88–98. LK-468.

### LKF-186 — Construcción de owned T[] trasladable: antecedente

**Estado:** comentario histórico de biblioteca; vigencia y alcance no verificados. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/List.ns:195–202, SHA-256 41bb760efdd8d7fe10965259d4185f1830bce8d09c83f98336b4a9a4aa605df2. El comentario afirma que al escribir AddRange no se encontró forma de construir desde el llamador un valor owned T[] trasladable, por lo que conserva prestado el bloque y mueve los elementos. No identifica el build ni demuestra restricción en la versión actual. **Pendiente:** contrastar sintaxis, binding, transferencia y cleanup para arreglos con elementos owned y preparar reproducción futura por versión. LK-469.

**Fuente adicional:** Environment.ns:128–134 (SHA-256 87bc25347fd7fc8f3986e769ad332c0f659cf95ad9cc3499aeb7d8e6c9c8d60c) reporta históricamente una limitación para construir y llenar owned string[] en varios pasos. Contrastar con versiones y constructos actuales; no prueba que todos los arreglos owned sean imposibles.

**Fuente adicional String:** String.ns:1061–1075 (SHA-256 72fb8ae33aab4b82104bd0ca90692ea2d70f7326bd84681f13fe13433043fe74) atribuye a reproducciones anteriores restricciones concretas de raii en arreglos, move de locales owned y sintaxis de inicializador, y emplea destino aportado por el caller. El comentario no identifica build ni prueba vigencia; contrastar separadamente cada constructo y la transferencia a elementos prestados. LK-487.

### LKF-185 — Límites y predicados variables en List

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** riesgos de revisión estática; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/List.ns:72–90, :164–180, :183–193, :234–252 y :536–585; SHA-256 41bb760efdd8d7fe10965259d4185f1830bce8d09c83f98336b4a9a4aa605df2. RefAt verifica index contra count, pero indexador, SetAt y otras operaciones usan items sin guarda lógica explícita visible; hay que distinguir comprobación del arreglo físico de precondición de lista y efectos reales. FindAll evalúa predicate en dos pasadas y reserva según la primera; si los resultados cambian o el callback modifica la lista, verificar escritura fuera del resultado, datos incompletos y limpieza, sin afirmar comportamiento ejecutado. También contrastar aritmética de rangos y capacidad. LK-469.

### LKF-187 — Salidas y reservas de PriorityQueue por verificar

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** hipótesis derivadas de lectura estática; no reproducidas. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/PriorityQueue.ns:96–132, :169–180 y :210–252, SHA-256 496268aaa90115f3cf9fc65412293313e9f84c96b5edd01f0d58871ec37a8d03. TryDequeue copia priorities[0] a priorityResult[0] y luego Dequeue destruye la prioridad desplazada; para TPriority de referencia hay que determinar si el valor devuelto es alias y queda inválido o si existe otra semántica de copia. Ambos Try escriben en arreglos de salida sin comprobar explícitamente su longitud. SwapAt y crecimiento usan reservas múltiples y SiftDown calcula índices enteros: verificar cleanup excepcional, alineación, overflow y alcanzabilidad antes de afirmar fugas o accesos inválidos. LK-470.

### LKF-188 — Índices y transferencia de salida en Queue

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** riesgos identificados por lectura estática, sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/Queue.ns:42–73 y :160–182, SHA-256 cb1f2dece365f620a629b26f6f400d3ebeb11e7136f823a54c545ce17c69735c. RefAt compara index con count, mientras el indexador usa PhysicalIndex sin guarda lógica explícita; debe comprobarse qué sucede con índices fuera de count pero dentro del arreglo físico y con overflow. TryDequeue y TryPeek escriben result[0] sin guarda explícita visible; verificar la evaluación de destino y Dequeue, la limpieza del valor transferido si falla el almacenamiento y la vida de aliases de TryPeek. LK-471.

**Alcance adicional:** Stack.ns:110–131 y :155–166 (SHA-256 e388977995690609efb8ad69995660336fb33d3df5aa4c4b0798dc9b5bdef7fd) presenta patrones equivalentes en TryPop/TryPeek y en la diferencia entre indexador y RefAt. Requiere evaluar por separado orden de evaluación, cleanup y límites reales; no se ha reproducido.

### LKF-189 — Límites y reservas de SortedList por verificar

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** riesgos de lectura estática, sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/SortedList.ns:220–229, :251–264, :268–310 y :392–404, SHA-256 9d631b0d7cac49f1e57e6940065c9de71e4db2857602da7d57ecc529c318d32f. TryGetValue escribe result[0] sin guarda explícita visible; KeyAt, ValueAt y RemoveAt acceden con índice sin comparar explícitamente count. Contrastar comprobaciones físicas, precondiciones y manejo de índices inválidos. El crecimiento reserva dos arreglos y los rellena con movimientos: analizar fallos parciales, overflow y limpieza antes de afirmar una fuga. Los snapshots pueden contener aliases prestados. LK-472.

### LKF-190 — GetViewBetween usa orden natural con comparador personalizado

**Componente:** biblioteca de colecciones; no atribuido al compilador. **Estado:** divergencia documentada y visible estáticamente, sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Collections.Generic/SortedSet.ns:149–174, SHA-256 720f360f71f6b740934d2d18e52058f7d8f76942c23858d7887f5fd68b773757. El comentario indica que el filtrado usa operator < aunque el conjunto tenga comparador personalizado; el código también construye un nuevo SortedSet con constructor predeterminado. Verificar efecto en rangos invertidos, tipos sin orden natural, identidad/propiedad de referencias y cleanup de snapshot, sin afirmar resultado ejecutado. LK-473.

### LKF-191 — Receptor struct temporal: antecedente y contraste

**Estado:** comentario histórico contradictorio con infraestructura posterior; vigencia no verificada. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Diagnostics/Stopwatch.ns:106–115, SHA-256 017bb2bcace82c5e034138147b1ae0bea8c21f9ce750180661897acea58189dc, afirma que Elapsed().TotalMilliseconds() requería materializar un local. ProjectOopHirNodeKind.h:121–124 y ProjectOopMirNodeKind.h:101–102 declaran TemporaryAddress para dar dirección a resultados temporales; esto sugiere una ruta posterior pero no demuestra que aquel caso funcione en cada versión. **Pendiente:** contrastar parser, binding y lowering, y preparar reproducción de métodos y propiedades con efectos sobre temporales. LK-475.

### LKF-192 — Persistencia diferida de FileTextWriter

**Componente:** biblioteca IO; no atribuido al compilador. **Estado:** comportamiento documentado y visible en fuente, sin ejecución actual. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.IO/TextWriter.ns:165–174 y :198–223, SHA-256 7f426dba7225290bbcea75b46af5fc5b886aa536b1c0d60634df518d3687ec99. FileTextWriter acumula en memoria y vuelca sólo al llamar Flush; su comentario advierte pérdida del contenido pendiente si el proceso termina antes. No se han contrastado errores de File.WriteAllText/AppendAllText, atomicidad, escritura parcial ni durabilidad tras Flush. Un reporte o grabación de fallo no debe asumir persistencia incremental por usar esta clase. LK-476.

### LKF-193 — Parámetro genérico como retorno de delegate: antecedente

**Estado:** restricción descrita en comentario de fuente; vigencia y alcance pendientes. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Threading/Job.ns:3–9, SHA-256 9665d3789fde7b61dcb51cccbd1461dcc85f568fc0313a9cbfdd533ed2cd5b4f. El comentario afirma que no se puede declarar un delegate genérico cuyo retorno sea su parámetro genérico por el orden de parsing, y usa JobFunction<T> como clase abstracta alternativa. No identifica build ni prueba que la restricción siga presente en todas las sintaxis o versiones. **Pendiente:** contrastar parser, binding y materialización de firmas y preparar reproducción futura. LK-477.

**Fuente adicional:** Lazy.ns:3–5, SHA-256 75efd310105a7a7edb1ed370520e0eba94b519f3b923e4347cc7bdd476065248, repite el antecedente sobre retorno de delegate genérico y usa ILazyValueFactory<T>. No constituye reproducción independiente ni prueba de vigencia.

### LKF-194 — Dependencias prestadas y sincronización de Job

**Componente:** biblioteca de concurrencia; no atribuido al compilador. **Estado:** cuestiones de revisión estática y discrepancia documental; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Threading/Job.ns:23–66, :69–80 y :126–162, SHA-256 9665d3789fde7b61dcb51cccbd1461dcc85f568fc0313a9cbfdd533ed2cd5b4f. El job mantiene dependencias no propietarias y el comentario exige programarlas antes, pero AddDependency no comprueba esa condición. La sincronización entre AddDependency y Schedule, la vida del job y dependencias, y la señal de finalización ante fallos del callback necesitan contraste. La nota LANGUAGE-3-MIGRATION.md:43–45, SHA-256 5c0acb0c0719d421e357eef3dc219f6a2aef2095f9261ba0960404a2e3faf759, describe el arreglo como owned; la fuente actual lo declara IJob[] y lo libera explícitamente. No inferir fuga ni carrera ejecutada. LK-477.

### LKF-195 — Campos borrowed: antecedente de Stage 50

**Estado:** comentario histórico; vigencia y alcance no verificados. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Threading/ScopedLock.ns:20–45, SHA-256 14933b567685456adfa934e46a3e45e02cabb3bf501f894831f096b07ee40911. El comentario afirma que Stage 50 rechazaba campos borrowed por la posible vida del campo frente al dueño y explica el uso de una referencia de clase sin calificador. No identifica build y no demuestra la política actual ni sus excepciones. **Pendiente:** contrastar ProjectMemberModel, tipos borrowed, lifetimes y diagnósticos de la versión efectiva; preparar casos futuros de escape y destrucción. LK-478.

### LKF-196 — Invariantes de conteo fuerte/débil y mutex prestado

**Componente:** biblioteca de concurrencia; no atribuido al compilador. **Estado:** puntos de contrato y riesgos por verificar estáticamente; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Threading/ReferenceCounting.ns:14–16 y :41–50, :89–164, SHA-256 67055e5ff495e28f633601e94e74a6cfbd371320ea378f142b894c54862c7223; ScopedLock.ns:64–81, SHA-256 14933b567685456adfa934e46a3e45e02cabb3bf501f894831f096b07ee40911. El bloque describe una referencia débil implícita y hooks separados de destrucción y desasignación; StrongCount y WeakCount son lecturas separadas, por lo que no deben presentarse como snapshot coherente de ambos contadores. Contrastar precondiciones de ReleaseWeak, vida del bloque durante carreras, hooks y excepciones. ScopedLock guarda un Mutex sin ownership y lo usa en su destructor; comprobar duración y cleanup en cada salida. No se afirma un fallo concurrente. LK-478.

### LKF-197 — Vida de callbacks y cierre de ThreadPool

**Componente:** biblioteca de concurrencia; no atribuido al compilador. **Estado:** contratos explícitos y riesgos por contrastar estáticamente; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core.Threading/ThreadPool.ns:21–45, :122–133 y :199–231, :233–278, SHA-256 1a97b28c4e241d54d42bb27a4ba9f196276fe3df25cb1e8211786f198d935f08. La API describe callbacks prestados cuyo owner debe sobrevivir a WaitIdle/Shutdown; el campo de cola se declara owned ThreadStart[] y requiere contrastar la semántica real de sus elementos. WorkerLoop llama work() antes de decrementar pending_; investigar efectos de fallo o terminación de callback, llamadas WaitIdle/Shutdown desde worker, vida del pool, overflow de capacidad y limpieza parcial. No se ha demostrado bloqueo, fuga ni doble liberación en ejecución. LK-479.

### LKF-198 — Límites y señal de lectura de BinaryReader

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** casos por revisar estáticamente; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/BinaryReader.ns:54–75 y :157–189, SHA-256 107d4cbcc458792bca19d99b1f65137faa680b47cbd8ed21f4664505093beb84. Skip calcula position + count antes de Seek: verificar política de overflow y valores extremos. ReadSpan devuelve vista de longitud cero tanto para count cero como cuando Has falla, por lo que el consumidor debe distinguir situaciones mediante contexto o posición; ReadInto devuelve cero ante falta de bytes o cantidad efectiva cero. El backing es prestado y su vida debe seguirse. No se demuestra acceso inválido ni fallo concreto. LK-480.

### LKF-199 — Invalidación de vista tras crecimiento en el mismo ámbito

**Estado:** limitación declarada por comentario de biblioteca; vigencia y cobertura del compilador pendientes de contraste. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/ByteBuffer.ns:67–80 y :119–130, SHA-256 9f757aefd0e7a4d4feb189c6c8d63e9d40dba193a3dc654daecd47aff23f31f2. AsSpan devuelve una vista al almacenamiento actual; EnsureCapacity puede reemplazar y liberar ese almacenamiento. El comentario afirma que el chequeo de escape descrito no impide usar la vista después de un Add que reasigne dentro del mismo ámbito. Que Add provoque realmente reasignación depende de la capacidad; no se ha reproducido ni contrastado la versión actual del analizador. **Pendiente:** seguir alias, invalidación interprocedural, resumen de efectos y diagnósticos; preparar casos futuros con y sin crecimiento, referencias y llamadas indirectas. LK-481.

### LKF-200 — ByteBuffer: límites de capacidad e índice

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** cuestiones estáticas por verificar, sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/ByteBuffer.ns:67–110 y :148–151, SHA-256 9f757aefd0e7a4d4feb189c6c8d63e9d40dba193a3dc654daecd47aff23f31f2. El crecimiento calcula items.Length*2 y count+take en int; el indexador usa items[index] sin comprobación explícita de count. Contrastar overflow, límites físicos y lógicos, alias de source durante crecimiento y cleanup de reserva; no se ha demostrado acceso indebido ni fuga. LK-481.

### LKF-201 — Salida concurrente y terminación abrupta

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** contratos descritos en fuente; implementación runtime pendiente de contraste. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/Console.ns:18–31, SHA-256 de885c53a8efe6164eaa18e834d43ff832b4889721ce41f5f9888c750ebdcedf, advierte que la salida no promete atomicidad entre hilos. Environment.ns:80–87, SHA-256 87bc25347fd7fc8f3986e769ad332c0f659cf95ad9cc3499aeb7d8e6c9c8d60c, describe terminación inmediata sin cleanup RAII pendiente. No se han ejecutado ni verificado aquí los hooks de runtime. LK-482.

### LKF-202 — Alias públicos y actualización de traza en Exception

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** cuestiones de revisión estática; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/Exception.ns:18–26 y :48–83, SHA-256 aa3a6de32f8a629a777b4e408a3b77aa8f37ecef53cad8454654f1c7fa266ee1. Message, InnerException y StackTrace son referencias públicas separadas de campos owned internos; SetStackTrace sustituye el almacenamiento y luego actualiza el alias. Deben contrastarse la destrucción generada al reemplazar, modificación externa de aliases, fallo de copia y duración de referencias retenidas. StackTrace es texto almacenado y no demuestra que se haya capturado historial de ejecución. El comentario :131–134 sobre un getter que lanza contradice Dictionary.ns:205–218 y la nota de compatibilidad; no se toma como contrato vigente. LK-483.

### LKF-203 — Lazy sin publicación thread-safe implícita

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** contrato descrito en fuente; sin reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/Lazy.ns:11–19 y :21–47, SHA-256 75efd310105a7a7edb1ed370520e0eba94b519f3b923e4347cc7bdd476065248. El comentario indica inicialización síncrona y que la publicación thread-safe no está incorporada; la fábrica es prestada y debe sobrevivir a Lazy. El método usa banderas ordinarias y finally para reiniciar evaluating. Contrastar protección externa, vida del payload, fallos y acceso concurrente; no se ha demostrado carrera en ejecución. LK-484.

### LKF-204 — Miembro estático en tipo genérico cerrado: antecedente

**Estado:** comentario histórico; vigencia no verificada. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/Optional.ns:129–131, SHA-256 61c79f88f1b99ce949b3ec3fa46e201b48e19dcfaa9c2742ad091c805551e756. El comentario reporta que Optional<int>.None() no se parseaba y usa Optionals.None<int>(). También alude a ID 132, pero Algorithm.ns:237–250 indica una corrección posterior de identidad de sobrecargas; no tratar ambos antecedentes como fallas actuales. **Pendiente:** contrastar parser, resolución y versiones y preparar reproducción futura. LK-485.

**Fuente adicional:** Span.ns:385–403, SHA-256 9b381be94851fa74ddc9e892558bed616e7de979b996cae52d3292d8ce91eac9, describe el ejemplo histórico Span<int>.Of(items) y la alternativa Spans.Of<int>(items). Líneas 405–414 señalan cierre histórico de ID 132; no implica que ID 131 siga abierto.

### LKF-205 — Vista borrowed como payload Optional ordinario

**Estado:** restricción de seguridad descrita en fuente; alcance y vigencia por contrastar. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/Optional.ns:26–40, SHA-256 61c79f88f1b99ce949b3ec3fa46e201b48e19dcfaa9c2742ad091c805551e756. El comentario informa que Optional<StringView> se rechaza porque un agregado ordinario podría sobrevivir a lo prestado; propone vista vacía para ausencia. No se ha reproducido ni verificado si existen otros portadores borrowed seguros o excepciones en la versión actual. **Pendiente:** contrastar binding, propagación de lifetime y diagnóstico por versión. LK-485.

### LKF-206 — Contratos diferenciados de Span

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** comportamientos visibles y contratos documentados en fuente; ejecución y lowering pendientes. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/Span.ns:25–50, :94–166, :179–253 y :461–466, SHA-256 9b381be94851fa74ddc9e892558bed616e7de979b996cae52d3292d8ce91eac9. El indexador devuelve default o ignora escritura fuera de rango, RefAt/Put hacen panic y UncheckedAt/Set omiten esa guarda explícita; CopyTo declara que no maneja solapamiento, a diferencia de CopyToOverlapping. OfRaw acepta puntero y longitud provistos sin validación. La fuente describe mapa de vida para vistas de arreglo pero no para raw: contrastar emisión y alias, incluidos Data(), longitud negativa, overflow y orden de evaluación antes de afirmar fallo. LK-486.

### LKF-207 — Contratos de escritura y aritmética en String

**Componente:** biblioteca Core, no atribuido al compilador. **Estado:** riesgos identificados en lectura, comportamiento no reproducido. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/String.ns:1029–1036, :1082–1149, :1252–1291 y :1434–1469; SHA-256 72fb8ae33aab4b82104bd0ca90692ea2d70f7326bd84681f13fe13433043fe74. CopyTo escribe en destinationIndex + i sin guarda explícita local; Split y Partition escriben cadenas nuevas en slots proporcionados por el llamador, cuya ocupación y cleanup anterior requieren contraste. Format acumula un índice decimal en int y comprueba index < args.Length sin guarda explícita index >= 0: ante una plantilla suficientemente larga debe investigarse semántica de overflow y chequeos efectivos antes de concluir acceso indebido. No se ha ejecutado ni demostrado corrupción o fuga. LK-487.

### LKF-208 — Crecimiento y temporal de StringBuilder

**Componente:** biblioteca Core; propiedad y cleanup por atribuir. **Estado:** revisión estática, no reproducción. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/StringBuilder.ns:28–80 y :183–195, SHA-256 11b0662d2642ec7a8e60c1d3c8e01efcdede97f1691e54a2187b107f47c92627. Asegurar calcula cuantos + necesarios y duplica capacidad sin guarda explícita de overflow, copia y reemplaza el arreglo; deben examinarse semántica aritmética y fallos. ToOwnedString reserva exacto, lo entrega a FromCharArray y no muestra delete[]; contrastar copia/transferencia y cleanup generado antes de declarar fuga. WORKITEMS de gaps-limitaciones ya contiene el antecedente como investigación de reserva y drop, no conclusión de defecto del compilador. LK-488.

### LKF-210 — Dirección de carácter indexado: antecedente

**Componente:** compilador. **Estado:** restricción descrita en comentario de biblioteca; vigencia sin verificar. **Evidencia:** A:/nsc-devel/lib/NativeSharp.Core/StringView.ns:28–35, SHA-256 8c5c2aeac49da864035b9887b4890f3ecb57f802023736905b917f697ea41d3d. El comentario afirma que &texto[0] no compila porque el operador de dirección exige local, parámetro o campo direccionable, y que CharacterPointer se añadió como alternativa. No demuestra que siga aplicando a todos los indexadores, tipos o versiones. Contrastar análisis semántico, lowering y casos por receptor; no suponer que CharacterPointer impide escritura ni permite cualquier índice. LK-489.

### LKF-209 — Vista cruda de cadena y contrato de acceso

**Componente:** biblioteca Core; no atribuido al compilador. **Estado:** contrato visible; comportamiento adverso no reproducido. **Evidencia:** StringView.ns:42–65, :96–113, :126–173 y :296–316, SHA-256 8c5c2aeac49da864035b9887b4890f3ecb57f802023736905b917f697ea41d3d. Una vista almacena puntero y longitud, no mantiene vivo el dueño ni añade terminador NUL; constructor acepta puntero y longitud sin validación visible, indexador y UncheckedAt acceden directamente y Data devuelve char*. Revisar procedencia, límites, semántica de Slice con argumentos extremos y consumo por API C. No inferir un fallo observado ni que los análisis de escape cubran punteros crudos. LK-489.

### LKF-211 — Identidad y generación de GenerationArena

**Componente:** compilador self-hosted, capa Basic; no atribuido a código generado por compilador. **Estado:** riesgo de contrato visible, uso adverso sin reproducir. **Evidencia:** A:/nsc-devel/selfhost/nsc/Basic/GenerationArena.ns:5–51, SHA-256 11bc40fefd715bae5bc155137fceb454001e3d0ace4636732589d32ba2a6f92b. ArenaHandle lleva dirección, generación y tamaño, pero no identidad de arena; Resolve comprueba IsEmpty y número de generación, no pertenencia del handle a storage. Dos arenas con igual generación o un handle construido externamente requieren revisión de precondiciones y callers. Reset incrementa int y vuelve a 1 cuando resulta no positivo; estudiar overflow y eventual reutilización de generación, sin afirmar comportamiento observado. LK-493.

### LKF-212 — Comprobación de reserva en backend LLVM self-host

**Componente:** compilador self-hosted, generación LLVM. **Estado:** ruta estática por investigar; fallo de reserva no reproducido. **Evidencia:** A:/nsc-devel/selfhost/nsc/CodeGen/LlvmBackend.ns:1007–1028 y :1333–1360, SHA-256 f1e2a3d0e8b7e9e266585aa0ed393b4ba0f3e88cb92998c4a724831383e7906e. BoxGenericScalar y AllocateNativeString generan llamadas HeapAlloc y luego generan stores sobre el resultado. Sus comparaciones con null comprueban handles devueltos por LLVMBuild al construir IR; en esos métodos no se observa una rama emitida que compruebe en ejecución el puntero retornado por HeapAlloc antes de almacenar. Seguir caller, configuración y contrato de fallos, y revisar tamaño length + 9, overflow y liberación del box antes de concluir el impacto; no se ejecutó ni se demostró crash o fuga. LK-494.

**Contraste adicional:** LlvmBackend.ns:2288–2340 y :2409–2452 (mismo SHA-256) construye llamadas HeapAlloc para objetos y arreglos; en esas ramas se comprueban handles LLVM durante generación, y el arreglo genera almacenamiento del largo. Analizar guardas emitidas, cantidad negativa, multiplicación, tamaño fallback y cleanup por separado; no se ha reproducido fallo. LK-494.

### LKF-213 — Guardas de intrínsecos de cadena en backend self-host

**Componente:** compilador self-hosted, generación LLVM. **Estado:** contraste estático parcial, no reproducido. **Evidencia:** A:/nsc-devel/selfhost/nsc/CodeGen/LlvmBackend.ns:1458–1495 y :1732–1791, SHA-256 f1e2a3d0e8b7e9e266585aa0ed393b4ba0f3e88cb92998c4a724831383e7906e. En las ramas visibles CharacterPointer extiende el índice y calcula dirección, Substring extiende inicio y longitud y copia, y Equals carga longitudes; no se observan allí guardas emitidas de rango o nulidad. StringView.ns:28–35 describe históricamente comprobación de rango de CharacterPointer, por lo que debe contrastarse cada backend y comprobar si existen guardas semánticas o externas antes de concluir incompatibilidad o acceso indebido. LK-495.

**Contraste adicional:** LlvmBackend.ns:2034–2080 (mismo SHA-256) calcula dirección de elementos de array o string después del encabezado sin guarda explícita visible en esa rama. La validación semántica previa, otras rutas de indexadores y posibles comprobaciones externas siguen por investigar; no generalizar a todos los accesos. LK-495.

**Contraste semántico:** SemanticAnalyzer.ns:623–646, SHA-256 3a608609a5d0d438965580415d1d7ee8423cf90371d0360d28f177352b54ab22, marca BoundsCheck para string y arreglos, pero no para punteros. Contrastar propagación de ese hecho hacia la rama LLVM descrita y el resto de rutas; la marca no prueba ejecución de una guarda. LK-495 y LK-502.

### LKF-214 — File.Exists y Directory.Exists en backend self-host

**Componente:** compilador self-hosted, generación LLVM. **Estado:** diferencia visible en ruta de generación; comportamiento no reproducido. **Evidencia:** A:/nsc-devel/selfhost/nsc/CodeGen/LlvmBackend.ns:1806–1828, SHA-256 f1e2a3d0e8b7e9e266585aa0ed393b4ba0f3e88cb92998c4a724831383e7906e. La misma rama para ambos receptores genera llamadas a file_exists y directory_exists y devuelve OR de los dos resultados. Si se alcanza con las API ordinarias y ambos hooks distinguen archivo de directorio, podría hacer que cada método acepte también el tipo contrario; confirmar resolución del intrínseco, implementación de hooks y contrato esperado antes de concluir impacto. LK-496.

### LKF-215 — Generación restringida de delete en backend self-host

**Componente:** compilador self-hosted, generación LLVM. **Estado:** limitación visible en fuente; comportamiento de programas y ciclo completo no reproducidos. **Evidencia:** A:/nsc-devel/selfhost/nsc/CodeGen/LlvmBackend.ns:2478–2519, SHA-256 f1e2a3d0e8b7e9e266585aa0ed393b4ba0f3e88cb92998c4a724831383e7906e. El comentario señala que la destrucción de genéricos cerrados espera metadata SH46. La rama DeleteExpression devuelve una constante sin emitir liberación cuando !node.Flag() o el nombre del método no es Replace, Asegurar o Main; sólo la rama restante emite liberación en el fragmento examinado. Contrastar significado de Flag, otras rutas de cleanup/destructores, resolución de esos nombres y alcance por versión antes de cuantificar memoria retenida o afirmar que todo delete carece de efecto. LK-497.

**Contraste del parser:** Parser.ns:555–561, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661, fija Flag a true para la forma delete[] y false para delete escalar. Esto aclara la rama citada, pero aún deben examinarse limpieza implícita y efectos ejecutables. LK-497.

### LKF-216 — Throw y finally en backend self-host

**Componente:** compilador self-hosted, generación LLVM. **Estado:** comportamiento de generación visible; no se han ejecutado programas de contraste. **Evidencia:** A:/nsc-devel/selfhost/nsc/CodeGen/LlvmBackend.ns:3507–3544, SHA-256 f1e2a3d0e8b7e9e266585aa0ed393b4ba0f3e88cb92998c4a724831383e7906e. La rama ThrowStatement evalúa opcionalmente su expresión y genera ExitProcess(1) seguido de unreachable; TryStatement genera el cuerpo y sólo visita node.Second cuando el cuerpo no termina. El fragmento no evidencia unwinding ni manejo de catch, y la relación exacta de node.Second con finally debe contrastarse con parser y semántica. Investigar retornos, break, continue, throw, cleanup y política explícita de este subconjunto; no atribuir el mismo comportamiento al compilador principal. LK-498.

**Contraste del parser:** Parser.ns:876–892, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661, coloca cláusulas catch en hijos y finally en node.Second. Esto confirma la correspondencia estructural examinada; todavía falta contrastar otras transformaciones y casos ejecutables. LK-498.

### LKF-217 — Metadata de depuración en backend self-host

**Componente:** compilador self-hosted, generación LLVM. **Estado:** inspección completa de este archivo, no prueba de artefacto generado. **Evidencia:** A:/nsc-devel/selfhost/nsc/CodeGen/LlvmBackend.ns:3901–4035, SHA-256 f1e2a3d0e8b7e9e266585aa0ed393b4ba0f3e88cb92998c4a724831383e7906e. EmitExecutableObject crea, finaliza y destruye un DIBuilder en :4025–4027; no se observaron en este archivo llamadas de creación de compile unit, archivos, subprogramas, ubicaciones o variables. La existencia de DIBuilder no demuestra que el objeto contenga símbolos de fuente o que exista PDB; contrastar otras rutas, productor/enlazador y artefactos reales en una fase posterior. No extrapolar al backend principal. LK-499.

### LKF-218 — Alcance del diagnóstico JSON autoalojado

**Componente:** compilador self-hosted, clase DiagnosticRenderer. **Estado:** esquema visible en fuente; consumidores y otras rutas sin contrastar. **Evidencia:** A:/nsc-devel/selfhost/nsc/Diagnostics/Diagnostics.ns:13–32 y :99–107, SHA-256 178987f61ab461809057b4aac93be7661a44fa176701b9e365de3b8c02a57388. Diagnostic conserva código, severidad, argumento y exitCode; Json emite schema/version, código, severidad y argumento, sin serializar exitCode, ubicación, rango o sugerencia en este método. No se afirma que los demás diagnósticos o backends carezcan de esos datos. Contrastar con productores de parser y semántica, identificar identidad de archivo y complementar con fuente sólo cuando exista evidencia. LK-500.

### LKF-219 — Ruta sin escape visible en source-info JSON self-host

**Componente:** compilador self-hosted, driver. **Estado:** riesgo visible en fuente; comportamiento con rutas concretas no reproducido. **Evidencia:** A:/nsc-devel/selfhost/nsc/Driver/Program.ns:79–100, SHA-256 433782d3d0902fbea63cd3cd35bc67fd97794d08deedce718e574852b93e057a. ReadSource concatena source.Path() directamente dentro de un valor JSON entre comillas, sin invocar en esta rama una rutina de escape. Si el valor preserva comillas, barras inversas o caracteres de control, la salida podría ser inválida o alterar la interpretación de la ruta; contrastar normalización de SourceBuffer.Path y parser consumidor antes de determinar casos alcanzables. LK-500.

### LKF-220 — Procedencia de unidades concatenadas en driver self-host

**Componente:** compilador self-hosted, driver. **Estado:** construcción visible; mapa de fuente y efectos en diagnósticos no verificados. **Evidencia:** A:/nsc-devel/selfhost/nsc/Driver/Program.ns:249–274, :294–310 y :347–364, SHA-256 433782d3d0902fbea63cd3cd35bc67fd97794d08deedce718e574852b93e057a. Estas rutas concatenan texto de unidades con saltos de línea y construyen un único SourceBuffer con FileId(1) y projectPath. En estos fragmentos no se conserva un mapa de offsets hacia cada unidad original. Investigar SourceGraph, frontend y cualquier mapa externo antes de decidir si un diagnóstico o símbolo puede recuperar la ubicación original; no adjudicar ciegamente la línea combinada a un archivo real. LK-501.

**Contraste por ruta:** Program.ns:519–553 (mismo SHA-256) describe y construye CheckProject por unidad, con FileId basado en orden y SourceBuffer cargado desde su ruta. Esa ruta de lexer/parser es diferente de las rutas concatenadas; no resuelve por sí sola el mapeo de la compilación de proyecto ni demuestra análisis semántico de todos los módulos. LK-501.

### LKF-221 — Alcance del MIR y verificador de cleanup self-host

**Componente:** compilador self-hosted, IR y verificador. **Estado:** alcance observado en fuente; no validado mediante ejecución. **Evidencia:** A:/nsc-devel/selfhost/nsc/IR/IntermediateRepresentation.ns:270–297, :330–343, :401–406 y :495–529, SHA-256 24471f21c8a6c14cbe9d2680b7b65952b88e04f1cf5e940080c05854f4b00cc9. Describe rotula toda IndexExpression con texto bounds.check; el verificador de cleanup examina únicamente que una operación textual cleanup.if-live vaya seguida de otra limpieza o return. Esas cadenas no prueban que se haya emitido una guarda ni que toda ruta libere correctamente; el backend LLVM revisado calcula ciertos índices sin guarda explícita visible (LKF-213). Contrastar IR con código generado, salidas implícitas y todo el grafo antes de concluir cobertura o defecto de ejecución. LK-502.

### LKF-222 — Alcance de SSA y dominancia self-host

**Componente:** compilador self-hosted, IR. **Estado:** alcance visible en fuente; sin ejecución. **Evidencia:** A:/nsc-devel/selfhost/nsc/IR/IntermediateRepresentation.ns:718–751 y :764–820, SHA-256 24471f21c8a6c14cbe9d2680b7b65952b88e04f1cf5e940080c05854f4b00cc9. El volcado Ssa imprime una línea phi para locales no direccionados en bloques con varios predecesores, sin mostrar valores entrantes; Prepare y Verify para dominancia/SSA construyen dominancia y contrastan su cantidad con el número de bloques, además de verificaciones HIR/flujo comunes. Ese resultado no demuestra por sí solo renombrado SSA, validez de entradas phi o dominancia semántica completa; contrastar otras etapas, alcance del subconjunto y artefactos antes de afirmar un defecto funcional. LK-503.

### LKF-223 — Estado especulativo al reconocer casts self-host

**Componente:** compilador self-hosted, parser. **Estado:** riesgo identificado por lectura parcial; sin reproducción. **Evidencia:** A:/nsc-devel/selfhost/nsc/Parser/Parser.ns:52–62, :150–160, :162–225 y :356–364, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661. LooksLikeCast guarda y restaura index pero invoca ParseType, que puede actualizar errors, firstErrorIndex, pendingGenericGreater y lastSemanticType. Investigar si una exploración fallida o genéricos anidados dejan estado que afecte el parseo posterior; verificar rutas alcanzables, recuperación y comportamiento normal antes de concluir un error. LK-505.

**Contraste adicional:** Parser.ns:485–505 (mismo SHA-256) restaura index, pendingGenericGreater, errors y firstErrorIndex cuando falla la exploración de argumentos genéricos de llamada. Esa lógica es distinta de LooksLikeCast y no demuestra por sí sola su corrección; lastSemanticType también requiere seguimiento. LK-505.

### LKF-224 — Decodificación de literales raw self-host

**Componente:** compilador self-hosted, lexer y parser. **Estado:** discrepancia estática por contrastar; no reproducida. **Evidencia:** A:/nsc-devel/selfhost/nsc/Lexer/Lexer.ns:299–349, SHA-256 6dc00c0f62a0695bcb62bdb3bd36e05e72a461375557540ce4d91fdd1811266c; A:/nsc-devel/selfhost/nsc/Parser/Parser.ns:302–346, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661. El lexer reconoce comillas triples como StringLiteral, mientras LiteralExpression llama DecodeString para ese tipo y el método visible procesa desde el segundo hasta el penúltimo carácter y trata barras inversas como escapes. Contrastar el lexema exacto, semántica esperada de raw y rutas de parseo antes de concluir el resultado observable. LK-506.

### LKF-225 — Lookahead de declaraciones ante entrada incompleta self-host

**Componente:** compilador self-hosted, parser. **Estado:** riesgo estático, sin reproducción. **Evidencia:** A:/nsc-devel/selfhost/nsc/Parser/Parser.ns:717–750, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661; TokenStream.ns:30–33, SHA-256 c85226eeb2d3f05f08bed5f4ca46950e491eebf8020a4d78c906de7532b3f4df. LooksLikeVariable consulta KindAt(cursor) y KindAt(cursor+1) directamente; algunos recorridos limitan cursor con Count, pero accesos posteriores no muestran guarda local. Contrastar si los llamadores, el token EOF y el indexador de List impiden índices fuera de rango ante genéricos o arreglos incompletos; no afirmar lectura indebida sin reconstruir ruta o ejecutar repro. LK-507.

### LKF-226 — Bases adicionales en parser self-host

**Componente:** compilador self-hosted, parser. **Estado:** descarte visible en fuente; repercusión semántica sin reproducir. **Evidencia:** A:/nsc-devel/selfhost/nsc/Parser/Parser.ns:1112–1123, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661. ParseAggregate guarda el primer tipo tras dos puntos en la declaración y consume los tipos adicionales en una variable ignoredBase sin agregarlos visiblemente al nodo. Contrastar árbol, análisis semántico y límites admitidos por tipo (clase, struct, interfaz) antes de afirmar qué combinaciones fallan o si otra representación recupera esos datos. LK-508.

### LKF-227 — Progreso de recuperación en bloque extern C self-host

**Componente:** compilador self-hosted, parser. **Estado:** posible ciclo ante entrada inválida; sin reproducción. **Evidencia:** A:/nsc-devel/selfhost/nsc/Parser/Parser.ns:52–56, :162–177, :1296–1301 y :1419–1460, SHA-256 18d52010d0ca06a6073f0413510312f41f1442bc9f77ce3a71e76827c69a5661. El bucle de declaraciones en ParseExternC invoca ParseGlobal sin guarda explícita de avance, mientras Expect sólo registra error si no encuentra el token y ParseType puede regresar sin consumir. Contrastar entradas que dejen el índice inalterado, EOF y estructuras anidadas; a diferencia de ParseCompilationUnit y ParseNamespace, no se ve aquí el respaldo before/index. No afirmar bloqueo observado. LK-509.

### LKF-228 — Carga y verificación de proyectos self-host

**Componente:** compilador self-hosted, cargador de proyectos. **Estado:** límites visibles en fuente; compatibilidad completa sin reproducir. **Evidencia:** A:/nsc-devel/selfhost/nsc/Projects/ProjectSystem.ns:80–118 y :178–210, SHA-256 39504e9997c97bd9c697e062a27e8e9caa24a5f11487cc349c77056f622ca2ed. Los atributos se buscan como cadenas con comillas dobles y Verify omite la comprobación individual de includes que contienen .. o **. Analizar sintaxis de proyecto admitida, entidades, comillas simples, comentarios, expansión real y validación de SourceGraph antes de concluir impacto. LK-510.

**Contraste de SourceGraph:** SourceGraph.ns:245–284, SHA-256 635e13044dc7a024516af9c30b21faf520385415052c3569a8e7878aaaf200b5, rechaza includes con .. y expande patrones por raíz, sufijo y recursividad; el skip de ProjectLoader.Verify no significa que esas rutas terminen aceptadas. Su tratamiento de patrones adicionales requiere análisis propio. LK-510 y LK-512.

### LKF-229 — Alcance de clave y manifiesto self-host

**Componente:** compilador self-hosted, metadatos de proyecto. **Estado:** implementación visible; uso efectivo de caché no contrastado. **Evidencia:** ProjectSystem.ns:237–256 y :275–298, mismo SHA-256. ProjectCache.Key combina hash del volcado de proyecto, perfil y target, sin leer directamente contenidos de fuentes en ese método; ToolchainManifest emite etiquetas fijas de capacidades. Contrastar callers, claves complementarias e invalidación, y no tratar etiquetas como prueba de instrumentación, versiones o binario efectivo. LK-511.

### LKF-230 — Inclusiones solapadas en SourceGraph self-host

**Componente:** compilador self-hosted, grafo de fuentes. **Estado:** riesgo de rechazo visible por inspección; no reproducido. **Evidencia:** A:/nsc-devel/selfhost/nsc/Projects/SourceGraph.ns:93–102, :121–126 y :268–284, SHA-256 635e13044dc7a024516af9c30b21faf520385415052c3569a8e7878aaaf200b5. AddUnit deduplica rutas existentes y ExpandFrom declara que un patrón no coincide si el total de unidades no aumenta. Si todos los archivos de un patrón ya fueron añadidos por otro include, la ruta podría reportar cero coincidencias pese a haberlos encontrado; contrastar orden, nombres y comportamiento efectivo. LK-512.

### LKF-231 — Prefijo de ruta relativa en SourceGraph self-host

**Componente:** compilador self-hosted, procedencia de fuentes. **Estado:** riesgo estático, no reproducido. **Evidencia:** SourceGraph.ns:26–44, mismo SHA-256. Relative utiliza StartsWith del directorio normalizado sin verificar un límite de componente antes de extraer el sufijo; si una ruta alcanza este método con un prefijo textual compartido pero fuera del directorio, podría mostrar una ruta relativa incorrecta. Determinar si normalización y construcción de rutas hacen alcanzable ese caso antes de concluir impacto. LK-512.

### LKF-232 — Estado y problemas de ModuleSet self-host

**Componente:** compilador self-hosted, análisis de módulos. **Estado:** ruta visible; ejecución y consumidores adicionales no contrastados. **Evidencia:** A:/nsc-devel/selfhost/nsc/Sema/ModuleSet.ns:121–143, :256–263, :437–509 y :517–576, SHA-256 217fb52000372cacc5092c91d5cc28a65203bf1552a91afbc06de31353f5de87; Driver/Program.ns:491–498, SHA-256 433782d3d0902fbea63cd3cd35bc67fd97794d08deedce718e574852b93e057a. ModuleIndex acumula problemas por duplicados; Failed consulta diagnostic y Dump muestra status=ok con problems separado, mientras el driver devuelve éxito si Failed es falso. Determinar si otra etapa convierte esos problemas en errores, y si el comando está pensado como diagnóstico informativo; no presentar status=ok como ausencia de problemas o compilación válida. LK-513.

### LKF-233 — Nombre no resuelto en expresión self-host

**Componente:** compilador self-hosted, análisis semántico. **Estado:** comportamiento local visible; política completa y efectos sin reproducir. **Evidencia:** A:/nsc-devel/selfhost/nsc/Sema/SemanticAnalyzer.ns:469–482, SHA-256 3a608609a5d0d438965580415d1d7ee8423cf90371d0360d28f177352b54ab22; Driver/Program.ns:519–524, SHA-256 433782d3d0902fbea63cd3cd35bc67fd97794d08deedce718e574852b93e057a. Ante IdentifierExpression sin símbolo, esta rama asigna tipo error y símbolo negativo sin llamar AddError; un comentario del driver describe históricamente ausencia de diagnóstico para nombre no resuelto. Verificar propagación y diagnósticos en otras fases y contextos antes de afirmar aceptación de programas inválidos. LK-514.

### LKF-234 — Alcance de noheap self-host

**Componente:** compilador self-hosted, análisis semántico. **Estado:** política visible en fuente; cobertura y efectos no reproducidos. **Evidencia:** A:/nsc-devel/selfhost/nsc/Sema/SemanticAnalyzer.ns:889–919, SHA-256 3a608609a5d0d438965580415d1d7ee8423cf90371d0360d28f177352b54ab22. ValidateNoHeap detecta asignaciones directas seleccionadas y llamadas resueltas sin contrato noheap, pero en esa rama exime extern C y no agrega error para target negativo. Contrastar verificaciones complementarias, semántica del contrato y efectos de callbacks o llamadas indirectas; un noheap aceptado no prueba que código externo no asigne. LK-515.

### LKF-235 — Alcance de escapes de referencias self-host

**Componente:** compilador self-hosted, análisis semántico. **Estado:** verificador visible; cobertura global no comprobada. **Evidencia:** SemanticAnalyzer.ns:846–887 y :922–946, mismo SHA-256. ValidateEscapes revisa en retornos una expresión unaria & y utiliza el primer símbolo de almacenamiento; no se demuestra en esta rutina seguimiento completo de alias, vistas borrowed, campos o llamadas. Contrastar otras validaciones y backend antes de afirmar que algún escape concreto sea aceptado o rechazado. LK-516.

### LKF-236 — Literales raw en preprocesamiento self-host

**Componente:** compilador self-hosted, procesador comptime. **Estado:** riesgo de desincronización léxica; no reproducido. **Evidencia:** A:/nsc-devel/selfhost/nsc/Source/ComptimeProcessor.ns:207–249, SHA-256 2fe16308751b0c7bc92337fc98a545d7defeffa347685cdd95356a846fc26ef9; Lexer.ns:299–349, SHA-256 6dc00c0f62a0695bcb62bdb3bd36e05e72a461375557540ce4d91fdd1811266c. Transform reconoce cadenas por comillas individuales y escapes sin un estado explícito para delimitadores raw variables, mientras el lexer sí reconoce comillas triples. Contrastar raw con comillas interiores, barras y texto #run/comptime y determinar si el preprocesador altera contenido literal; no afirmar defecto ejecutado. LK-517.

### LKF-237 — Saltos de línea dentro de #run autoalojado

**Componente:** compilador self-hosted, procesador comptime. **Estado:** transformación visible; mapeo posterior sin contrastar. **Evidencia:** ComptimeProcessor.ns:257–287, mismo SHA-256. Un #run evaluable se reemplaza por número y espacios hasta conservar el ancho; los espacios no preservan saltos de línea que estén dentro del tramo reemplazado. Si un #run multilinea alcanza esa ruta, el offset puede conservarse mientras cambian las líneas del texto transformado. Contrastar si diagnósticos usan texto original o transformado y si dicha sintaxis está admitida. LK-517.

### LKF-238 — Tipo sbyte en tabla self-host

**Componente:** compilador self-hosted, tabla de tipos. **Estado:** discrepancia visible en fuente, sin reproducción. **Evidencia:** A:/nsc-devel/selfhost/nsc/Types/TypeSystem.ns:6–10, :56–75 y :108–110, :212–215, SHA-256 888a9e86557786aa80c8f798390b74fb69c853643f071a25c4fc79eb6e48c406; Lexer/Lexer.ns:102–107 y CodeGen/LlvmBackend.ns:291–296. El lexer reconoce sbyte y el backend lo trata como entero de ocho bits, pero TypeTable no declara builtin o SemanticTypeKind específico y su fallback lo interna como Named. Contrastar conversiones, layout y rutas de inicialización antes de concluir el comportamiento de programas. LK-518.

### LKF-239 — Cálculo de tamaño de arreglo self-host

**Componente:** compilador self-hosted, tabla de tipos. **Estado:** riesgo estático sin reproducción. **Evidencia:** TypeSystem.ns:189–209, mismo SHA-256. Intern obtiene count de un texto y calcula (int)(count * tamaño de elemento) sin guarda explícita visible en esa rama; también usa count==0 para layout de referencia no dimensionada. Contrastar validación anterior, valores negativos/cero, límites y semántica de overflow antes de atribuir layout incorrecto a un programa. LK-518.

### LKF-240 — Resultado de rollback de artefactos

**Componente:** compilador principal, publicación de artefactos. **Estado:** límite de reporte visible; fallo no reproducido. **Evidencia:** A:/nsc-devel/src/Basic/ArtifactTransaction.cpp:124–128, :154–159 y :408–433, SHA-256 b8c21633752901d7c1af843448049ab760b68925aab9d98e912f4ec1422ed153. rollbackCommit marca rollbackFailed si falla restauración de backup, pero removeNoThrow ignora errores al eliminar destino instalado y temporal. Por ello rollback-complete no demuestra por sí solo que esas eliminaciones se hayan realizado. Contrastar plataformas, callers y estado final mediante repro futura. LK-519.

### LKF-241 — Nombres de staging de publicación concurrente

**Componente:** compilador principal, publicación de artefactos. **Estado:** riesgo de colisión estático; sin ejecución concurrente. **Evidencia:** ArtifactTransaction.cpp:18, :296–299, :333–335 y :450–453, mismo SHA-256. El sufijo de hermanos de destino usa ordinal atómico del proceso sin PID u otro identificador entre procesos; en Windows se intentan eliminar rutas preexistentes con esos nombres. Investigar bloqueo externo y coordinación de callers al publicar el mismo destino desde procesos distintos antes de concluir colisión o pérdida. LK-519.

### LKF-242 — Escape de nombres en referencias IR textuales

**Componente:** compilador principal, identidad de símbolos. **Estado:** riesgo condicionado a entradas alcanzables; no reproducido. **Evidencia:** A:/nsc-devel/src/Compilation/TypeIdentity.cpp:35–66, SHA-256 b92f0b194a1ba68be00462d279a91e6b7796da0cc86d2136a0c3a242c9d38f93. irTextSymbolReference añade comillas cuando el nombre las necesita pero copia su contenido sin escape explícito. Investigar si los callers permiten comillas, barras inversas u otros bytes que requieran escape LLVM, o si siempre reciben nombres previamente codificados; no afirmar IR inválido sin verificar el contrato de entrada. LK-521.

### LKF-243 — Ausencia de métricas y escape de telemetría

**Componente:** compilador principal, telemetría. **Estado:** comportamiento visible en fuente; impacto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/CompilationTelemetry.cpp:26–44, :162–171 y :235–277, SHA-256 c7b667bced27f47de56f385888cbb25fd2bfe251a54128beb3ff29d194b23e75. jsonEscape descarta controles menores a 0x20 salvo salto, retorno y tabulador; mediciones ausentes o fallidas parten de cero y currentProcessPeakWorkingSetBytes devuelve cero si no obtiene el dato. Sin campos adicionales, el consumidor no puede distinguir siempre cero medido de ausencia. Contrastar convenciones del formato, fuentes y casos alcanzables antes de cambiar su contrato. LK-522.

### LKF-244 — Opciones explícitas de depuración por acción

**Componente:** compilador principal, invocación. **Estado:** restricción visible en fuente; no reproducida en binario actual. **Evidencia:** A:/nsc-devel/src/Driver/CompilerInvocation.cpp:170–188 y :191–217, SHA-256 de90ddb4980ecbf8a05c9cf52b4b4276fa1791f5264aaa6ac8b436514a84f33a. La validación acepta opciones explícitas de información debug, mapas de prefijo y perfil para las acciones OOP de proyecto obj/exe/lib/dll con la forma clasificada; rechaza esas opciones en otras acciones. Esto no demuestra que todas las acciones admitidas produzcan PDB ni determina el comportamiento de opciones por defecto. LK-523.

### LKF-245 — Advertencia ThinLTO antes de defaults de proyecto

**Componente:** compilador principal, parser de opciones. **Estado:** secuencia visible en fuente; efecto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/CompilerOptionParser.cpp:831–843, SHA-256 6829ac903424f5e67dd33e3d6dba68d066d199d73912db47461fcb1c08a62961; CompilerInvocation.cpp:257–282, SHA-256 de90ddb4980ecbf8a05c9cf52b4b4276fa1791f5264aaa6ac8b436514a84f33a. La advertencia de ThinLTO con O0 se evalúa en parseCompilerOptions antes de aplicar defaults del proyecto y perfil. Si el nivel implícito inicial O0 cambia después a O1–O3, contrastar si se emite una advertencia que no corresponde a la configuración efectiva; investigar el consumo real de optimización antes de afirmarlo. LK-523.


### LKF-246 — Limpieza no informada de sidecar ThinLTO anterior

**Componente:** compilador principal, publicación de biblioteca estática. **Estado:** comportamiento visible en fuente; consecuencia no reproducida. **Evidencia:** A:/nsc-devel/src/Driver/DriverArtifactGeneration.cpp:965–982, SHA-256 14b0d0e88bb85dd29a860d38d11ed0d013c25463aaa10bc204cb8fc1f4f03288. Tras publicar correctamente una biblioteca que no preparó sidecar ThinLTO, se intenta eliminar el sidecar previo con error_code pero no se comunica un fallo y se devuelve éxito. Si la eliminación falla, podría permanecer un archivo obsoleto; el comentario indica que se rechazaría, pero aún hay que contrastar el lector y su identidad antes de garantizarlo. Contraste: ThinLtoLibraryLinking.cpp:119–146 y :255–297, SHA-256 5507b2883b1a86fac748376a5dc4008a7691bb5f43b342aafe2867c1b4c412f9, compara compatibilidad y digests de biblioteca y metadata y registra fallback al rechazar el sidecar; si los contenidos siguen coincidiendo, esa comprobación por sí sola no identifica su antigüedad. Investigar permisos, archivos bloqueados y restauración de caché; no afirmar corrupción ni uso incorrecto. LK-524 y LK-544.


### LKF-247 — Descripción incompleta de acciones de depuración en ayuda inglesa

**Componente:** compilador principal, ayuda de consola. **Estado:** inconsistencia visible en fuente; salida de binario no ejecutada. **Evidencia:** A:/nsc-devel/src/Driver/DriverHelp.cpp:61–76 y :297–300, SHA-256 ecdaf2758422c084c2ecc726c1dfbd3ab504a4df75e14369bd2716c3e4447751; A:/nsc-devel/src/Driver/CompilerInvocation.cpp:170–188, SHA-256 de90ddb4980ecbf8a05c9cf52b4b4276fa1791f5264aaa6ac8b436514a84f33a. El texto explicativo en inglés dice que -g aplica a objetos/ejecutables OOP de proyecto, pero el uso anuncia opciones también para lib/dll y la clasificación de invocación incluye las cuatro acciones. La discrepancia puede confundir al seleccionar símbolos; no demuestra que cada producto tenga PDB ni que sus símbolos sean completos. Corregir o precisar la ayuda tras contrastar el comportamiento real y las variantes de idioma. LK-525.


### LKF-248 — Aceptación parcial de números JSON

**Componente:** compilador, comandos de proyecto y lector JSON interno. **Estado:** lectura estática; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/Internal/JsonParser.cpp:72–88, SHA-256 19d2ac32f8edf0e665a1abb0b9ed863b3cfdffd3a20f837b486dd460365a25f1; A:/nsc-devel/src/Driver/ProjectCommands.cpp:94–120, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. El lector avanza por dígitos, punto, exponentes y signos sin validar la gramática completa y usa std::stod sin comprobar cuántos caracteres consumió. Tokens como 1e o 1-2 podrían aceptarse por conversión parcial; también investigar ceros iniciales, dependencia de locale y rangos. Los efectos dependen del esquema y consumidor; no se ha ejecutado ni demostrado alteración de proyectos. LK-529.

### LKF-249 — Escapes Unicode JSON con pares sustitutos

**Componente:** compilador, comandos de proyecto y lector JSON interno. **Estado:** comportamiento visible en fuente; efecto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/Internal/JsonParser.cpp:138–168, SHA-256 19d2ac32f8edf0e665a1abb0b9ed863b3cfdffd3a20f837b486dd460365a25f1. Cada escape \uXXXX se codifica por separado como hasta tres bytes, sin combinar pares sustitutos UTF-16 ni rechazar sustitutos aislados. Una cadena JSON válida con caracteres suplementarios escapados puede producir bytes que no representan UTF-8 válido. Contrastar efectos en rutas, nombres y consumidores reales; sin ejecución. LK-529.

### LKF-250 — Profundidad de anidación JSON sin límite explícito

**Componente:** compilador, comandos de proyecto y lector JSON interno. **Estado:** riesgo estático; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/Internal/JsonParser.cpp:37–45, :178–205 y :220–237, SHA-256 19d2ac32f8edf0e665a1abb0b9ed863b3cfdffd3a20f837b486dd460365a25f1; ProjectCommands.cpp:94–115, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. Valores objeto y arreglo se analizan recursivamente sin límite de profundidad visible en este lector. El consumidor revisado limita inicialmente el tamaño del archivo a 4 MiB, lo cual no es un límite explícito de profundidad; investigar si una entrada alcanzable puede agotar pila y qué otras restricciones aplican. LK-529.


### LKF-251 — Deduplicación de validación por ruta de archivo de biblioteca

**Componente:** compilador principal, validación de referencias precompiladas. **Estado:** riesgo condicionado a referencias alcanzables; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/LibraryReferenceValidation.cpp:376–397, :419–430 y :456–485, SHA-256 d480d1816348ef1f099e3a77cf04dccc8ae8c3c9d7e60f6cc03972cb5e4c6922. La validación guarda como visitada la clave normalizada de libraryPath y retorna antes de examinar metadataPath y runtimePath en apariciones posteriores de la misma biblioteca. Investigar si los cargadores o el grafo prohíben de antemano que una misma biblioteca aparezca con metadata o runtime diferentes; si no, contrastar si la segunda combinación evade una comprobación necesaria. No se afirma que el caso sea alcanzable ni que una DLL incorrecta se enlace. LK-530.

### LKF-252 — Evidencia heurística de runtime Unicode en PE

**Componente:** compilador principal, validación de runtimes enlazados. **Estado:** límite visible en fuente; impacto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/LibraryReferenceValidation.cpp:674–704 y :708–723, SHA-256 d480d1816348ef1f099e3a77cf04dccc8ae8c3c9d7e60f6cc03972cb5e4c6922. Para un nombre no ASCII, la presencia en cualquier parte del PE de los bytes UTF-16LE del nombre terminado en NUL se trata como indicio de alcanzabilidad del runtime. Esa coincidencia por sí sola no demuestra que el cargador Unicode la utilice ni que la DLL se cargue; investigar restricciones de generación y otras comprobaciones antes de valorar un falso positivo alcanzable. LK-530.


### LKF-253 — Marcadores de migración detectados sin contexto léxico

**Componente:** compilador principal, análisis de migración. **Estado:** comportamiento visible en fuente; impacto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/MigrationAnalyzer.cpp:168–191 y :357–371, SHA-256 27c00f5fae29956c35af6ef9351b6b2a9e916f2bfecf6c486a36c70ce25905cd. Se busca el marcador de archivo generado en los primeros 512 bytes y el marcador de supresión en texto crudo de cada línea, sin exigir que aparezcan en comentarios. Una cadena o texto no destinado a instrucción podría activar el tratamiento de generado o suprimir hallazgos de la línea siguiente. Contrastar contrato esperado y casos alcanzables antes de afirmar resultados incorrectos. LK-531.

### LKF-254 — Comprobación léxica de ruta de salida de migración

**Componente:** compilador principal, aplicación de correcciones de migración. **Estado:** riesgo condicionado; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/MigrationAnalyzer.cpp:483–535, SHA-256 27c00f5fae29956c35af6ef9351b6b2a9e916f2bfecf6c486a36c70ce25905cd. La función rechaza sourcePath == destinationPath, pero esa comparación no demuestra que rutas distintas no designen el mismo archivo; tras leer el origen abre el destino con truncamiento. Investigar rutas relativas, normalización, enlaces y restricciones de callers para determinar si una salida equivalente al origen es alcanzable y su efecto. No se ha modificado ni ejecutado el producto. LK-531.


### LKF-255 — Alcance de reproducibilidad de manifiestos de instalación

**Componente:** herramientas de instalación del compilador. **Estado:** comportamiento visible en fuente; no ejecutado. **Evidencia:** A:/nsc-devel/src/Driver/PackageLifecycle.cpp:152–155 y :716–773, SHA-256 7619fb3e1a945b760eabd4591a89ca9cea6434911c3f2183bf99f5260ca9e648. El comparador marca bin/nsc.exe y bin/nsc.pdb como identidades volátiles y no compara su contenido cuando ambas rutas existen; puede informar reproducible si sólo difieren esos archivos. Es una exclusión explícita del criterio, no prueba de igualdad del compilador ni del PDB. Documentar el alcance para los consumidores y usar hash/identidad efectivos cuando se necesite correlación. LK-532.

### LKF-256 — Archivos temporales de ciclo de instalación con nombres fijos

**Componente:** herramientas de instalación del compilador, publicación de planes. **Estado:** riesgo de concurrencia condicionado; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PackageLifecycle.cpp:157–175, :179–200 y :244–270, SHA-256 7619fb3e1a945b760eabd4591a89ca9cea6434911c3f2183bf99f5260ca9e648. La escritura usa hermanos con sufijos fijos .tmp y .previous y elimina rutas preexistentes; investigar bloqueos externos, colisiones entre procesos y propiedad de esos nombres antes de concluir pérdida o corrupción. Si falla la eliminación del respaldo después de publicar, se informa error aun cuando el destino ya se publicó (:236–241). LK-532.

### LKF-257 — Retiro de instalación sin rollback de archivos ya eliminados

**Componente:** herramientas de instalación del compilador. **Estado:** secuencia visible en fuente; fallo parcial no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PackageLifecycle.cpp:588–633, :665–680 y :790–818, SHA-256 7619fb3e1a945b760eabd4591a89ca9cea6434911c3f2183bf99f5260ca9e648. Se prevalidan destinos, pero la eliminación se realiza secuencialmente y termina al primer error sin restaurar los archivos previamente eliminados. Investigar contrato de recuperación y concurrencia entre verificación y eliminación; distinguir error con estado parcial de operación no iniciada. LK-532.


### LKF-258 — Identidad de bundle incompleta para sesiones Clang C++

**Componente:** compilador principal, caché de bundles e interoperabilidad Clang. **Estado:** ruta visible en fuente; efecto observable no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PrecompiledModuleConfiguration.cpp:347–361, :395–400 y :469–524, SHA-256 0b941ed6d005f3076ddf490bdb9b5d4dbf82c943a3d90d85b3626d3b1b83a052. Al capturar entradas para caché, una sesión cuyo idioma no es C marca cppInputsComplete=false; la configuración de raíz de bundle y el cálculo de identidad en este punto sólo ocurren si esa bandera permanece verdadera. Contrastar CompilationBundleCache y otros callers para determinar exactamente el resultado de lookup y reutilización en cada configuración; no extrapolarlo a todas las cachés ni afirmar un fallo de compilación. Contraste adicional: ProjectCommands.cpp:1618–1626, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4, añade --no-artifact-cache en build de proyecto si el grafo contiene CppLibraryReference, independientemente del idioma efectivo de la sesión; corresponde a esa ruta de invocación. LK-533.

### LKF-259 — Fallos de publicación de bundle no comunicados

**Componente:** compilador principal, caché de bundles. **Estado:** comportamiento visible en fuente; impacto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PrecompiledModuleConfiguration.cpp:682–702, SHA-256 0b941ed6d005f3076ddf490bdb9b5d4dbf82c943a3d90d85b3626d3b1b83a052. publishCompilationBundle ignora el resultado y el mensaje de store y captura todas las excepciones; una compilación puede continuar sin comunicar una publicación de caché fallida. La política podría ser deliberada para mantener la compilación funcional; contrastar telemetría externa y contrato antes de considerarlo defecto. No implica por sí sola corrupción ni éxito del almacenamiento. LK-533.


### LKF-260 — Valores de carácter predeterminados no reconstruibles desde metadatos

**Componente:** compilador principal, materialización de superficie precompilada. **Estado:** restricción visible en fuente; no reproducida en compilación. **Evidencia:** A:/nsc-devel/src/Driver/PrecompiledSurface/ContractMaterialization.cpp:621–675, SHA-256 5fab80ace46f0cd06e27231daf5280294a68d5980635ec99bec68488a0c669e3. La reconstrucción de un parámetro Character admite barra invertida, comilla, salto, retorno, tabulador y ASCII imprimible; para otros valores devuelve error, incluido carácter no ASCII. Investigar si el productor y los validadores admiten esos valores en parámetros predeterminados y qué rutas de consumo invocan esta función; no afirmar que todo carácter no ASCII sea inválido en NativeSharp. LK-534.

### LKF-261 — Representación de constantes flotantes reconstruidas

**Componente:** compilador principal, materialización de superficie precompilada. **Estado:** riesgo condicionado a valores y configuración alcanzables; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PrecompiledSurface/ContractMaterialization.cpp:403–425 y :639–647, SHA-256 5fab80ace46f0cd06e27231daf5280294a68d5980635ec99bec68488a0c669e3. La fuente se forma con ostringstream y se agrega .0 si el texto no contiene punto ni exponente, sin tratamiento explícito visible para valores no finitos o locale. Contrastar si los productores admiten NaN/infinito, qué locale tiene el proceso y qué sintaxis acepta el parser; no afirmar una falla alcanzable sin esos contratos. LK-534.


### LKF-262 — Tipos de función y genéricos abiertos no materializados desde NSMETA

**Componente:** compilador principal, reconstrucción de fuente precompilada. **Estado:** restricciones visibles en fuente; alcance de entradas y efecto no reproducidos. **Evidencia:** A:/nsc-devel/src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp:173–188 y :222–239, SHA-256 46151a5743e7467ee2500ff152ef7c1eed2c2a0f4b9ae84cbdc940c4694e170e. El mapeo rechaza determinadas identidades de función, incluidos carrier distinto de puntero y atributos como unsafe, noheap, async, nullable, lifetime o error, y rechaza escribir directamente claves de tipo genérico abierto con aridad. Contrastar productores, validadores y rutas alternativas de plantillas antes de extrapolar la restricción a toda interoperabilidad o a los genéricos cerrados. LK-535.

### LKF-263 — Claves de ruta reducidas byte a byte

**Componente:** compilador principal, identidad de referencias y rutas. **Estado:** riesgo dependiente de nombres, locale y sistema de archivos; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PrecompiledSurface/MetadataSourceMapping.cpp:26–39, SHA-256 46151a5743e7467ee2500ff152ef7c1eed2c2a0f4b9ae84cbdc940c4694e170e; consumidores por ejemplo LibraryReferenceValidation.cpp:390–392 y :479–485, SHA-256 d480d1816348ef1f099e3a77cf04dccc8ae8c3c9d7e60f6cc03972cb5e4c6922. foldedPathKey normaliza léxicamente, codifica UTF-8 y aplica std::tolower a cada byte; esa operación no es un cotejo Unicode ni prueba equivalencia de archivos. Investigar rutas no ASCII, variación de locale, enlaces y sensibilidad del volumen, y si pueden causar deduplicación incorrecta en callers concretos. No se afirma colisión ni comportamiento reproducido. LK-535.


### LKF-264 — Diagnóstico y rollback de restauración de paquetes

**Componente:** compilador principal, comandos de proyecto y restauración de paquetes. **Estado:** riesgo condicionado a errores de E/S; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:642–691, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. Ante fallos al copiar o mover, la limpieza reutiliza fileError antes de construir parte del mensaje; al fallar la publicación se intenta restaurar backup, pero rollbackError no se informa y luego se reutiliza en la limpieza. También se ignora el error al eliminar el respaldo después del éxito. Investigar condiciones alcanzables y estado real ante cada fallo; no afirmar pérdida sin reproducción. LK-536.

### LKF-265 — Verificación de paquetes posterior a publicación

**Componente:** compilador principal, restauración de paquetes. **Estado:** secuencia visible en fuente; efecto condicionado no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:347–388, :642–691 y :719–761, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. Los hashes se calculan al preparar el plan; materialize copia y publica la carpeta y elimina el backup antes de que executeRestore verifique los archivos publicados. Si cambian las entradas o falla la verificación, investigar el estado de salida y recuperación disponibles; el comando sí comprueba los hashes después, por lo que no debe presentarse como ausencia total de verificación. LK-536.


### LKF-266 — Conversión de tamaño numérico del lockfile

**Componente:** compilador principal, lectura de lockfile de paquetes. **Estado:** riesgo visible en fuente; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:1137–1176, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4; JsonParser.cpp:72–88, SHA-256 19d2ac32f8edf0e665a1abb0b9ed863b3cfdffd3a20f837b486dd460365a25f1. El tamaño se guarda primero como double, se rechazan sólo valores negativos y se convierte a uintmax_t sin comprobar que sea entero ni esté en rango; la conversión de un flotante fuera del rango representable tiene comportamiento indefinido en C++. Un valor fraccionario se trunca y los enteros grandes pueden perder precisión en double. El hash y tamaño se verifican después, pero eso no evita la conversión previa. Contrastar restricciones de entrada y ejecutar reproducciones sólo en una fase autorizada. LK-537.

### LKF-267 — Rollback no comunicado de directorio Content (mismo riesgo que LKF-137)

**Componente:** compilador principal, publicación de contenido del proyecto. **Estado:** riesgo condicionado a error de E/S; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:910–945 y :967–987, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. Si falla el rename de staging, se intenta recuperar el directorio anterior pero no se comunica rollbackError; la limpieza inicial de staging y backup también descarta errores. Si falla la limpieza del backup después de publicar, se informa que ya se publicó. Investigar coordinación entre procesos, bloqueo y recuperación antes de afirmar pérdida. LK-537.


### LKF-268 — Valor de debug desconocido tratado como debug completo

**Componente:** compilador principal, configuración de build de proyecto. **Estado:** secuencia visible en fuente; efecto no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:74–83, :135–165 y :1313–1346, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. Sin opción CLI explícita, el sidecar selecciona -g0 para none y -gline-tables-only para line-tables-only, pero cualquier otro texto se traduce a -g; un campo de tipo distinto de string usa el valor por defecto full. Contrastar si existe una validación previa o posterior que limite los valores antes de concluir que una entrada incorrecta se acepta en todas las rutas. LK-538.


### LKF-269 — Reporte de dependencias no disponible con referencias C/C++

**Componente:** compilador principal, build de proyecto y reporte de procedencia. **Estado:** restricción visible en fuente; no ejecutada. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:1749–1765, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. Si el grafo contiene CppLibraryReference y se solicita --dependency-report, la ruta devuelve error indicando que falta seguimiento de cabeceras transitivas. No significa que toda compilación con C/C++ falle; afecta a esta combinación de acción y opción. Investigar mecanismos alternativos de captura y registrar como desconocida la clausura no observada, sin sustituirla por un reporte incompleto presentado como completo. LK-539.


### LKF-270 — Argumento vacío en nsc run en Windows

**Componente:** compilador principal, comando run. **Estado:** riesgo visible en fuente; no ejecutado. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:1801–1837, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. launch llama MultiByteToWideChar con el tamaño explícito del argumento y trata resultado <= 0 como UTF-8 inválido; para un argumento vacío la longitud suministrada es cero. Incluso si se resolviera esa conversión, quoteWindows devuelve sin comillas una cadena que no contiene espacios, tabuladores ni comillas, incluida la vacía. Contrastar cómo llega un argumento vacío a esta ruta y reproducir el comportamiento antes de atribuirlo a todas las entradas. Es una ruta distinta del depurador en LKF-138. LK-540.

### LKF-271 — Opción de formato retirada de argumentos del programa

**Componente:** compilador principal, comando run. **Estado:** secuencia visible en fuente; no ejecutada. **Evidencia:** A:/nsc-devel/src/Driver/ProjectCommands.cpp:265–273, :1026–1033 y :1859–1866, SHA-256 9660c9358ee49b730cfc1963462d324d7ec9baa44b52cdae6236635f13cfb4c4. runRun aplica stripOutputFormat a todos los argumentos antes de interpretar el separador --; la función elimina cualquier token igual a --output-format=json sin distinguir si corresponde al programa. Contrastar entrada de CLI completa y modos de salida, y diseñar reproducción con el token después de --. LK-540.


### LKF-272 — Accesores sólo de escritura no materializados desde metadatos

**Componente:** compilador principal, reconstrucción de superficie pública. **Estado:** restricción visible en fuente; alcanzabilidad no reproducida. **Evidencia:** A:/nsc-devel/src/Driver/PublicSurfaceBuilder.cpp:292–328 y :408–413, SHA-256 050d64b18de4acaf261b20b594a3c46b45df4735aa1d5bd971f3f24e07d7030b. La validación del contrato devuelve error cuando no hay getter, aun si existe setter. Contrastar si el lenguaje y los productores NSMETA pueden generar ese contrato y si otras rutas lo filtran antes de materializarlo; no afirmar que cualquier propiedad de sólo escritura sea válida o falle en el compilador actual. LK-541.


### LKF-273 — Recorrido de agregados anónimos sin detección de ciclos local

**Componente:** compilador principal, reconstrucción de superficie precompilada. **Estado:** riesgo condicionado a metadatos aceptados por validadores previos; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PublicSurfaceBuilder.cpp:781–807 y :838–864, SHA-256 050d64b18de4acaf261b20b594a3c46b45df4735aa1d5bd971f3f24e07d7030b. emitAnonymousCarrier usa una pila explícita y vuelve a agregar cada carrier hijo sin control local de nodos activos ni profundidad; un ciclo de carriers aceptado por las capas previas impediría terminar el recorrido y aumentaría memoria. Investigar invariantes del codec y validadores de layout, tamaño y ciclos antes de afirmar que una entrada así es alcanzable. LK-542.


### LKF-274 — Descubrimiento incremental de tipos de contrato durante una sola pasada

**Componente:** compilador principal, reconstrucción de superficie precompilada. **Estado:** riesgo condicionado a orden y metadatos alcanzables; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/PublicSurfaceBuilder.cpp:1313–1335, :1338–1388 y :1786–1797, SHA-256 050d64b18de4acaf261b20b594a3c46b45df4735aa1d5bd971f3f24e07d7030b. collectPublishedMembers recorre miembros una vez, omite algunos cuyo tipo no es público ni está todavía en contractTypeIdentities, y añade nuevos tipos al conjunto al procesar miembros posteriores; publishContractTypes añade tipos después, sin volver a recorrer miembros en esta función. Investigar el orden normativo del codec, accesibilidad, cierre previo y consumidores para determinar si existe un caso válido donde un miembro necesario se omita. No afirmar un defecto funcional sin esa comprobación. LK-543.


### LKF-275 — Reutilización de archivo ThinLTO por clave abreviada y verificación estructural

**Componente:** compilador principal, enlace ThinLTO. **Estado:** riesgo condicionado a colisión o modificación externa; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ThinLtoLibraryLinking.cpp:165–173 y :183–217, SHA-256 5507b2883b1a86fac748376a5dc4008a7691bb5f43b342aafe2867c1b4c412f9. El directorio del archivo temporal usa los primeros 16 caracteres hexadecimales de SHA-256 calculado a partir de nombre y objetos; si ya hay un COFF que pasa usableArchive se reutiliza sin comparar aquí su contenido con los objetos esperados. Investigar aislamiento, amenazas aplicables y colisiones, además del alcance del verificador COFF, antes de afirmar que se reutilice un archivo incorrecto en condiciones normales. LK-544.

### LKF-276 — Dependencias ThinLTO identificadas por nombre de metadata

**Componente:** compilador principal, compatibilidad de sidecars ThinLTO. **Estado:** riesgo condicionado a referencias homónimas; no reproducido. **Evidencia:** A:/nsc-devel/src/Driver/ThinLtoLibraryLinking.cpp:34–43, :81–114 y :271–275, SHA-256 5507b2883b1a86fac748376a5dc4008a7691bb5f43b342aafe2867c1b4c412f9. La tabla metadataByName conserva la primera ruta por nombre de archivo reducido a minúsculas ASCII y dependenciesMatch compara el digest contra esa ruta. Si hay referencias distintas con igual nombre base, investigar restricciones previas y si el orden produce rechazo/fallback de una dependencia válida. No afirmar aceptación incorrecta ni que tales referencias sean permitidas. LK-544.


### LKF-277 — Restricción de propiedades e indexadores en asignación de coalescencia

**Componente:** compilador principal, análisis semántico de asignaciones. **Estado:** restricción documentada y comprobación visible en fuente; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:215–245, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385. La ruta de ??= exige almacenamiento asignable y direccionable, con excepción específica de un global volatile importado. El comentario indica que propiedades/indexadores requerirían capturar una sola vez el receptor para compartirlo entre getter y setter, algo que ese HIR no representa de forma segura; se emite diagnóstico para objetivos que no cumplan. Contrastar categorías especiales y lowering antes de generalizar a todos los indexadores. LK-545.


### LKF-278 — Deconstrucción de tuplas con vida no trivial

**Componente:** compilador principal, análisis semántico de asignaciones. **Estado:** restricción visible en fuente; no ejecutada. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:372–417, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385. Si existe un binding de deconstrucción y el tipo de tupla no tiene vida trivial, se emite error indicando que los elementos owning o move-only requieren bindings de movimiento y drop flags por elemento. La ruta inspeccionada evalúa fuente y valida destinos; contrastar declaraciones, otras rutas de deconstrucción y lowering antes de extrapolar la restricción a todo uso de tuplas. LK-546.


**Contraste de fuente nominal:** PatternAnalyzer.cpp:41–112 y :133–183, SHA-256 116f4dd029c22bb863ca3be8abc09e436a61cc801659b1576770fcba9ea793aa, selecciona un Deconstruct accesible, de instancia, no virtual, sin parámetros y con retorno tupla por valor; rechaza el retorno de vida no trivial y la fuente temporal no direccionable de vida no trivial en esta ruta. Es una comprobación adicional para deconstrucción nominal, no demuestra que se rechace toda deconstrucción de tuplas o que exista un fallo de ejecución. LK-546.


**Contraste de declaración:** StatementAnalyzer.cpp:1447–1497, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907, también rechaza en esta ruta la declaración por deconstrucción cuando la tupla tiene elementos de vida no trivial, antes de crear los bindings locales. El hallazgo cubre ahora esta ruta además de la asignación y el caso nominal; otras formas y lowering siguen pendientes. LK-546.


### LKF-279 — Procedencia nombrada en reasignación borrowed

**Componente:** compilador principal, análisis de propiedad y vida. **Estado:** restricción visible en esta ruta; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:1096–1125, :1128–1177 y :1252–1299, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385. Para reasignaciones borrowed clasificadas por esta ruta, collectBorrowedAssignmentOwners sólo recoge procedencia cuando el lado derecho es un identificador con dueño o préstamo rastreado; si no encuentra dueño local o parámetro se diagnostica error. IdentifierAnalyzer.cpp:418–465, SHA-256 029bc7ce6f74a2b4d3937365103416aef8ed21f5c3c975e154a60b179a80c750, comprueba si cualquiera de los posibles dueños fue movido pero propaga al resultado el primer local posible, o el primer parámetro si no hay locales. Contrastar consumidores de esa procedencia y el conjunto completo antes de atribuir pérdida de seguridad. Investigar expresiones equivalentes, bindings alternativos y contrato de lenguaje antes de concluir que toda reasignación borrowed no nombrada falla. LK-547.


### LKF-280 — Conversión en asignación a miembro con acceso condicional

**Componente:** compilador principal, análisis semántico de asignación. **Estado:** hipótesis estática; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:1811–1835, :1896–1945 y :2324–2393, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385. En finishAssignment, la asignación simple a NullConditionalMemberExpression devuelve el resultado antes de finishPlainAssignment y su validatePlainAssignmentConversion; la ruta NullConditionalIndexExpression llama una comprobación explícita de conversión. La preparación y el análisis previo pueden imponer otras restricciones: trazar ese flujo completo y HIR/MIR con tipos incompatibles y calificadores de puntero antes de afirmar aceptación incorrecta. LK-548.


### LKF-281 — Alternancia del candidato de respaldo ante nombres ambiguos

**Componente:** compilador principal, resolución de tipos en cuerpos. **Estado:** riesgo condicionado a visibilidad y callers; no reproducido. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/AssignmentAnalyzer.cpp:2814–2852, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385. resolveTypeIdentifier guarda el primer candidato de respaldo y borra fallback al encontrar otro; si hay un tercer candidato elegible y no se devolvió uno del proyecto y namespace del contexto, vuelve a llenarlo. El comentario dice no adivinar ante nombres ambiguos, pero la variable no conserva explícitamente un estado de ambigüedad permanente. Investigar qué candidatos entrega typeOrdinalsForName, filtros de visibilidad y callers para establecer si tres candidatos distintos son alcanzables y producen resolución incorrecta. LK-549.


### LKF-282 — Propiedad en packs expandidos de llamadas a funciones de namespace

**Componente:** compilador principal, análisis de llamadas. **Estado:** restricción visible en una ruta; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:464–487 y :539–627, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. En la evaluación de candidatos de funciones de namespace, un argumento considerado elemento de pack expandido vuelve inviable el candidato si lleva ownership, transferencia o propietario nuevo; el comentario explica que el span temporal no posee elementos y sugiere un span construido previamente. Contrastar normalización de un único argumento, métodos y otras rutas antes de extender esta conclusión a todos los packs. LK-551.


### LKF-283 — Órdenes atómicos y posición de argumentos nombrados

**Componente:** compilador principal, análisis semántico de llamadas atómicas. **Estado:** posible limitación estática; alcance y reproducibilidad pendientes, sin prueba de ejecución. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:100–160, :2177–2184, :2248–2251, :2358–2363, :2880–2906 y :5404–5418; SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. El mapeo de argumentos reconoce nombres y conserva su correspondencia con parámetros, pero las validaciones de compare-exchange obtienen los órdenes de posiciones fijas de expression->children(), la validación general toma el último hijo y la de fence el primero; applySelectedCoreOperations recibe el binding pero no lo usa. Si una llamada atómica admite argumentos nombrados reordenados y la sintaxis llega en orden escrito, una validación podría plegar el argumento equivocado, rechazar una llamada válida o asignar un orden distinto; faltan confirmar gramática, normalización y rutas posteriores. No se afirma que se acepte código inseguro ni que el fallo esté reproducido. Contrastar argumentos posicionales y nombrados permutados, valores por defecto y formas Interlocked/receptor antes de decidir una corrección. LK-553.


### LKF-284 — Captura de resto de arreglo requiere especialización Span materializada

**Componente:** compilador principal, análisis de patrones de lista. **Estado:** restricción visible en el código, no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:3134–3158 y :3210–3268; SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. Para la ruta sintética de captura de resto sobre arreglo nativo o fijo, el analizador busca la especialización cerrada Span<T> o ReadOnlySpan<T> según la mutabilidad del receptor; si no está materializada, emite InvalidOperand. Esto no demuestra que una especialización determinada falte en una compilación concreta ni cubre los receptores que resuelven Slice ordinario. Contrastar materialización y diagnóstico con las otras etapas y versiones; la vista se describe sin copia, por lo que su procedencia y vida deben investigarse separadamente. LK-555.


### LKF-285 — Restricciones de miembros generados para agregados

**Componente:** compilador principal, miembros sintetizados de agregados. **Estado:** restricciones visibles en validación estática; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:3397–3528, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd; A:/nsc-devel/src/Frontend/Bodies/ConstructionAnalyzer.cpp:140–269, SHA-256 9c34eb7bb50acf3545d04e4d4cf24c75bbe92394010a3ea524fa6c66a3ef75fb. La ruta consultada rechaza miembros sintetizados distintos de ToString para unions; al validar recursivamente Clone rechaza un agregado con destructor o campo owned porque el clon generado sería copia simple. Para igualdad y hash revisa además requisitos de derive en tipos anidados. Son reglas defensivas de esta ruta, no demostración de defecto ni prohibición de implementar operaciones propias. Investigar cobertura de tuplas, arreglos fijos, tipos anidados y las demás rutas antes de generalizar. LK-556.


### LKF-286 — Propiedad en callbacks estructurales

**Componente:** compilador principal, llamadas indirectas. **Estado:** límite local visible; otras rutas y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:3885–3955 y :4023–4054, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. rejectDelegateNewToOrdinaryParameters exige una declaración de delegate con sintaxis y omite funciones estructurales; en parámetros declarados sin calificador detecta creación fresca pasada por valor y reporta pérdida de propiedad. En otra comprobación conservadora, :4973–5010 trata los parámetros estructurales como prestados al propagar un posible alias de temporal; esto no es evidencia de propiedad efectiva del callback externo. Contrastar conversiones, contratos C y lowering antes de concluir que una llamada concreta se acepta o produce fuga. LK-558.

### LKF-287 — Alcance local del control de FnOnce

**Componente:** compilador principal, flujo de cierres. **Estado:** límite de la función examinada; otras rutas y casos reales pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:4142–4179, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. validateCallableTraitInvocation consulta el registro de invocación sólo cuando el destino es un identificador vinculado a un local; otras formas retornan sin esta comprobación concreta. Para ese local rechaza FnMut const y FnOnce ya o posiblemente invocado. No se concluye que campos, parámetros, aliases o temporales permitan otra invocación: investigar restricciones y comprobaciones adicionales. LK-559.


### LKF-288 — Diagnósticos de sobrecarga y argumentos nombrados

**Componente:** compilador principal, diagnósticos de llamadas. **Estado:** posible diferencia estática, no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:4246–4269, :4547–4597 y :4679–4710; SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. La selección construye un mapa de argumento a parámetro, pero el reanálisis para el mensaje de ownership y el diagnóstico de pass-kind comparan por posición; también presuponen en una ruta que los defaults omitidos son finales. Con argumentos nombrados reordenados u omitidos, investigar si el error describe otro parámetro o recurre al diagnóstico genérico. ConversionAndPassKindChecks.cpp:233–259 (SHA-256 ecba48102262f8040deafc81f4473caca7730a7f800af6d906e6758a6e089218) confirma que checkArgumentPassKinds acepta un mapa opcional y, si no lo recibe, usa posiciones. Esto se refiere al mensaje posterior a no encontrar candidata viable: no demuestra aceptación de una llamada incorrecta ni fallo de seguridad. Contrastar el mapeo y las restricciones anteriores. LK-560.



**Contraste NSC-411:** A:/nsc-devel/src/Frontend/ConstructionModel/ConstructorCandidateEvaluation.cpp:18–87, :137–199, :230–253 y :310–374, SHA-256 920b716d987188f2186f6df5faf93481dcef1fb5745eb99fc171f060eec206ac. mapConstructorArguments conserva ordinal de parámetro por argumento y la evaluación usa ese mapa; explainOwnershipRejection, cuando hay un único candidato accesible evaluado y coincide el número de parámetros con argumentos, arma el texto usando parameters[index] en vez del ordinal mapeado. Con argumentos nombrados permutados podría atribuir el nombre o tipo de otro parámetro. Este contraste solo afecta al mensaje en esa ruta observada, no demuestra aceptación incorrecta ni caso reproducido; contrastar restricciones del parser, packs, defaults y accesibilidad. LK-560.

### LKF-289 — Descarga global de préstamos por nombre Join

**Componente:** compilador principal, análisis de préstamos de tareas hijas. **Estado:** posible falta de precisión con riesgo de omitir un diagnóstico; no reproducido. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:4859–4865 y :5181–5233, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd; A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:920–940, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. updateScopedChildBorrowsForCall limpia todo scopedChildBorrows_ si call.methodName es Join, sin que esta función compare tipo, receptor o identidad de tarea; Spawn registra préstamos en un conjunto de locales. La validación al salir del bloque usa ese conjunto. Investigar si un Join ajeno o de otro TaskScope puede alcanzarse con préstamos pendientes y si validaciones adicionales evitan omitir el error; investigar también argumentos nombrados de hijos, pues la captura examinada usa índices posicionales. No se ha demostrado use-after-free ni compilación de un caso concreto. LK-561.


### LKF-290 — Cobertura del recorrido Send/Sync de capturas

**Componente:** compilador principal, análisis de capturas para thread executor. **Estado:** límite visible del recorrido local; impacto y otras validaciones pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:5258–5401, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. callTypeIsSend y callTypeIsSync rechazan punteros crudos y Sync también refmut; recorren campos de tipos con ordinal resoluble y omiten una clave cuyo ordinal no se resuelve, terminando true si ningún otro elemento fue rechazado. Investigar si claves de arreglos, tuplas, genéricos, tipos externos u otras formas pueden llegar ahí sin validación que examine sus elementos; no se afirma que un tipo inseguro sea aceptado ni que haya carrera observada. LK-562.


### LKF-291 — Retornos admitidos en llamada condicional nula

**Componente:** compilador principal, análisis de llamadas. **Estado:** restricción visible en ruta de método seleccionado; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/CallAnalyzer.cpp:5698–5729, SHA-256 8491ebcaf856aade625bdad42efb67614a1db05489055d892b412e2bd2145abd. finalizeSelectedCallResult emite InvalidOperand cuando una llamada null-conditional válida no pasa callResultIsNullable; esta función reconoce punteros, arreglos nativos, string, delegates y clases/interfaces resueltas. El mensaje indica que valores y void requieren Optional<T>. CarrierResolver.cpp:153–174, SHA-256 649a8e22af2717c12d216ccf8d29b8856b17ca8cdf63554294128b0c32169042, reconoce NativeSharp.Core.Optional como especialización struct, mientras callResultIsNullable no enumera explícitamente Optional; contrastar resolución y rutas completas antes de concluir si ese mensaje ofrece una alternativa aceptada. AccessAndCallExpressions.cpp:70–125, SHA-256 4ebbf14b1721db87345b6732eb35e13ccfffa870349b4ce6a5c28bc5475345ea, también exige receptor clase/interfaz y resultado nullable para lectura de campo/propiedad, salvo la ruta de escritura activa; mantiene el grupo de métodos para validarlo posteriormente. Distinguir restricción de diseño de defecto y contrastar la ruta de escritura por separado. LK-563.


### LKF-292 — Reevaluación sintáctica y operadores sobrecargados

**Componente:** compilador principal, asignación compuesta a propiedad o indexador. **Estado:** posible discrepancia; alcanzabilidad y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ConstantExpressionClassifier.cpp:58–134, SHA-256 373b6f226f95feda424bf7eeaca14c601c339e90cf376e19700d2732205c40e2; AssignmentAnalyzer.cpp:1621–1655, SHA-256 4b9e9966debaae26a78c9d0e2f22fad07b2eee7cd1f86e58f4592740201c1385; OperatorExpressionAnalyzer.cpp:319–337, SHA-256 df0cfc9d0e88df56a670101ac287cf644dfa4ab7f63ce029c714058f03b6b8e4. La prueba de reevaluación acepta unarios salvo ++/-- y binarios si sus operandos son aceptables, sin consultar aquí si el operador se resolvió a código de usuario; el validador la usa para receptor e índices de accesor y su mensaje describe evaluaciones para getter y setter. Investigar si un operador sobrecargado con efectos puede llegar a esta ruta y ejecutarse más de una vez, y si otras restricciones o lowering lo evitan. No se afirma fallo reproducido. Relacionado con LKF-170 y LK-450; LK-565.


### LKF-293 — Materialización de elementos de arreglos fijos

**Componente:** compilador principal, inicialización de agregados. **Estado:** riesgo condicionado visible en una ruta; límites previos y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ConstructionAnalyzer.cpp:304–316 y :340–359, SHA-256 9c34eb7bb50acf3545d04e4d4cf24c75bbe92394010a3ea524fa6c66a3ef75fb. aggregateElementTypeKeys y aggregateElementNames convierten el conteo uint64 de un arreglo fijo a size_t y crean un vector con una entrada por elemento. Estas funciones no muestran aquí un límite explícito ni manejo local de reserva fallida. Investigar validación de tamaño y dimensiones anterior, plataformas, propagación de excepciones y límites efectivos antes de concluir que un tamaño extremo sea alcanzable o que el compilador falle. LK-566.


### LKF-294 — Restricciones de move-spread de agregados

**Componente:** compilador principal, inicializadores de agregados. **Estado:** restricciones visibles; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ConstructionAnalyzer.cpp:461–615, SHA-256 9c34eb7bb50acf3545d04e4d4cf24c75bbe92394010a3ea524fa6c66a3ef75fb. Para agregado no trivial la ruta exige ..move(source) en inicializador local cuando allowMoveSpread lo permite; la fuente debe resolver a local move-only no pinned o parámetro move-only por valor y no tener préstamo local activo; un destructor personalizado transitivo del destino causa rechazo. Además, :847–859 exige que el move-spread transfiera al menos un campo; :899–915 admite una sola entrada spread por inicializador y :960–979 exige spread para inicializador generalizado no trivial y rechaza miembros faltantes. Son restricciones de la ruta examinada, no una fuga ni un defecto probado. Contrastar movimientos parciales, otros contextos y cleanup. LK-567.


### LKF-295 — Contratos de escape desconocidos y validación de llamadas

**Componente:** compilador principal, propagación de escape. **Estado:** diferencia de tratamiento visible; alcanzabilidad y efecto pendientes de comprobación. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/EscapeAnalyzer.cpp:34–44, :129–145, :291–326 y :329–366, SHA-256 39f9d0f6fd98abd82537a64fdf46d1e564e01aad570da61fb09d53c1cffee1bf. El orden del resumen sitúa Unknown por encima de Global y la propagación conserva el rango mayor; validateEscapeCall sólo ejecuta la comprobación de retención local para Global. Investigar qué productores generan Unknown, si otra etapa lo valida y si una dependencia Global puede quedar representada por Unknown sin diagnóstico equivalente. No concluir que el compilador acepte un escape inseguro sin trazar un caso concreto. LK-568.


### LKF-296 — Restricciones de propagación Result con postfix ?

**Componente:** compilador principal, expresiones y flujo de retorno. **Estado:** restricciones visibles en la ruta examinada; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/AccessAndCallExpressions.cpp:207–239 y :281–386, SHA-256 4ebbf14b1721db87345b6732eb35e13ccfffa870349b4ce6a5c28bc5475345ea. El operador requiere fuente y retorno con variantes compatibles Ok/Error; no puede salir de finally y requiere callable que devuelva valor distinto de constructor o destructor. Para errores distintos exige conversión explícita definida por usuario; un carrier move-only direccionable requiere transferencia. Son restricciones de esta ruta, no defectos probados; contrastar cleanup y otras formas de propagación. LK-564.


### LKF-297 — Mensaje de indexación condicional incompleto

**Componente:** compilador principal, diagnóstico de indexación condicional. **Estado:** discrepancia estática en mensaje; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/CollectionExpressions.cpp:82–130 y :132–217, SHA-256 0158d443b1d4f86861842d4b373ba9ec0867b2304e6f562c8bb8651ee737076f. La ruta intenta tanto arreglo nativo como indexador de usuario con un índice y getter; si ninguna coincide, el diagnóstico final dice que sólo se admiten arreglos nativos. Contrastar condiciones y mensajes previos antes de ajustar la redacción; esta observación no prueba acceso incorrecto ni extensión de soporte a todos los indexadores. LK-563.


### LKF-298 — Selección de métodos del constructor de colecciones

**Componente:** compilador principal, expresiones de colección. **Estado:** límite local de selección visible; efecto y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/CollectionExpressions.cpp:220–297 y :580–597, SHA-256 0158d443b1d4f86861842d4b373ba9ec0867b2304e6f562c8bb8651ee737076f. collectionBuilderMember devuelve el primer miembro encontrado con nombre y condición static solicitada. bindCollectionBuilder comprueba un parámetro en Add y conversión de elementos; en este bloque no se observan selección de sobrecarga completa ni validación de todas las firmas, accesibilidad, ownership y retornos de CreateBuilder/ToCollection. Contrastar construcción del índice, validaciones anteriores, HIR y ABI antes de concluir aceptación incorrecta o error de ejecución. LK-569.

### LKF-299 — Tipos admitidos en spread de colección nativa

**Componente:** compilador principal, expresiones de colección. **Estado:** restricción visible en ruta de arreglo; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/CollectionExpressions.cpp:300–400, SHA-256 0158d443b1d4f86861842d4b373ba9ec0867b2304e6f562c8bb8651ee737076f. Esta ruta exige que cada fuente spread sea arreglo nativo y que su tipo de elemento coincida exactamente con el destino; elementos ordinarios usan conversión implícita. No generalizar a builders ni a otras construcciones spread. Contrastar nulidad, rango, overflow y cleanup con LKF-096. LK-340 y LK-569.


### LKF-300 — Construcción directa de tipos C/C++ importados

**Componente:** compilador principal, construcción de objetos. **Estado:** rechazo visible en la versión revisada; no reproducido y sujeto a cambios de interop. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp:233–245, SHA-256 85450948f11585444e84e5944e16c0642bd29f77a4938e4a2740332594ba5404. En la ruta ObjectCreationExpression, si se reconoce un tipo importado C/C++ y cppLifetimeAnalysisForTests_ no está activo, se emite InvalidAllocationType indicando que falta implementar su lifetime. La ruta de placement :475–487 también rechaza el tipo importado y no muestra la excepción de esa bandera. La excepción de la primera ruta es una condición de análisis para pruebas, no demuestra soporte de producción. Contrastar construcción, destructores y formas alternativas de interop en la revisión vigente antes de ampliar esta conclusión. LK-570.


### LKF-301 — Inicialización de arreglo con punteros a volatile

**Componente:** compilador principal, arreglos nativos. **Estado:** restricción visible en inicializador inferido; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp:649–675, SHA-256 85450948f11585444e84e5944e16c0642bd29f77a4938e4a2740332594ba5404. Al analizar elementos de un inicializador inferido, la ruta rechaza un valor con pointee volatile si el tipo elemento es puntero, indicando que todavía no puede preservar el calificador. OperatorAndConditionalExpressions.cpp:241–273, SHA-256 642496e6d4ceb306dca1137de715f0840c77937259ed6cfc22e370e0e86db9b1, también rechaza elementos de tupla con pointee volatile en la ruta examinada. Contrastar otras formas de agregado y versión antes de generalizar. LK-571.

### LKF-302 — Lifetime trivial en stackalloc y raw new

**Componente:** compilador principal, reserva cruda. **Estado:** restricción visible en la ruta examinada; no reproducida. **Evidencia:** misma fuente :695–763, SHA-256 85450948f11585444e84e5944e16c0642bd29f77a4938e4a2740332594ba5404. validateRawAllocationElement rechaza void y tipos cuyo lifetime no es trivial para stackalloc y raw new, con diagnóstico que menciona destructores y campos owned; también rechaza tipos C/C++ importados reconocidos. Estas reglas son defensivas y no demuestran fuga. Contrastar tipos anidados, otras rutas de construcción y destrucción y validaciones posteriores. LK-571 y LKF-300.



**Contraste NSC-408:** TypeKeyPredicates.cpp:84–131, SHA-256 09b949839337133a8a4bc8a41f7a7d3afa146e478c9b1af12a990b389ffad48a, recurre sobre elementos de tuplas; para un struct nominal consulta sus miembros directos y devuelve no trivial si tiene destructor o campo no estático declarado owned. No se ve en esa función un recorrido recursivo del tipo de cada campo ordinario; también devuelve true si faltan modelos, la clave no es nominal o no resuelve ordinal. Son comportamientos locales, no prueba de que una estructura anidada no trivial llegue a la reserva ni de que falten otras validaciones. Contrastar resolución y sustitución de campos, genéricos, invariantes de disponibilidad de modelos, rutas de construcción y cleanup; diseñar reproducción futura con struct anidado con destructor o campo owned. Sin ejecución.

### LKF-303 — Restricciones explícitas de delete y destroy

**Componente:** compilador principal, expresiones de lifetime. **Estado:** restricciones visibles en frontend; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/ConstructionAndLifetimeExpressions.cpp:970–1115 y :1179–1277, SHA-256 85450948f11585444e84e5944e16c0642bd29f77a4938e4a2740332594ba5404. La ruta rechaza delete explícito de un local RAII, con excepción específica para delete[] de local owned array no pinned; rechaza delete de this dentro de destructor y de ciertos campos owned de referencia o string. destroy rechaza local RAII, this en destructor y valor owned, y limita los tipos admitidos. El tratamiento específico de arreglos y aliases requiere contrastar lowering y cleanup. Son reglas defensivas, no prueba de fuga ni protección universal. LKF-020 y LK-572.


### LKF-304 — Alcance de validación de casts explícitos

**Componente:** compilador principal, conversiones. **Estado:** posible brecha de validación local; alcanzabilidad y otras etapas pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/ConversionExpressions.cpp:161–215 y :323–443, SHA-256 9d8d75ea2f25e0efed26d0817b1c9521236ce81f11df47035089d87347809c64. Los casts nombrados aplican reglas específicas; para un cast sin nombre, tras controles de const/volatile y conversiones de usuario o delegate, validateReferenceCast retorna sin validar si ni origen ni destino es referencia. Investigar si parser, clasificador, HIR o LLVM rechazan combinaciones no contempladas antes de concluir que se acepta o ejecuta un cast inválido. Contrastar además procedencia, alineación, nulidad y espacio de direcciones derivados del destino frente a garantías de la fuente. LK-573.


### LKF-305 — Restricciones de slicing implícito de arreglos y cadenas

**Componente:** compilador principal, índices y rangos. **Estado:** restricciones visibles en la versión revisada; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp:178–201 y :235–257, SHA-256 64c51c3406c670590f104c0ed42c35bc20eb9624614a7e62acac94201f145d69. Las rutas de indexación de arreglo nativo y cadena rechazan un RangeExpression como índice, con diagnósticos que proponen vistas Span/ReadOnlySpan o StringView.Slice para evitar asignación implícita. No extrapolar a indexadores o tipos de usuario; contrastar la vida de las vistas y otros métodos de slicing. LK-574.


### LKF-306 — Protocolo de rango con despacho estático

**Componente:** compilador principal, indexación por rango de tipos de usuario. **Estado:** restricciones visibles en la ruta examinada; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp:280–305 y :325–450, SHA-256 64c51c3406c670590f104c0ed42c35bc20eb9624614a7e62acac94201f145d69. La resolución busca métodos de instancia accesibles Length y Slice, requiere Length de retorno int y descarta receptor interfaz; la validación exige retorno por valor y rechaza miembros virtuales, override o abstract. Los extremos deben convertirse implícitamente a int y los negativos constantes se rechazan. Contrastar otras rutas de slicing y comprobaciones dinámicas; no es una fuga demostrada. LK-574.


### LKF-307 — Dirección de elementos de ciertos arreglos C/C++ volatile

**Componente:** compilador principal, acceso indexado importado. **Estado:** restricción visible en una ruta concreta; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp:793–819, SHA-256 64c51c3406c670590f104c0ed42c35bc20eb9624614a7e62acac94201f145d69. Al formar la dirección del elemento de un arreglo fijo, esta comprobación emite InvalidOperand si la raíz resuelve a un único estado de namespace C/C++ volatile, con tipo de arreglo fijo sin asterisco. No generalizar el diagnóstico a todos los campos, punteros o arreglos volatile; contrastar representación y rutas del calificador en la versión vigente. LK-575.


### LKF-308 — Dirección de elementos con pointee calificado

**Componente:** compilador principal, indexación de arreglos fijos. **Estado:** restricción visible en la ruta examinada; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/IndexAndRangeExpressions.cpp:868–905, SHA-256 64c51c3406c670590f104c0ed42c35bc20eb9624614a7e62acac94201f145d69. Al formar la dirección de un elemento puntero o subarreglo fijo con pointee const o volatile, esta rama emite InvalidOperand por falta de soporte de calificadores de punteros anidados; además la addressability considera almacenamiento volatile y alineación del campo importado. La lectura o escritura y otros contextos no se describen con ese único diagnóstico. Contrastar ABI, punteros anidados y rutas posteriores. LK-575.


### LKF-309 — Seguimiento estático de elementos movidos de arreglos

**Componente:** compilador principal, movimiento de elementos. **Estado:** límite descrito y visible en la ruta examinada; no reproducido. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/MoveExpressions.cpp:308–358 y :361–419, SHA-256 e4cd7a88c22493b904749e56363524003433178337176cf59b27a4ef96e091b9. El comentario indica que el seguimiento estático de locales movidos no identifica el slot dinámico de un arreglo; el binding marca sourceIsArrayElement y clearSourceStorage según tipo, sin registrar aquí un índice movido concreto. El comentario atribuye a HIR/LLVM el vaciado del slot; comprobar su emisión y otros análisis antes de concluir seguridad o fallo. La ausencia de seguimiento preciso obliga a LeakKiller a conservar alias e índice desconocidos. LK-304 y LK-576.


### LKF-310 — Validez del resultado de move(it.Current)

**Componente:** compilador principal, movimiento de elementos de iterador. **Estado:** posible discrepancia estática; alcanzabilidad y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/MoveExpressions.cpp:543–582, :585–609 y :928–951, SHA-256 e4cd7a88c22493b904749e56363524003433178337176cf59b27a4ef96e091b9. analyzeMovedIteratorCurrent registra binding, tipo y transferencia, pero en la rama de éxito no asigna result.valid; analyzeMovedMember y analyzeExpressionKindMoveExpression retornan al manejarla. ExpressionResult.h:3–6 inicializa valid=false (SHA-256 52fbe1834902fc76b6df2188221dae94cdd826dc5cafee96901d8c862cd71904), por lo que el resultado local permanece inválido en esa rama; falta demostrar que la ruta sea alcanzable y contrastar resolución de Current y otras rutas de fields antes de concluir que un programa válido se rechaza. También verificar alias y reinicio del seguimiento al avanzar el iterador. No se ejecutó un caso. LK-577.


### LKF-311 — Restricciones de with sobre agregados

**Componente:** compilador principal, expresiones de actualización. **Estado:** restricciones visibles; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/OperatorAndConditionalExpressions.cpp:384–455, SHA-256 642496e6d4ceb306dca1137de715f0840c77937259ed6cfc22e370e0e86db9b1. La ruta exige poder enumerar campos con nombres y rechaza fuente cuyo lifetime no es trivial o que se presenta como owned, transferida o propietaria fresca. El comentario describe copia del valor seguida de actualizaciones y justifica la restricción por alias y limpieza excepcional. Contrastar otras formas de copia, accesibilidad y lowering sin presentar el rechazo defensivo como defecto. LK-578.


### LKF-312 — Inferencia limitada de tipo común en ternario

**Componente:** compilador principal, expresiones condicionales. **Estado:** alcance visible en la ruta examinada; no reproducido. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/OperatorAndConditionalExpressions.cpp:612–725, SHA-256 642496e6d4ceb306dca1137de715f0840c77937259ed6cfc22e370e0e86db9b1. Para ramas de tipo distinto, la ruta especial contempla string literal/string, null convertible y tipo numérico común; su comentario indica que no construye una jerarquía general de tipo común para referencias. Otras reglas como ramas never y decay C se procesan aparte. Contrastar ejemplos concretos y conversiones previas antes de generalizar a todas las relaciones entre tipos. LK-579.


### LKF-313 — Rechazo de políticas wrapping/unchecked antes de tipar operando

**Componente:** compilador principal, expresiones de política de overflow. **Estado:** comportamiento estático de una ruta; alcance y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/PrimaryExpressions.cpp:446–494, SHA-256 448d5dc2524a1d651b2e1294ba354c989042b32c31e3f3456c8787e59f72ab5e. La rama OverflowPolicyExpression rechaza policy unchecked o wrapping con mensaje referido a aritmética decimal antes de analizar expression->first(); acepta para análisis posterior checked y saturating. Contrastar parser, otras formas de política y tipos que alcanzan esta ruta antes de concluir que operaciones enteras válidas se rechazan. La observación no prueba resultado dinámico ni describe todas las políticas del compilador. LK-580.


### LKF-314 — Selección local del handler de interpolación

**Componente:** compilador principal, interpolación de cadenas. **Estado:** límite local de comprobación visible; impacto y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/ExpressionCore/PrimaryExpressions.cpp:841–910, SHA-256 448d5dc2524a1d651b2e1294ba354c989042b32c31e3f3456c8787e59f72ab5e. interpolationHandlerMember devuelve el primer miembro encontrado por nombre; bindInterpolationHandler exige un parámetro en AppendFormatted y analiza la validez de cada hole, sin mostrar aquí resolución completa de sobrecargas, conversiones a ese parámetro, accesibilidad ni firmas de AppendLiteral/Finish. Contrastar validaciones previas, lowering, ownership y ABI antes de concluir que un handler incompatible se acepta o falla. La ruta de interpolación estática es distinta. LK-581.


### LKF-315 — Direcciones de globales calificadas y arreglos C sin límite

**Componente:** compilador NativeSharp, análisis de estado de namespace. **Estado:** restricciones explícitas en la ruta examinada; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/IdentifierAnalyzer.cpp:1129–1162 y :1189–1205, SHA-256 029bc7ce6f74a2b4d3937365103416aef8ed21f5c3c975e154a60b179a80c750. Al formar directamente la dirección de una global, la ruta rechaza ciertos almacenamientos C/C++ volatile, atómicos o punteros con pointee calificado y otros casos de volatile anidado; el diagnóstico declara pendiente el soporte para punteros calificados. También rechaza & de un arreglo C externo de tamaño desconocido porque no representa el tipo puntero a arreglo de tamaño desconocido; el propio arreglo puede decaer a puntero al primer elemento bajo el tratamiento indicado. Las condiciones exactas están en qualifiedAddress y unknownBoundArray; no se generaliza el rechazo a toda global const, a cualquier puntero calificado ni a todas las rutas de dirección. Contrastarlo con parser, otras expresiones y lowering; LK-582. No se ejecutaron pruebas.

**Contraste adicional:** IdentifierAnalyzer.cpp:1432–1447 rechaza tomar la dirección de un operando puntero no asignable o con receptor readonly: el comentario precisa que implicaría un calificador intermedio `T* const*` que el modelo no representa. La misma explicación distingue `&p` de `T const*`, cuyo calificador corresponde al tipo base. No se afirma que toda dirección de puntero se rechace.


### LKF-316 — Asignación definitiva de parámetros out en lambdas

**Componente:** compilador NativeSharp, enlace de lambdas. **Estado:** hipótesis estática; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/LambdaDelegateBinder.cpp:1034–1052 y :1069–1094, :1189–1195, SHA-256 da6f0e29a53806d470ba988d408f13edd2902f4c2b877dea408481060fe3ee78; A:/nsc-devel/src/Frontend/Bodies/DefiniteAssignmentAnalyzer.cpp:74–91, SHA-256 d8c184ae9513785d91b3660bbb9c017cee034d39a649b4c68fcf2bbf350cf26f. El binder contrasta el modo ref/out/in con el delegate declarado, representa el parámetro como local y agrega ese local a assignedLocals_ sin distinguir aquí el modo out. El chequeo general de lectura de locales deja pasar los ya marcados; el cuerpo se analiza después. Investigar si otra validación controla lectura previa y escritura obligatoria de out en todas las salidas, cómo se representan delegates estructurales y cuál es el contrato del lenguaje antes de afirmar aceptación indebida o falla de memoria. No se ejecutaron pruebas. LK-583.


**Contraste posterior:** MethodBodyBuildCoordinator.cpp:903–910, SHA-256 cf22f420d1b98cc03f78e1141d1933f9527ad53be6815164bc955fa0929beeca, invoca verifyOutParameterAssignments al finalizar el análisis. MethodBodyCallableLifecycle.cpp:351–383, SHA-256 3fb75ec3992fc787c1baad5b9256db2d88daa8f2b77bca6be741ef0c4c913ff3, recorre métodos del modelo de miembros y comprueba si hubo alguna asignación, no todas las rutas; este recorrido visible no enumera por separado los parámetros locales de lambda. Esto acota una verificación posterior relevante, sin descartar otras ni demostrar aceptación de un programa concreto. Para el límite por caminos de métodos ordinarios véase LKF-025.


### LKF-317 — Layout de capturas usa tamaño del puntero del compilador anfitrión

**Componente:** compilador NativeSharp, cálculo semántico de entorno de lambda. **Estado:** dependencia de host visible en fuente; impacto en targets y uso del layout pendientes de contrastar. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/LambdaDelegateBinder.cpp:1319–1346, SHA-256 da6f0e29a53806d470ba988d408f13edd2902f4c2b877dea408481060fe3ee78. computeLambdaClosureLayout proporciona sizeof(void*) y alignof(void*) del proceso compilador para cada ranura a computeClosureLayout; el comentario indica una celda por captura. Verificar targets admitidos, configuración de layout y todos los consumidores del resultado antes de concluir una incompatibilidad real o un error de memoria en compilación cruzada. No implica que host y target difieran en el escenario soportado ni que el layout calculado controle necesariamente el código emitido. LK-584. Sin ejecución.


### LKF-318 — Propiedad y procedencia del resultado de loop expression

**Componente:** compilador NativeSharp, análisis de expresiones de ciclo. **Estado:** límite local visible; impacto y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/LoopExpressionAnalyzer.cpp:181–209 y :239–294, SHA-256 c92b04a6bf1f0a6c04718fce3e1d631c6c4244e03a56cf547e313d5961c16c48. loopExpressionResult toma ownership del primer valor break que no es never; combina ownershipTransfer exigiendo igualdad y transferencia en todos los break, y ownershipFreshOwner exigiendo frescura en todos. Esta función no copia explícitamente raíces de préstamo ni otros metadatos de puntero desde esos resultados. Contrastar restricciones de break, validaciones anteriores y consumidores posteriores antes de afirmar que una combinación concreta se acepta incorrectamente o pierde seguridad; no demuestra fuga ni uso posterior a liberación. LK-585. Sin ejecución.


**Contraste de break:** StatementAnalyzer.cpp:6799–6842, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907, analiza el valor con move permitido y rechaza en una condición concreta un borrowed cuya raíz local termina al salir del loop; registra el valor y el estado de salida. Esta protección complementa la unión del resultado, sin demostrar cobertura de todas las raíces o alias. LK-585.


### LKF-319 — Acceso directo a miembros de record atómico importado

**Componente:** compilador NativeSharp, análisis de acceso a miembros C. **Estado:** restricción visible en la ruta examinada; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/MemberAccessAnalyzer.cpp:748–775, SHA-256 5fe6e45f5088014a8af2fb70d3a312ff833fbe5fcdd2df1f45686f27ff32c036. Cuando el receptor de instancia está marcado cppAtomicRecordStorage, analyzeFieldMember rechaza el acceso directo a un campo y pide copiar antes el valor completo; el comentario explica el tratamiento atómico del record completo. Contrastar operaciones de carga/copia disponibles, tamaños y ABI antes de recomendar una sustitución concreta; no implica que el acceso por campos de todos los records atómicos sea válido o seguro. LK-586. Sin ejecución.

### LKF-320 — Estado de miembro activo calculado sin decisión local

**Componente:** compilador NativeSharp, acceso a agregados con miembro activo. **Estado:** observación local e hipótesis de cobertura; no reproducida. **Evidencia:** MemberAccessAnalyzer.cpp:781–800, misma huella que LKF-319. Para un agregado reconocido por aggregateRequiresActiveMember, se calcula la identidad de almacenamiento para identificadores locales o parámetros y se consulta si el campo es el activo; tanto writing como activeMemberProven se descartan mediante static_cast<void> sin diagnóstico en este bloque. El seguimiento también aparece en BodyScopeAndFlow.cpp y AssignmentAnalyzer.cpp, pero falta contrastar el contrato del lenguaje, el tipo concreto de unión, reglas C/C++ y validaciones en otras etapas. No se concluye que leer un campo alternativo sea inválido, que se omita un error exigido ni que haya fallo de memoria. LK-587. Sin ejecución.


**Contraste de productor:** StatementAnalyzer.cpp:4467–4500, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907, registra el primer campo elegible en inicialización agregada de una unión que requiere miembro activo y copia el estado conocido desde un identificador local o parámetro. La existencia de este productor no demuestra que la lectura de un miembro alternativo deba rechazarse ni cubre todos los accesos. LK-587.


### LKF-321 — Revisión conservadora de pin en funciones async

**Componente:** compilador NativeSharp, análisis de cuerpo async. **Estado:** alcance de comprobación visible en código; alcance real de locals() y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp:209–249, SHA-256 3fb75ec3992fc787c1baad5b9256db2d88daa8f2b77bca6be741ef0c4c913ff3. Si el callable tiene efecto Async y awaitExpressions_ no está vacío, esta ruta recorre locals() y diagnostica cada local que knownLifeSourcesOfLocal identifica como pinned; el bloque no compara explícitamente el intervalo de vida de ese local con la posición de cada await. Contrastar el contenido efectivo del shard, otras fases y el contrato antes de afirmar rechazo de un caso particular con scopes disjuntos o ausencia de análisis adicional. LK-588. Sin ejecución.


### LKF-322 — Nombres de respaldo concretos para categorías desconocidas

**Componente:** compilador NativeSharp, presentación textual del modelo de cuerpos. **Estado:** comportamiento de respaldo visible; alcanzabilidad y consumidores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/MethodBodyKindNames.cpp:5–20, :22–48, :95–106 y :681–692, SHA-256 8253529dd38bb1930fc592528beeada20547e43ae4a49ef0844af9a3fff64728. Si un valor de enumeración no coincide con las ramas visibles, algunos conversores retornan nombres de categorías válidas, por ejemplo method, local, finally o array-allocation; otros usan unknown. Esto podría etiquetar erróneamente valores nuevos o inválidos si llegan al conversor; falta contrastar cobertura de enumeradores, límites de ABI y consumidores. No se afirma que falte un caso válido ni que el nombre controle el comportamiento del compilador. Para LeakKiller evitar interpretar un texto de respaldo como evidencia de una reserva observada. LK-589. Sin ejecución.


### LKF-323 — Detección estática limitada de divisor cero derivado de const locales

**Componente:** compilador NativeSharp, análisis de operadores. **Estado:** límite documentado en comentario y ruta de consulta visible; no reproducido en esta revisión. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/OperatorExpressionAnalyzer.cpp:97–143, SHA-256 df0cfc9d0e88df56a670101ac287cf644dfa4ab7f63ce029c714058f03b6b8e4. divisorIsConstantZero reconoce una constante entera adjunta a la expresión o un identificador local const con constantIntegerValue conocido. El comentario indica que un const local derivado de otros const locales no se pliega en el caso descrito y atribuye la protección restante a una guarda runtime. Contrastar productores de constantIntegerValue, validaciones alternativas y lowering antes de afirmar aceptación actual o protección ejecutada para un programa particular. LK-590. Sin ejecución.


### LKF-324 — Bandera one-past-end propagada en aritmética de punteros

**Componente:** compilador NativeSharp, análisis semántico de punteros. **Estado:** hipótesis de precisión; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/OperatorExpressionAnalyzer.cpp:851–890, SHA-256 df0cfc9d0e88df56a670101ac287cf644dfa4ab7f63ce029c714058f03b6b8e4, copia onePastEndPointer del puntero fuente al resultado de suma o resta con offset, sin refinamiento explícito del desplazamiento en ese bloque. IdentifierAnalyzer.cpp:1506–1513, SHA-256 029bc7ce6f74a2b4d3937365103416aef8ed21f5c3c975e154a60b179a80c750, rechaza desreferenciar un resultado marcado one-past-end. Investigar qué expresiones pueden producir la marca, almacenamiento y análisis posterior: podría rechazar un desplazamiento que vuelva al rango, pero no se ha demostrado que un ejemplo alcance esas rutas ni se descartan validaciones complementarias. No se ejecutó. LK-591.


### LKF-325 — Solapamiento de proyecciones con raíces nombradas distintas

**Componente:** compilador NativeSharp, análisis de préstamos y lugares. **Estado:** hipótesis de precisión de alias; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/OwnershipAnalyzer.cpp:209–239, :340–354, :443–481 y :549–588, SHA-256 1e24b0cdc8cf2be7420a6ad8d37ef88c5d9f4b6fd31a2d3f99e1344838995fec. storagePlaceKey puede formar claves por local o parámetro y añadir índices para arreglos o punteros; placesOverlap trata raíces distintas como no solapadas salvo la raíz especial U. Investigar si las precondiciones y consumidores garantizan que dos raíces distintas no referencien el mismo backing o si convierten alias desconocidos en U. La observación por sí sola no demuestra aceptación de préstamos conflictivos ni identifica todas las formas de puntero; contrastar copia, referencias, argumentos y flujo antes de concluir. LK-592. Sin ejecución.

**Consumidor local identificado:** OwnershipAnalyzer.cpp:623–649 usa placesOverlap para detectar conflicto con referencias locales activas y omite la misma referencia o ancestros de reborrow según las condiciones visibles. Esto justifica contrastar las raíces producidas para cada alias, pero no demuestra por sí solo que una pareja concreta llegue con claves distintas ni que falten otras comprobaciones.


### LKF-326 — Propiedad de match con primer brazo never

**Componente:** compilador NativeSharp, expresión match. **Estado:** hipótesis de precisión visible; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/PatternAnalyzer.cpp:766–789 y :825–853, SHA-256 116f4dd029c22bb863ca3be8abc09e436a61cc801659b1576770fcba9ea793aa. applyMatchExpressionOwnership usa la propiedad de values.front() como referencia y omite valores never al comparar; si el primer brazo never tiene propiedad diferente de un brazo que produce valor, esta función retorna sin asignar propiedad al resultado. Contrastar representación real de never, alcanzabilidad, validaciones previas y consumidores antes de afirmar comportamiento incorrecto o fuga. LK-593. Sin ejecución.

### LKF-327 — Flujo de guardas match sin hechos condicionales registrados

**Componente:** compilador NativeSharp, flujo de ramas match. **Estado:** hipótesis estática; no reproducida. **Evidencia:** PatternAnalyzer.cpp:464–518, misma huella que LKF-326. Después de analizar una guarda, se captura su estado; el estado remaining se une con la rama falsa únicamente si existe conditionFlowBySyntax_ para esa guarda. Si no existe, el bloque no incorpora explícitamente el estado posterior de la guarda al camino restante. Investigar generadores de hechos, efectos de llamadas y movimientos, limpieza de bindings y rutas posteriores antes de concluir que un efecto concreto se pierde o que un programa inválido se acepta. LK-594. Sin ejecución.


### LKF-328 — Tipos de recurso aceptados por using

**Componente:** compilador NativeSharp, enlace de using. **Estado:** restricción visible en la ruta examinada; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:239–313, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. resolveUsingDispose busca NativeSharp.Core.IDisposable y su contrato y solo establece disposable para un tipo de recurso class que implementa la interfaz o interface que deriva de ella; otros kinds, incluido struct, llegan al diagnóstico de tipo en esta función. Contrastar compatibilidad declarable de structs con interfaces, otras sintaxis de using y versión antes de extrapolar a todo cleanup o afirmar un caso concreto aceptable por el lenguaje. LK-595. Sin ejecución.


### LKF-329 — Rechazo de FunctionDeclaration en análisis de sentencia

**Componente:** compilador NativeSharp, análisis de sentencias. **Estado:** diagnóstico visible para una forma de AST; alcanzabilidad por sintaxis pendiente. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:2147–2183, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. analyzeStandaloneStatement emite error de registro no implementado cuando recibe un nodo FunctionDeclaration. LambdaDelegateBinder.cpp:1123–1158, SHA-256 da6f0e29a53806d470ba988d408f13edd2902f4c2b877dea408481060fe3ee78, también contiene soporte para una función local representada como lambda, incluida autorreferencia. Contrastar parser y rutas efectivas antes de afirmar que las funciones locales en general carecen de soporte o que el diagnóstico es alcanzable desde sintaxis fuente. LK-596. Sin ejecución.


### LKF-330 — Clasificación local de vida conocida para excepciones

**Componente:** compilador NativeSharp, seguimiento de vida de excepciones. **Estado:** alcance local visible; cobertura de rutas y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:2271–2327, :2330–2390 y :2400–2452, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. knownLifeSourcesOfValue retorna información para identificadores resueltos como local o parámetro; las funciones examinadas de retorno y asignación de excepciones consultan ese clasificador. Una expresión compuesta no se clasifica por esa función. Investigar normalización del AST, propagación por otras rutas y reglas de conversión antes de concluir que un escape específico evita los controles o produce uso posterior a liberación. LK-597. Sin ejecución.


### LKF-331 — Aproximación del estado de entrada a catch

**Componente:** compilador NativeSharp, flujo de propiedad de try/catch. **Estado:** aproximación visible; cobertura de llamadas y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:2612–2675, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. completeTryBody une estados de entrada y salida normal con throwFlowStates_ registrados desde el comienzo del try; el comentario atribuye a entrada y salida una aproximación conservadora de llamadas que podrían lanzar. Investigar si cada llamada que puede fallar registra su estado intermedio y cómo opera el join cuando un recurso cambia entre entrada y salida; no se ha demostrado que se omita un camino concreto ni que exista doble liberación. LK-598. Sin ejecución.

**Contraste adicional:** StatementAnalyzer.cpp:2974–2993 y :2996–3039 registra captureOwnershipFlow() tanto para rethrow como para throw con operando, tras analizar la transferencia correspondiente. Eso confirma esas dos fuentes de throwFlowStates_; no demuestra que cada llamada que puede fallar registre su estado. LK-598.


### LKF-332 — Restricciones de declaraciones VLA locales

**Componente:** compilador NativeSharp, declaraciones de arreglos de longitud variable. **Estado:** restricciones visibles en la ruta examinada; no reproducidas. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:3211–3237 y :3240–3337, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. La ruta rechaza un arreglo flexible como variable local, exige los modificadores de dimensiones en la forma exterior indicada, dimensiones integrales no bool y elemento no void de vida trivial; también rechaza inicializador directo de la declaración VLA. Contrastar parser, variantes de sintaxis y rutas de ejecución antes de extender estas reglas a todos los arreglos o inferir ausencia de protección de límites. LK-599. Sin ejecución.


### LKF-333 — Alcance raíz asignado a referencia sin límite conocido

**Componente:** compilador NativeSharp, inicialización de referencias locales. **Estado:** comportamiento local visible; alcance y seguridad pendientes de contraste. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:3856–3917, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. analyzeReferenceVariableInitializer obtiene referenceEscapeOriginOfExpression y, si maximumValidScopeOrdinal está ausente, lo asigna al scope raíz antes de comparar el alcance local. Para identidad de almacenamiento desconocida usa por separado la clave U. Investigar qué significa la ausencia del límite, qué expresiones llegan a esta ruta y qué validaciones posteriores o contratos de retorno aplican antes de concluir que un préstamo puede escapar indebidamente. No se ejecutó. LK-600.


### LKF-334 — Fuente directa para inicializador de local borrowed

**Componente:** compilador NativeSharp, inicialización de locales prestados. **Estado:** restricción visible en una ruta; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:3985–4053 y :4055–4118, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. Salvo la excepción raiiBorrowedElementArray, analyzeBorrowedVariableInitializer exige un identificador directo; configureBorrowedVariableSource admite raíces locales RAII o borrowed y parámetros owned o borrowed, y conserva raíces posibles y alcance. Contrastar rutas de vistas, resultados de llamadas, reasignaciones y normalización antes de generalizar la restricción a todo préstamo del lenguaje. LK-601. Sin ejecución.


### LKF-335 — Análisis de sonda foreach en estado semántico compartido

**Componente:** compilador NativeSharp, preparación de foreach. **Estado:** hipótesis estática; efectos y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:5078–5135 y :5189–5217, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. configureForeachSourceOwnership analiza una copia sintáctica de la colección con diagnóstico temporal y después beginForeach analiza la declaración real; el bloque no muestra captura ni restauración explícita de estado semántico entre ambas llamadas. El propio comentario reconoce efectos de analyzeExpression sobre bindings. Investigar qué estados de flujo, movimientos, descubrimientos y bindings puede modificar esa sonda y qué capas los aíslan antes de afirmar doble evaluación en ejecución o un diagnóstico incorrecto. LK-602. Sin ejecución.


### LKF-336 — Retorno ref desde lambda

**Componente:** compilador NativeSharp, análisis de retorno de lambda. **Estado:** restricción visible en ruta examinada; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:5303–5319, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. Cuando currentLambdaOrdinal_ está presente y la sentencia return está marcada como referencia, analyzeLambdaReturn analiza el operando si existe y emite InvalidReturn indicando que los retornos ref de lambda no forman parte del contrato implementado. Contrastar sintaxis alcanzable y otras formas de callable sin extrapolar a métodos o funciones locales completas. LK-603. Sin ejecución.


### LKF-337 — Ref return de elemento detrás de campo del receptor

**Componente:** compilador NativeSharp, verificación de retornos por referencia. **Estado:** hipótesis de contrato y procedencia; no reproducida. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:5507–5525 y :5593–5605, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. Para retorno con lifetime this y origen ExternalStorage sin nombre, la función acepta una excepción específica cuando el índice proviene de un campo del receptor tipado como arreglo nativo o puntero; el comentario asocia la vida del elemento a la del receptor. Investigar propiedad real del buffer, punteros prestados, reasignación y demás contratos antes de concluir que esta asociación permite una referencia colgante o que se aplica a todo campo. LK-604. Sin ejecución.

### LKF-338 — Vida sin nombre en comprobación de ref return static

**Componente:** compilador NativeSharp, verificación de lifetime de retorno ref. **Estado:** comportamiento local visible; semántica del origen y reproducción pendientes. **Evidencia:** StatementAnalyzer.cpp:5480–5504 y :5572–5605, misma huella que LKF-337. validateReferenceReturnLifetime considera compatible con lifetime static un escape cuyo lifetimeName está vacío o es static; también omite la comparación de nombres cuando falta el nombre de origen en el caso de un lifetime explícito distinto. Otras comprobaciones rechazan tipos de origen de frame antes de llegar aquí. Investigar qué clases de origen pueden carecer de nombre y validaciones complementarias antes de afirmar que un retorno inseguro sea aceptado. LK-605. Sin ejecución.


### LKF-339 — Respaldo de origen en retorno borrowed

**Componente:** compilador NativeSharp, análisis de retorno prestado. **Estado:** alcance local visible; precondiciones y cobertura pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/StatementAnalyzer.cpp:5980–6023 y :6103–6165, SHA-256 5b2e58733776bd18cdc6a9f08b43335d4981c65c358119e16e8bf1788054f907. Después de las validaciones visibles, configureBorrowedReturnSource clasifica como BorrowedReturnForward el caso que no tiene borrowedLocalOrdinal válido ni ownershipSourceParameterOrdinal; el comentario describe un retorno prestado de llamada, pero el bloque no verifica aquí la clase sintáctica. Investigar si todos los valores alcanzables en ese caso provienen de contratos de llamada válidos y si otras validaciones conservan procedencia múltiple; no se ha demostrado aceptación indebida ni fuga. LK-606. Sin ejecución.


### LKF-340 — Representación de constantes Stage37 frente a tipos de 128 bits

**Componente:** compilador NativeSharp, evaluación de constantes Stage37. **Estado:** capacidad local visible; alcance y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/Support/IntegralConstantArithmetic.cpp:80–105 y :126–164, SHA-256 c7d53f607804820f889ddabc4cf82ba857b0637911785ec109f7e6fa26a1b4e7; A:/nsc-devel/src/Frontend/Internal/Stage37ConstantValue.h:10–25, SHA-256 085f72e95564407c13ff68d1c476b5e30c28ae2c22fd6a9cdf1efe9053670f7a. integralBitWidth reconoce i128/u128 con ancho 128, mientras Stage37ConstantValue.integer y normalizeIntegral usan uint64_t, y signedIntegral retorna int64_t para anchos al menos 64. Investigar qué operaciones y valores de 128 bits alcanzan este evaluador, restricciones previas y rutas alternativas antes de afirmar truncamiento observable o defecto en todo folding. LK-607. Sin ejecución.

**Contraste adicional:** IntegralConstantArithmetic.cpp:379–443 usa operandos uint64_t, compara el desplazamiento contra el ancho de tipo y calcula el mínimo firmado con int64_t. Para un ancho 128, desplazamientos de 64–127 o el cálculo del mínimo requieren especial atención porque exceden el ancho de esos operandos C++; investigar si son alcanzables y si una validación anterior los impide antes de afirmar comportamiento indefinido del compilador. LK-607.


### LKF-341 — Cast constante flotante a entero en evaluador Stage37

**Componente:** compilador NativeSharp, evaluación de constantes Stage37. **Estado:** límite visible y descrito en esta ruta; no reproducido. **Evidencia:** A:/nsc-devel/src/Frontend/Bodies/Support/IntegralConstantArithmetic.cpp:143–165 y :542–550, SHA-256 c7d53f607804820f889ddabc4cf82ba857b0637911785ec109f7e6fa26a1b4e7. convertStage37Constant exige valor Integer para destino integral, y el comentario de CastExpression excluye los casts constantes de flotante a entero en Stage37. Contrastar contextos que consumen este evaluador, restricciones previas, evaluadores alternativos y lowering antes de extrapolar a casts de ejecución o a toda constante. LK-608. Sin ejecución.


### LKF-342 — Presupuesto comptime comprobado después de cargar entrada

**Componente:** compilador principal, lectura de capacidad comptime. **Estado:** secuencia visible localmente; alcance y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ComptimeEngine.cpp:285–320 y :351–400, SHA-256 d9969df278fd30b4a2bb16731b24ec2f270363bca10e0fff7fc435b2b3daf462. Para capacidad file, readCapabilityInput lee el archivo completo en una cadena; resolveCapabilityCalls comprueba content.size() frente al presupuesto solo después de que la lectura termina. La ruta env también copia el valor antes de esa comparación. Por ello esta comprobación local no limita previamente la memoria temporal usada por esa lectura. No se ha contrastado si existen límites anteriores, restricciones del entorno o rutas alternativas; no se afirma fallo ejecutado ni vulnerabilidad. **Pendiente:** seguir declaración y punto de entrada, tamaños extremos, excepciones de asignación, límites externos, contabilidad de copias y liberación; definir límites y diagnósticos de LeakKiller sin suponer que el presupuesto lógico es un límite físico integral. LK-609.


**Contraste adicional NSC-409:** :490–521 comprueba el tamaño de fuente antes de invocar la expansión, pero el tamaño de la fuente expandida se contrasta con maximumMemoryBytes solo después de recibir el resultado de MacroExpansionEngine; :401 suma el tamaño de cada entrada y :741–749 construye material para la clave con dependencias. Falta inspeccionar el expansor y la contabilidad de copias, así como controles independientes; no se afirma que falte todo límite preventivo ni se ha medido uso real.


### LKF-343 — Accesibilidad de constructor implícito de struct

**Componente:** compilador principal, enlace de construcción. **Estado:** discrepancia local entre comentario y orden de ramas; contrato, alcanzabilidad y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ConstructionModel/ConstructionBinding.cpp:349–380 y :565–587, SHA-256 3bd433f94485511f269c25822448e08e0258ec6d0bbac63b9720a63e01342ae4. El comentario afirma que un struct siempre dispone de construcción sin parámetros incluso si declara constructores, pero resolveConstructionAccessibility devuelve Inaccessible si ningún constructor declarado resulta accesible antes de evaluar la ruta de default para struct sin argumentos. Si existe un sitio válido en que todos los constructores explícitos son inaccesibles y el lenguaje exige ese default, esta ruta podría rechazarlo. No se ha demostrado que el caso sea alcanzable ni contrario al contrato vigente; no se ejecutaron pruebas. **Pendiente:** contrastar especificación, constructorAccessible, condiciones de acceso y genéricos, candidato implícito, otras rutas y lowering; diseñar reproducción futura con constructores solo inaccesibles y caso de control accesible. LK-610.


### LKF-344 — Filtro de restricciones en inicializadores de constructores

**Componente:** compilador principal, encadenamiento this/base. **Estado:** diferencia estática entre rutas; alcanzabilidad, controles adicionales y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ConstructionModel/ConstructorInitializers.cpp:137–167 y :197–269, SHA-256 af16576e67ef372cef8ed2cbf92a8478075286faa8f787d654e033f7015c6482, evalúa y selecciona candidatos para inicializadores sin una llamada visible a rejectConstraintFailedCandidate; ConstructionBinding.cpp:270–327, SHA-256 3bd433f94485511f269c25822448e08e0258ec6d0bbac63b9720a63e01342ae4, sí invoca ese filtro al evaluar candidatos para construcción directa. No se ha demostrado que una especialización con restricciones fallidas alcance esta ruta, que no exista validación anterior o posterior, ni que se acepte código inválido. **Pendiente:** contrastar contexto genérico, disponibilidad de constraint model, orden de resolución, this/base, preview y diagnósticos; diseñar casos futuros para restricciones satisfechas e incumplidas. LK-611.


### LKF-345 — Un solo diagnóstico para ciclos de delegación de constructores

**Componente:** compilador principal, validación de inicializadores. **Estado:** límite local de detalle diagnóstico, lectura estática sin reproducción. **Evidencia:** A:/nsc-devel/src/Frontend/ConstructionModel/ConstructorInitializers.cpp:380–452, SHA-256 af16576e67ef372cef8ed2cbf92a8478075286faa8f787d654e033f7015c6482. validateThisConstructorCycles recorre cadenas this explícitas y resueltas, marca inválidos los miembros del ciclo y emplea un único indicador reported para emitir como máximo un diagnóstico en toda la invocación, incluso si identifica más de un ciclo o miembro inválido. No describe por sí solo una omisión de detección ni cubre recursión de funciones o bucles generales. **Pendiente:** verificar orden e invocaciones, reporte por ciclo y ubicaciones, y preparar casos futuros con ciclos independientes y cadenas que entran a ellos; conservar cada ciclo demostrado y su alcance en LeakKiller. LK-611.


### LKF-346 — Default implícito de base y parámetros predeterminados

**Componente:** compilador principal, construcción y herencia. **Estado:** diferencia de criterio estática; contrato, rutas y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ConstructionModel/ImplicitDefaultConstructors.cpp:55–100, SHA-256 6f6515e733e1e7150735fda04fcb995df86fdf1e6b2d5021b45bde8b1f933c2d, busca un constructor base con parameters.empty(); ConstructionBinding.cpp:171–199, SHA-256 3bd433f94485511f269c25822448e08e0258ec6d0bbac63b9720a63e01342ae4, en previewImplicitChainConstructor considera invocable sin argumentos un constructor cuyos parámetros todos tienen default. La diferencia podría producir predicciones y binding distintos para bases con parámetros predeterminados. Falta demostrar que el mismo caso alcanza ambas rutas y establecer semántica prevista; no se ejecutaron pruebas. **Pendiente:** contrastar especificación, sobrecargas, accesibilidad, packs, inicialización y diagnósticos; preparar casos futuros con base sin parámetros, con todos predeterminados y con mezcla requerida. LK-612.

### LKF-347 — Inicializadores de campos en clase precompilada

**Componente:** compilador principal, restauración y construcción precompilada. **Estado:** exclusión local explícita; alcance efectivo y alternativas pendientes. **Evidencia:** ImplicitDefaultConstructors.cpp:30–50, SHA-256 6f6515e733e1e7150735fda04fcb995df86fdf1e6b2d5021b45bde8b1f933c2d, omite el binding implícito para clase precompilada cuando encuentra almacenamiento de instancia; el comentario atribuye la cautela a que NSMETA no transporta las expresiones fuente de inicialización. Para clase sin campos visibles permite continuar. No se ha verificado cobertura de metadatos, snapshots, constructores publicados ni resultado externo de esta omisión. **Pendiente:** seguir productor/consumidor NSMETA, snapshots y alternativas de construcción, herencia y librerías; documentar cuando LeakKiller no disponga de inicializador y no asumir inicialización cero equivalente. LK-612.


### LKF-348 — Detalle de diagnósticos C++ adaptados

**Componente:** compilador principal, adaptador de diagnósticos C++. **Estado:** transformación local visible; formato de origen y serialización pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/CppDiagnosticAdapter.cpp:13–23 y :26–67, SHA-256 8e9ec8764d27ab2379626bb50b19fa914f3e0b0e735e4f63a3f1770ab011b891. Remark pasa a Note y Fatal a Error con prefijo textual; todas las entradas adaptadas usan CppInteropDiagnostic. Las notas anidadas se recorren y aplanan en cadenas con ubicación textual cuando existe, mientras la localización externa estructurada se asigna al diagnóstico principal. El rango principal puede quedar vacío si el resolver no lo produce. Esta ruta por sí sola no preserva la jerarquía ni una ubicación estructurada por nota; falta contrastar información disponible en origen y salidas alternativas. **Pendiente:** seguir productor, ID y rangos originales, serialización y políticas de límite; LeakKiller debe conservar la evidencia y marcar desconocido lo que no reciba, sin reconstruir jerarquías mediante conjeturas. LK-613.


### LKF-349 — Alcance local de ciclos de proyecciones asociadas

**Componente:** compilador principal, tipos asociados de traits. **Estado:** alcance de una visita estática; otras validaciones y casos alcanzables pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/AssociatedItemCollector.cpp:420–455, SHA-256 adbbf4eb4c21c5450896f25da264ab5c5ce7bc310157b5af67f72a63b222bda1. visitAssociatedProjection usa estados de visita y sigue una arista si definition->baseName() está en associatedTypes_; no se ve en esa función un recorrido de argumentos de tipo ni de otros componentes de la sintaxis. Detecta la repetición en las aristas que sí sigue. No se ha demostrado que una proyección cíclica anidada sea admitida o escape a otras comprobaciones. **Pendiente:** contrastar parser, normalización, expansión de aliases y especializaciones, rutas alternas y límites de recursión; preparar casos futuros con ciclo directo, indirecto, anidado, grafo acíclico y nombres ambiguos. LK-614.

### LKF-350 — Validación local de constantes asociadas

**Componente:** compilador principal, traits y constantes asociadas. **Estado:** criterio local visible; contrato y validaciones complementarias pendientes. **Evidencia:** misma fuente :300–349, SHA-256 adbbf4eb4c21c5450896f25da264ab5c5ce7bc310157b5af67f72a63b222bda1. validateAssociatedConstantRequirement compara typeSpelling de tipos disponibles y exige que ProjectIntegerEvaluator(nullptr) produzca un valor para el inicializador presente. Este chequeo local no demuestra evaluación de todas las formas constantes del lenguaje ni equivalencia semántica de aliases con spelling distinto. No se ha comprobado que esos casos sean válidos o alcancen esta ruta. **Pendiente:** seguir evaluator, resolución de nombres, rangos, sustitución genérica, aliases y diagnósticos; preparar casos futuros de literales, expresiones dependientes y tipos equivalentes. LK-614.


### LKF-351 — Telemetría y errores del puente de caché de genéricos

**Componente:** compilador principal, caché de plantillas y MIR. **Estado:** comportamiento local visible; semántica global y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/Cache/TemplateCacheBridge.cpp:97–110, :181–190, :194–216 y :251–269, SHA-256 ae65c782398d061088f3967bec22ca5a858564462d19202db320dde2013ae934. La restauración de plantilla devuelve false ante fallo o miss; las funciones de escritura examinadas descartan resultado y error de store. restoreTemplateRecords incrementa contadores tras restaurar un catálogo en el arena, y su comentario indica que el splice del AST es otra capa; en esa función no se ve inserción del árbol restaurado. No se ha demostrado que el compilador oculte todos los errores de caché, que el contador no tenga usos adicionales ni que produzca resultados incorrectos. **Pendiente:** seguir store/restore, validación, telemetría, otras capas y fallback cold; contrastar caché corrupta, no disponible o inválida y conservación de contratos. LeakKiller debe distinguir hit reportado, restauración efectiva y análisis realmente ejecutado. LK-615.


### LKF-352 — Huella de plantilla y coordenadas de origen

**Componente:** compilador principal, clave de caché de plantillas. **Estado:** representación local visible; cobertura del lexer y validaciones de unidad pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/Cache/TemplateCatalogCodec.cpp:25–58 y :94–102, SHA-256 40dcd38f7064d039ebcd697a25ad5ca95b6159f28de038d7963bde9647304214. computeTemplateSourceFingerprint concatena lexemas de tokens dentro del rango del nodo separados por espacios y calcula SHA-256; templateCacheKey añade la versión del esquema. No es directamente una huella de bytes completos del archivo ni incluye explícitamente el resto de la unidad en esa función. El codec también escribe IDs y offsets del rango; su comentario remite a una validación de identidad en otra capa. No se ha demostrado que cambios de trivia afecten el conjunto de tokens, ni restauración incorrecta de rangos. **Pendiente:** contrastar lexer, identidad completa de unidad, reubicación, actualizaciones de posiciones y caché; para LeakKiller conservar por separado huella del artefacto fuente y clave de caché. LK-615.



**Contraste NSC-426:** A:/nsc-devel/src/Frontend/Generics/SpecializationExecutor.cpp:399–426 y :461–494, SHA-256 0bb07eb25abbd1f380b3cbd8c81a0754f30de2f74030c76f51fbd263fdd9ef18, restaura C3 llamando deserializeAstNodeTree sin pasar FileId reubicado en esas rutas, enlaza el clon y vuelve a ejecutar processNode. La clave usada deriva de templateCacheKey y specialization key. Esta lectura no demuestra que la identidad de archivo u offsets sean incorrectos: falta seguir las claves, estabilidad de FileId, validación del almacén y contexto de unidad. LK-615.

### LKF-353 — Escritura de listas con entradas nulas en caché sintáctica

**Componente:** compilador principal, serialización de catálogo genérico. **Estado:** comportamiento local visible; invariantes y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/Cache/TemplateCatalogCodec.cpp:693–704, :835–852, :888–907 y :975–989, SHA-256 40dcd38f7064d039ebcd697a25ad5ca95b6159f28de038d7963bde9647304214. Los escritores emiten el tamaño de ciertas listas y agendan sus entradas; un paso Node o Type con puntero nulo no escribe bytes, sin que estas rutas locales descuenten la entrada o escriban marcador. Si esas listas pueden contener nulos al serializarse, el conteo podría no corresponder al contenido esperado por el lector. No se ha comprobado que el AST válido permita tales entradas, ni que una capa anterior o el lector impidan un resultado incorrecto. **Pendiente:** verificar invariantes de listas, lectores, generadores y validación de caché; preparar casos futuros con listas vacías, entradas válidas y nulas o payload truncado, sin ejecutar fase 0. LK-616.


### LKF-354 — Profundidad y validación de lectura del catálogo sintáctico

**Componente:** compilador principal, restauración de caché genérica. **Estado:** alcance local visible; límites exteriores y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/Cache/TemplateCatalogCodec.cpp:394–414, :509–549, :1023–1055, :1126–1206 y :1330–1338, SHA-256 40dcd38f7064d039ebcd697a25ad5ca95b6159f28de038d7963bde9647304214. La escritura appendSyntaxTree usa una lista explícita, pero los lectores de tipos y nodos llaman recursivamente a lectores de subárboles; no se observa un parámetro de profundidad en esas funciones. restoreCatalogEntry devuelve éxito al obtener un nodo y no comprueba allí consumo completo del payload. Esto no demuestra falta de límites en CachePayloadReader, la capa envolvente, parser o almacenamiento, ni desbordamiento de pila o aceptación efectiva de datos sobrantes. **Pendiente:** seguir tamaño máximo, procedencia y verificación de integridad de entrada, profundidad validada, ownership del arena al fallar y consumo completo; diseñar casos futuros profundos, truncados y con sufijo, sin ejecutarlos ahora. LK-616.


### LKF-355 — Inferencia genérica de extensiones con argumentos

**Componente:** compilador principal, inferencia previa de llamadas genéricas. **Estado:** diferencia local de mapeo; forma de AST, alcance y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericCallInference.cpp:187–230 y :233–272, SHA-256 16a36de10df5aee1ab401fede884c44025cf88d6397c2435be3c1bb778857461. mapGenericCallArguments inicia la asignación posicional en el primer parámetro y después identifica el candidato de extensión; el receptor se usa como evidencia explícita en inferGenericCandidateEvidence cuando call->children() está vacío. El mismo archivo :642–644 afirma que el receptor viaja en el callee. Si una llamada de extensión con argumentos llega en esa forma sin normalización previa, la inferencia podría asociar un argumento al parámetro del receptor o dejar sin mapear otro. No se ha establecido si tal caso alcanza la ruta, si otra pasada reescribe los argumentos o si existe una resolución alternativa; no se ejecutaron pruebas. **Pendiente:** trazar parser, lowering de extensión, argumentos nombrados, defaults, overloads, receptor, inferencia y materialización; preparar casos futuros con cero y varios argumentos y tipos diferentes. LK-617.

### LKF-356 — Profundidad acotada de inferencia genérica

**Componente:** compilador principal, inferencia de llamadas anidadas. **Estado:** límite explícito en ruta observada; valor y demás controles pendientes. **Evidencia:** GenericCallInference.cpp:122–143 y :495–524, SHA-256 16a36de10df5aee1ab401fede884c44025cf88d6397c2435be3c1bb778857461. inferGenericCall devuelve false cuando inferenceDepth supera DefaultMaximumInstantiationDepth y emite diagnóstico solo si reportFailure es true; la búsqueda preliminar anidada invoca inferencia con reportFailure false. No se infiere que la compilación termine silenciosamente ni que las demás rutas carezcan de diagnóstico. **Pendiente:** localizar constante y validación posterior, diferenciar sondeo de diagnóstico definitivo, y diseñar casos en profundidad límite y sobre el límite; LeakKiller debe registrar análisis incompleto ante rechazo o límite. LK-617.


### LKF-357 — Contratos de puntero en clonación de proyección asociada

**Componente:** compilador principal, clonación de genéricos. **Estado:** pérdida local de campos en construcción de modificador; alcanzabilidad y controles posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericCloner.cpp:203–230, SHA-256 578c0c6e4d6025c6dafc2d9d27844cb4df9a09647fb93376f5499f87e7962e6b, construye cada modificador adicional de source con kind, arrayCount, range y arrayRank. A:/nsc-devel/include/nativesharp/AST/TypeModifierSyntax.h:13–27, SHA-256 e5dfa2c589ae1d623f7231235b22b06e847da79908a4902a65e95125b2cfa345, define además pointerNullability, pointerHasProvenance, pointerKnownAlignment, pointerAddressSpace e importedCRestrict, que en esa inicialización reciben sus defaults. Si se alcanza la ruta con un modificador fuente cuyos contratos difieren de esos defaults, la copia local no conserva esos valores; falta demostrar que esa forma sea representable y aceptada, así como reconstrucciones o rechazos posteriores. No se ejecutaron pruebas. **Pendiente:** seguir cloneProjectionType, parser, sustitución, validación y lowering; preparar casos futuros por campo y comparación con clonación ordinaria. LeakKiller no debe inferir garantía original solo del modificador clonado. LK-618.


**Contraste adicional:** GenericCloner.cpp:368–408 y :452–490, SHA-256 578c0c6e4d6025c6dafc2d9d27844cb4df9a09647fb93376f5499f87e7962e6b, también reconstruye modificadores con esos cuatro campos en cloneTypeWorkItem y appendCloneModifiers. La ruta principal cloneType :621–656 llama a appendCloneModifiers; clonePackType :566–576 también la utiliza. En el clon superficial de hijos, :171–200 y :386–408 no se observa la misma aplicación de todos los calificadores de superficie que realiza applyClonedTypeSurface :580–602 en la ruta principal; contrastar si las formas calificadas se desvían previamente a isSpecialCloneType y si se recuperan luego. Estas observaciones son locales y no prueban que sintaxis con cada contrato alcance las ramas ni que el código generado pierda garantías.


### LKF-358 — Perfil de fallo del asignador en clones genéricos

**Componente:** compilador principal, clonación AST y asignadores. **Estado:** omisión visible en copia local; alcanzabilidad y restauración posterior pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericCloner.cpp:831–854 y :920–981, SHA-256 578c0c6e4d6025c6dafc2d9d27844cb4df9a09647fb93376f5499f87e7962e6b, copia campos de AstNode mediante cloneNodeCopyFields y cloneNodeGeneric sin llamar a setAllocatorFailureProfile en esas funciones. TemplateCatalogCodec.cpp:554–563 y :613–620, SHA-256 40dcd38f7064d039ebcd697a25ad5ca95b6159f28de038d7963bde9647304214, sí serializa y restaura ese campo. ConstructionAndLifetimeExpressions.cpp:356–364 lee el perfil para seleccionar Null, Result, Throws o Panic en el sitio de construcción. Si un nodo con perfil no vacío pasa por esta ruta y no se recupera luego, la información podría perderse antes del binding; no se ha demostrado que esa combinación sea alcanzable ni que la compilación resultante sea incorrecta. **Pendiente:** localizar productor, sintaxis, rutas especiales y pasos posteriores; contrastar política de fallo, cleanup y HIR para especializaciones, con casos futuros de los cuatro perfiles sin ejecutar fase 0. LK-619.


### LKF-359 — Inferencia preliminar y packs de lambdas genéricas

**Componente:** compilador principal, especialización de lambda genérica. **Estado:** límites locales explícitos; validación posterior y cobertura pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericLambdaSpecializer.cpp:130–169, :180–194 y :233–269, SHA-256 37a292cc5ab77096b8d3995368b1e0f0db26d006fdc5e2fdeb2676cb1fba0f41. La inferencia desde valores exige que cada argumento aporte tipo local y que coincida la cantidad; un pack sin sustitución requiere argumentos genéricos explícitos. Si no puede inferir el retorno y existe parámetro de valor concreto, la ruta usa el primero como candidato; el comentario indica que el análisis semántico posterior verifica la conversión del cuerpo. No se ha comprobado que esa comprobación cubra todas las ramas ni que el fallback conduzca a aceptación incorrecta. **Pendiente:** contrastar captura, retorno condicionado, overloads, genéricos explícitos y validación de cuerpo; LeakKiller debe diferenciar candidato preliminar de tipo semántico final. LK-620.

### LKF-360 — Resultado de clonación de lambda genérica

**Componente:** compilador principal, especialización genérica. **Estado:** posible ruta de error sin comprobación local; alcanzabilidad y reproducción pendientes. **Evidencia:** GenericLambdaSpecializer.cpp:480–487, SHA-256 37a292cc5ab77096b8d3995368b1e0f0db26d006fdc5e2fdeb2676cb1fba0f41, usa specialized inmediatamente después de cloneNode sin una comprobación local de nulidad; GenericCloner.cpp:1119–1136, SHA-256 578c0c6e4d6025c6dafc2d9d27844cb4df9a09647fb93376f5499f87e7962e6b, puede devolver nullptr al superar kMaximumCloneDepth. No se ha demostrado que la profundidad del clonador pueda excederse durante esa llamada, ni que controles previos no la bloqueen. **Pendiente:** seguir profundidad, límites, propagación de error y vida del arena; preparar casos futuros de anidamiento límite y fallo de especialización sin ejecutar fase 0. LK-620.


### LKF-361 — Eventos de progreso genérico deshabilitados en esta ruta

**Componente:** compilador principal, instrumentación de genéricos. **Estado:** exclusión condicional visible en el código; configuración y otros eventos pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericModelSupport.cpp:65–127, SHA-256 c0563aa9b1035052d53f913852877557d4960af39defef4fffbf2a8b7b8d77f7, coloca GenericWorkProgress y auxiliares de detalle bajo #if 0; GenericCallInference.cpp:506–512, SHA-256 16a36de10df5aee1ab401fede884c44025cf88d6397c2435be3c1bb778857461, también deshabilita el bloque de progreso de inferencia. Esto no demuestra ausencia de toda telemetría o de eventos por otros mecanismos. **Pendiente:** inventariar emisores activos, compilación efectiva y cobertura; LeakKiller no debe prometer trazas o progreso por template a partir de estos bloques. LK-621.



**Contraste NSC-436:** A:/nsc-devel/src/Frontend/GenericTelemetry.cpp:8–70 y :91–160, SHA-256 cc696403f2413c99af3848e7ec804b390eca45c5ff7c8bd1cdde945fcfa28397, sí mantiene contadores y tiempos agregados cuando enabled está activo; detalle por template requiere detailed. La implementación activa de Scope mide tiempo sin emitir allí eventos de progreso. renderSummary imprime us=0 si no existe entrada para una subfase, por lo que ese valor aislado no distingue ausencia de registro y tiempo registrado cero. Contrastar activación, fuentes y consumo; no inferir cobertura ni ausencia de trabajo a partir de cero. LK-621.

### LKF-362 — Inferencia temprana con evidencia parcial de expresiones

**Componente:** compilador principal, inferencia preliminar de genéricos. **Estado:** reglas locales visibles; controles semánticos posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericTypeInference.cpp:111–123, :190–203 y :224–269, SHA-256 696e8286863f1e5a5ca061f9ae361bc0b40f3ea966a7cf6aba9b46081a599ec8. La inferencia de miembro no this devuelve desconocido; condicional y binaria pueden usar el tipo conocido cuando la otra rama u operando no se infiere, mientras que con ambos presentes exigen mismo spelling. Para llamada ordinaria se consulta una tabla por nombre de callee en esa función. Son aproximaciones previas, no prueba de resolución final incorrecta ni de ejecución. **Pendiente:** seguir construcción de tabla, overloads, ámbitos, conversiones y sema posterior; preparar casos con miembro desconocido, ramas heterogéneas y funciones homónimas. LeakKiller debe distinguir evidencia parcial y tipo confirmado. LK-622.


### LKF-363 — Superficie del tipo reconstruido durante inferencia genérica

**Componente:** compilador principal, inferencia temprana de argumentos de tipo. **Estado:** copia parcial local visible; casos alcanzables y validación posterior pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/GenericTypeInference.cpp:421–459 y :462–484, SHA-256 696e8286863f1e5a5ca061f9ae361bc0b40f3ea966a7cf6aba9b46081a599ec8. cloneInferredTypeArgument crea un TypeSyntax, copia ownership bajo condición, tupla, trait, const/volatile, origen, argumentos y prefijo de modificadores; en ese cuerpo no se observa copia de referenceQualifier, lifetimeName ni atributos de tipo de función como carrier, calling convention, retorno y error. El codec TemplateCatalogCodec.cpp:172–204 serializa esos atributos como campos diferenciados. Si una inferencia alcanzable aporta un tipo para el cual son significativos, la copia local podría no conservarlos; faltan restricciones de inferencia y reconstrucción o diagnóstico posterior. No se ejecutaron pruebas. **Pendiente:** seguir admisión de tipos de función, referencias, aliases, sustituciones, tipo semántico y ABI; diseñar casos futuros y comparar inferencia con argumentos explícitos. LK-623.


### LKF-364 — Información de contratos en patrón de familia genérica

**Componente:** compilador principal, identidad de familias de especialización de métodos. **Estado:** composición de clave local visible; propósito de igualdad y otras validaciones pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/Support/GenericPatterns.cpp:69–162, SHA-256 41074c15b51b52af678214221d6ef0d8987ed19bb4c1ceeea02e3688dbbff175. genericTypePattern incluye ownership, dynamic trait, const/volatile del pointee, argumentos y forma de modificadores, pero en ese generador no se observa incorporación de nullability, provenance, alignment, address space o restrict de cada puntero, ni de todos los atributos de función y lifetime. methodSpecializationFamilyKey usa ese patrón y auxiliaryText de parámetros junto con canonicalKey. No se ha demostrado que esos atributos deban distinguir familias, que haya sobrecargas válidas diferenciadas por ellos o que otra identidad no los preserve. **Pendiente:** seguir consumidores de family key, reglas del lenguaje, colisiones y selección de sobrecargas; no derivar contratos completos a partir de esa cadena en LeakKiller. LK-624.


### LKF-365 — Métodos genéricos dentro de tipos genéricos

**Componente:** compilador principal, recolección de plantillas. **Estado:** diagnóstico explícito visible; rutas alternativas y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TemplateCollector.cpp:245–263 y :318–347, SHA-256 6cefbca6b2ed799f6e3e9670dbc65c43cb8734b2deea23a2fbb19de73b1e5339. collectGenericMethodTemplate emite ExpectedDeclaration si insideGenericType es true; el mensaje indica que esta combinación se difiere y pide especializar una dimensión a la vez. El recorrido propaga el estado al visitar hijos de un tipo genérico. No se ha verificado cada variante sintáctica ni comportamiento de versiones o metadata precompilada. **Pendiente:** contrastar parser, diagnósticos posteriores, métodos anidados y escenarios de especialización; LeakKiller debe señalar cobertura de esta configuración como limitada en esa versión. LK-625.


### LKF-366 — Alcance del recolector de dependencias de plantillas

**Componente:** compilador principal, grafo temprano de genéricos. **Estado:** cobertura local visible; productores alternativos y efecto pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TemplateDependencyCollector.cpp:191–249, :252–261 y :353–399, SHA-256 46ade9fecce228a42dc61b2fd586e39df76606af66f3723f1c92566afb457e90. collectTypeDependency recorre argumentos de tipo y registra resoluciones o dependencias no resueltas; collectCallDependency retorna si la llamada no tiene argumentos de tipo escritos. La captura opcional de candidatos externos recorre también tipos de retorno/error de función, pero es una lista distinta de las aristas de plantilla. En las llamadas a collectTypeDependency mostradas no se ve un recorrido explícito de esos dos slots ni del throwsTypeSyntax del nodo. No se ha demostrado que una dependencia relevante se pierda finalmente: otras rutas pueden materializarla o reconstruirla. **Pendiente:** seguir inferencia implícita, tipos de función, throws, cierres NSMETA, consumidores y restauración; diseñar casos futuros con tipos solo en retorno/error y llamadas genéricas inferidas. LeakKiller debe marcar parcial el grafo cuando no pueda demostrar cierre. LK-626.



**Contraste NSC-434:** A:/nsc-devel/src/Frontend/Generics/TypeDemandScheduler.cpp:477–497, SHA-256 61df571cf05cad956405daa231056af5102c2b08af9163d86be5f4fdafeb7f2d, processNestedTypeSyntax recorre expresamente functionReturnType y functionErrorType durante procesamiento y demanda de tipos. Esto muestra una vía posterior distinta; falta determinar si alimenta el grafo temprano o cierre NSMETA de LKF-366 y cubre cada caso.

### LKF-367 — Visibilidad en resolución de plantilla por módulo

**Componente:** compilador principal, resolución temprana de tipos genéricos. **Estado:** diferencia local de filtros; invariantes de índice y controles posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TemplateResolver.cpp:93–129 y :131–175, SHA-256 a2fb41a96373bff8c61e7aad4e3598a2fcf5af03b417f03eacfc3bd192c4f0c6. Para writtenName con separador de módulo, resolveTypeTemplate busca en typesByModuleQualified_ y filtra aridad, sin invocar templateVisible en ese bloque; la ruta sin separador sí aplica templateVisible por candidato. No se ha comprobado cómo se llena el índice calificado, si el usuario puede producir esa forma ni qué controles de accesibilidad o visibilidad actúan antes y después. Por ello no se afirma acceso indebido ni comportamiento reproducido. **Pendiente:** rastrear índices, parser, módulos visibles, tipos internos y privados, selección y diagnósticos semánticos; diseñar casos futuros de módulos relacionados y no relacionados. LK-627.



**Contraste NSC-435:** A:/nsc-devel/src/Frontend/Generics/TypeSpellingAndVisibility.cpp:235–265 y :313–344, SHA-256 73f386333e15e2ba7a28c0d070d3d5681ba75266bb277746094a25a2200749d0. Para tipos nominales no genéricos calificados por módulo, resolveModuleQualifiedDeclaredTypeIdentity filtra proyecto propio o declaración pública, pero en ese bloque no invoca projectVisible; en la búsqueda por otros niveles sí lo usa para módulos referenciados. Es una ruta distinta de resolveTypeTemplate y tampoco demuestra acceso indebido: contrastar índices, gramática, ProjectNameLookup y validación posterior. LK-627.

### LKF-368 — Comparación parcial de restricciones de impl genérica

**Componente:** compilador principal, coherencia de implementaciones de traits. **Estado:** composición local de forma visible; semántica y rutas alcanzables pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TraitImplementationMerger.cpp:130–149 y :187–216, SHA-256 dd1601803390c40cf31c151e565c15fa979d03d1eacb98087366e8c1b41c9115. genericConstraintShape incorpora varias banderas, igualdad y tipos, y mergeTraitImplementation compara las formas para exigir repetición exacta. En esa función no se observa inclusión de requiresRefLike ni de associatedProjection o substitutedArgument. Otros campos pueden influir en validaciones diferentes y no se ha demostrado que combinaciones con esas diferencias sean sintácticamente válidas o aceptadas sin diagnóstico. **Pendiente:** contrastar parser, representación de cláusulas, semántica de equivalencia, sema posterior y metadata; diseñar casos futuros de restricciones coincidentes y diferentes. LK-628.


### LKF-369 — Conservación de contratos al expandir alias de tipo

**Componente:** compilador principal, expansión de alias genéricos. **Estado:** copia local parcial visible; sintaxis alcanzable y controles posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TypeAliasExpander.cpp:93–172 y :175–215, SHA-256 7df56c9dd1b72d97b620320e4e6d4d848676204bcdadb554b8383d250d532ce6. cloneAliasType reconstruye modificadores con kind, arrayCount y range; TypeModifierSyntax.h:13–27, SHA-256 e5dfa2c589ae1d623f7231235b22b06e847da79908a4902a65e95125b2cfa345, también define arrayRank y contratos de puntero que reciben defaults en esa inicialización. applyExpandedAliasSyntax aplica nombre, ownership, argumentos, modificadores y origen, pero en ese cuerpo no se observa transferencia de todos los atributos de función, referencia, lifetime o const/volatile del tipo expandido. No se ha demostrado que cada combinación sea válida, alcanzable o quede sin restauración/diagnóstico posterior. **Pendiente:** contrastar parser, variantes de alias, arreglos multidimensionales y punteros calificados, tipos de función, resolución y HIR/MIR; comparar con tipo escrito directamente y preparar casos futuros sin ejecutar fase 0. LK-629.


### LKF-370 — Clave local de demanda sin identidad de archivo explícita

**Componente:** compilador principal, planificación de especializaciones. **Estado:** composición de clave local visible; efectos e invariantes del índice pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TypeDemandScheduler.cpp:140–153 y :248–274, SHA-256 61df571cf05cad956405daa231056af5102c2b08af9163d86be5f4fdafeb7f2d. La clave para demanda de Span/ReadOnlySpan concatena proyecto, namespace, nombre de vista, spelling del elemento y offset inicial; la de demanda genérica concatena proyecto, namespace y offset. Ninguna incluye explícitamente FileId o sourceOrdinal en esas construcciones. La primera se usa para deduplicación; la segunda se registra y se usa como clave estable de orden. Dos archivos pueden compartir offsets, pero falta investigar alcance del índice, intención de deduplicación por especialización, desempates y efecto sobre resultados o atribución. No se ha reproducido colisión problemática. **Pendiente:** contrastar ProjectCompilationIndexes, ciclo de vida de oleadas, archivos y namespace compartidos; LeakKiller debe incluir identidad de archivo al correlacionar sitios, no usar esa clave como ID universal. LK-630.


### LKF-371 — Extensión de arreglo fijo en identidad de tipo genérico

**Componente:** compilador principal, identidad preliminar de especializaciones. **Estado:** representación local visible; rutas alcanzables y otras identidades pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Generics/TypeSpellingAndVisibility.cpp:144–190, :193–232 y :381–396, SHA-256 73f386333e15e2ba7a28c0d070d3d5681ba75266bb277746094a25a2200749d0. typeSpelling y appendTypeModifiers representan el modificador Array fijo como [fixed] sin codificar allí arrayCount; typeSpecializationIdentity usa una identidad ya resuelta cuando existe y de otro modo parte del spelling más origen. La clave real se construye mediante typeSpecializationIdentity en TemplateResolver.cpp:357–378. No se ha demostrado que dos arreglos con longitudes distintas sean argumentos legales en esta ruta, que sus identidades resueltas omitan también la longitud o que se produzca reutilización errónea. **Pendiente:** seguir parser, identidad resuelta, normalización de constantes, claves y materialización; preparar casos futuros de longitudes distintas y tipos anidados. LeakKiller debe conservar longitud separada y no deducirla de [fixed]. LK-631.


### LKF-372 — Preparación de ownership en invocaciones HIR distintas

**Componente:** compilador principal, lowering HIR de llamadas. **Estado:** diferencia local visible; contratos admitidos y otras capas pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirCallFrameLowering.cpp:383–455, :657–747, :839–879 y :935–967, SHA-256 807507cbda22be3bda9b3127a38a9852a203c54a2871e919ca4440071105a31e. Las rutas ordinaria y de namespace aplican wrappers de préstamo y transferencia a argumentos no pack; el delegado aplica wrapper de transferencia usando delegateParameterOwned; la invocación de evento observada añade conversión y argumento sin invocar allí esos wrappers. La ruta de receptor reenviado de namespace aplica préstamo en :628–650. No se ha establecido qué contratos owned/borrowed son válidos en cada ruta, si otra etapa efectúa el manejo ni que haya fuga o doble liberación. **Pendiente:** seguir binding de eventos/delegados, restricciones semánticas, finalización de expresión, HIR/MIR y cleanup normal/excepcional; diseñar casos futuros de argumentos frescos, move y borrowed. LK-632.



**Contraste NSC-443:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp:606–649 y :652–698, SHA-256 29a4036d83d7c514ea0358d59c4a4ca760f07c3440b2ba1ccefd09049e1d2c5e, finaliza el evento entregando su nodo y envuelve el delegado con wrapFullExpressionCleanup cuando finishDelegateInvoke produce nodo, usando hasOwnedTemporary. Esto precisa la diferencia local LKF-372 pero no demuestra ausencia de cleanup de eventos en otras capas ni que los contratos señalados sean admitidos. LK-632.

### LKF-373 — Marcadores from-end en índices multidimensionales HIR

**Componente:** compilador principal, lowering HIR de índices. **Estado:** representación local acotada; rango permitido y consumidores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp:216–261, SHA-256 29a4036d83d7c514ea0358d59c4a4ca760f07c3440b2ba1ccefd09049e1d2c5e. finishCoreIndexFrame conserva número de dimensiones y por cada índice FromEndExpression pone un bit en integerValue solo si dimension < 64; también guarda booleanValue para la primera dimensión. No se ha comprobado que rangos superiores a 64 se admitan, que exista una representación complementaria o que el backend dependa exclusivamente de esta máscara. No se afirma acceso incorrecto ni fallo reproducido. **Pendiente:** seguir límites de parser/sema, representaciones alternativas y consumidores MIR/LLVM; preparar casos futuros en el borde de 64 dimensiones si el lenguaje los permite. LK-633.

**Contraste NSC-449 — límite local de stackalloc:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirCompletionTerminalFrames.cpp:265–335, SHA-256 4bc0c039a11310ed04669ce2ce73d2b985ea05a56362a9e0bb93696425c99d77. En esta ruta, si el HIR de la cantidad es IntegerConstant, se exige layout y se rechaza cuando elementos × tamaño excedería 1.048.576 bytes, mediante división para evitar desbordamiento; para tamaño cero no se aplica esa comparación. El umbral es una constante local y el diagnóstico lo presenta como reserva predeterminada. La rama observada no aplica este gate a un conteo no constante; eso no demuestra ausencia de controles en otras fases ni desbordamiento ejecutable. Tampoco demuestra que quepa en la pila disponible ni que el umbral corresponda a todos los targets o configuraciones. **Pendiente:** contrastar plegado de constantes, números negativos, sema, MIR/LLVM, guardas dinámicas, reserva por target, recursión y bucles; preparar casos futuros sin ejecutar en fase 0. LK-571, LKF-022 y LKF-108.

### LKF-374 — Tamaño de construcción con asignador y capacidad trailing

**Componente:** compilador NativeSharp, construcción HIR con asignador. **Estado:** cálculo local visible; alcance, validaciones y consecuencia pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirConstructionFrameLowering.cpp:455–489 y :575–593, SHA-256 f51a4c59c15b049843422d403407b75a90978e36413764598ba250e1b1dd3e55; AllocatorConstructionFrameState.h:14 inicializa computedSize a cero. Si hay layout se toma tamaño y alineación; en construcción trailing con capacidad constante, el valor se convierte a size_t y se suma a computedSize sin verificación explícita en ese bloque, y el resultado se entrega al finalizador. La lectura no establece si capacidades negativas, sumas excesivas o layout ausente llegan a este punto, si son rechazados antes o corregidos en el finalizador, ni si el resultado determina el tamaño de la reserva efectiva. No hay reproducción ni fuga demostrada. **Pendiente:** contrastar gramática y binding, límites de capacidad, unidades y signo, layout, anchura del target, adición y alineación, finalizador HIR y MIR/LLVM; preparar casos futuros en bordes y de fallo de asignación. LK-635.

### LKF-375 — Metadatos de vida en declaraciones desestructuradas

**Componente:** compilador NativeSharp, HIR de declaraciones. **Estado:** diferencia local observada; restricciones y manejo posterior pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirDeclarationFrameLowering.cpp:66–123 y :497–538, SHA-256 393d151a59c05b3220e641a757a6514740a9923e40f171c5074f546ca44bfc92. initializeVariableDeclarationFrame copia ownership, automaticDestruction, objectLifetimeOwnership y storageOwnership y exige región de cleanup para locales aplicables. appendDeconstructionBindingLeaf crea LocalDeclaration con nombre y ordinal y agrega un elemento TupleDeconstruct, sin copiar explícitamente esos campos ni invocar allí initializeVariableCleanup. Esto no demuestra que esos locales admitan recursos con destrucción automática, ni que falte limpieza en otro nivel, ni una fuga. **Pendiente:** revisar sema y tipos permitidos en binding de tuplas, activación y regiones en bloques, MIR/LLVM y ejecución, incluidos fallos parciales; preparar casos futuros con valores trivial y con destructor/owned solo si son legales. LK-636.

**Contraste NSC-458 de LKF-314:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirInitializationAccessFrames.cpp:219–254, SHA-256 8373e079c8f94ac3c5572e6a3eafbb18ce07a68d3e9d8a2cc34e8737cabe07f8, vuelve a analizar el texto de interpolación, cuenta holes y selecciona hasta min(holeCount, número de hijos) para construir la lista de operandos. Este fragmento no verifica aquí igualdad de conteos; el parser o fases posteriores podrían garantizarla o emitir diagnóstico. No se ha demostrado desajuste alcanzable ni pérdida de evaluación. **Pendiente:** contrastar parser original, binding, finalización de handler y mapeo de rango; preparar casos futuros de cadenas válidas y malformadas, sin ejecutarlos ahora. LK-581.

### LKF-376 — Destino de asignación compuesta de string en ruta HIR

**Componente:** compilador NativeSharp, inicialización de asignación HIR. **Estado:** restricción local visible; alcance del lenguaje y rutas alternativas pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirInitializationAssignmentFrames.cpp:396–451, SHA-256 f0383437719c4d6ef6d701d34667cd3e2da0c71797daeab28fb3036b738812d2. La rama StringCompoundAssignment exige que nameBinding del destino tenga localOrdinal; si falta, retorna el diagnóstico “string += requires an identifier-backed RAII string destination”. El clasificador y rutas especializadas pueden conducir destinos diferentes por otras vías, y no se ha establecido si una propiedad, campo o elemento string con += es válido, se rechaza antes o funciona por otra ruta. No se ejecutó reproducción ni se afirma limitación universal. **Pendiente:** contrastar sema, clasificador, setters y ruta directa/alternativa, ownership del destino, evaluación única y cleanup; preparar casos futuros de local, campo, propiedad e índice cuando sean legales. LK-637.

**Contraste NSC-462 de LKF-168:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirInitializationPrimaryFrames.cpp:172–222, SHA-256 0e248bad5da8a303160e7b794dbd4af78b7c419abe89f7c60d5e0234549fdcb8. En esta ruta HIR, si el operando tiene binding de llamada con ordinal válido y awaitProducerCanSuspend devuelve verdadero, se rechaza con NS304-T05-F03 indicando que se requiere lowering de máquina de estados. La misma inicialización recopila regiones activas hasta la profundidad retenida y guarda el handler de excepción si existe. Esto acota el rechazo observado a la vía y versión leídas; no demuestra qué pasa para productor indirecto, ni que esas regiones se ejecuten correctamente. Contrastar binding y rutas alternativas, sema y ejecución en fase posterior. LK-449 y LK-430.

### LKF-377 — Índices de arreglo con acceso condicional nulo en HIR

**Componente:** compilador NativeSharp, inicialización HIR de acceso a arreglo. **Estado:** selección local visible; sintaxis admitida y rutas posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirInitializationValueFrames.cpp:126–155, SHA-256 23227c07b939fccc6510e8cc54ecc92ea1c9e14b9a0dc1fb552daac0a2e5eaa5. Para NullConditionalArrayElement, la preparación añade a operandSyntaxes solo el primer hijo de índices cuando existe, o second en otro caso; para los elementos ordinarios añade todos los hijos. No se ha determinado si el parser/sema admite múltiples índices para esa variante, si estos se representan en otra forma o si otra ruta los maneja; no hay acceso incorrecto ni pérdida de evaluación reproducidos. **Pendiente:** contrastar sintaxis, binding y rango admitido, clasificador, finalización HIR y MIR/LLVM; preparar casos futuros con uno y varios índices y efectos secundarios solo cuando sean legales. LK-638 y LKF-373.

### LKF-378 — Cobertura local de comprobación noheap en HIR

**Componente:** compilador NativeSharp principal, fronteras de expresión HIR. **Estado:** alcance local visible; controles complementarios y ejemplos alcanzables pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirStatementFrameInitialization.cpp:22–50 y :94–245, SHA-256 acfe411710d81209071b40a3e4d43953926db1f41d8cbcbab8aec9c4e78c963f. Las comprobaciones de reserva directa observan la clase del nodo raíz de la frontera; la comprobación de invocación indirecta desenvuelve un FullExpressionCleanup de un hijo para DelegateInvoke/EventInvoke, mientras las funciones de reserva directa y construcción inspeccionan su raíz local. Se aplican además comprobaciones de binding de llamada y construcción. El archivo aislado no demuestra que una reserva anidada o envuelta eluda noheap: otras fronteras, sema o recorridos podrían rechazarla, o la forma podría ser ilegal. **Pendiente:** seguir todos los sitios de aplicación y wrappers, contratos directos/indirectos, construcciones y temporales; preparar casos futuros con anidación y controles, sin ejecutar fase 0. LK-515 y LKF-234 (componente self-host distinto).

**Contraste NSC-468 de LKF-374:** A:/nsc-devel/src/Frontend/HIR/HirAggregateLowering.cpp:595–637 y :775–838, SHA-256 0897cfd2755b2ba90d540ec0bcd6dcdb9d69c366c081e52e11c3c33c23b0eb43. finishAllocatorConstructionExpression entrega computedSize y computedAlignment a appendAllocatorPreamble, que los representa como constantes long e int y los pasa a la llamada al miembro Allocate; después construye sobre el byte* y, para perfiles distintos de None, selecciona una rama según null. Esto confirma el flujo HIR del tamaño hacia el argumento solicitado, no su representación efectiva, validación anterior ni tamaño finalmente reservado por un asignador. Siguen pendientes overflow, signo, capacidad dinámica, ausencia de layout, conversión por target y efectos de fallo de constructor; no hay prueba ejecutada. LK-635.

**Corrección NSC-469 de LKF-168:** A:/nsc-devel/src/Frontend/HIR/HirAsyncLowering.cpp:923–972, SHA-256 d332401f284ba4ad5b41dae050240b2c13cd67b330390a386835878ea29ef999. Aunque NSC-462 identificó una rama condicional que emitiría NS304-T05-F03, awaitProducerCanSuspend recorre miembros y termina siempre en false en esta versión; por tanto esa llamada no activa el rechazo. El comentario señala como control previo verifySynchronousAwaits: su alcance ya se documenta arriba y no equivale a probar todos los productores indirectos ni la ejecución. La interpretación anterior de que HIR efectivamente rechazaba el productor conocido queda corregida. Seguir las otras vías y el comportamiento runtime; LK-449.

### LKF-379 — Reescritura de throw en generador síncrono

**Componente:** compilador NativeSharp, reescritura HIR de generadores. **Estado:** diferencia local visible; semántica permitida y controles posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/HirAsyncLowering.cpp:31–109, :124–171, :290–315 y :511–546, SHA-256 d332401f284ba4ad5b41dae050240b2c13cd67b330390a386835878ea29ef999. Para iterador con tipo de error, rewriteGeneratorThrows reemplaza nodos Throw recorridos por asignación de error y estado y un return false; el return construido por rewriteGeneratorThrow no copia explícitamente cleanupRegionOrdinals ni comprueba exceptionTargetRegionOrdinal. En contraste, la vía async conserva throws con handler local y copia regiones al retorno sintetizado para throws salientes. No se ha establecido que un throw con handler o recursos activos sea legal y alcance esa reescritura, ni que otra etapa conserve el comportamiento o haga cleanup; no se reproduce pérdida de catch o fuga. **Pendiente:** contrastar sema, transformaciones anteriores, cuerpos try/finally, drop del frame, MIR/LLVM y contratos; preparar casos futuros con catch interno, finally, recurso vivo y error externo, sin ejecutar fase 0. LK-639.

### LKF-380 — Rebase local de ordinales en HIR paralelo

**Componente:** compilador NativeSharp principal, fusión HIR de funciones paralelas. **Estado:** alcance local de ajuste visible; invariantes y efecto pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/HirBuildCoordinator.cpp:262–328 y :808–849, SHA-256 e8910ed0dccf7e0e4d97cdbd41c516c5e2e99973dbb282db491a7e2ea159ff94. scanParallelOrdinals contabiliza localOrdinal y cleanupRegionOrdinal de OwnedTemporaryBorrow y regiones HIR; applyParallelOrdinalOffsets ajusta esos campos de OwnedTemporaryBorrow y ordinals HIR de región/destino de excepción en el árbol. Ese recorrido no muestra ajuste de otros campos con ordinal sintético ni del vector cleanupRegionOrdinals. Podrían estar referidos a identidades estables fuera del rango sintético o corregirse por otra ruta; no se ha demostrado colisión, referencia inválida ni diferencia de ejecución entre build secuencial y paralelo. **Pendiente:** inventariar productores/consumidores de todos los ordinales, umbrales y espacios por función, condiciones del worker y llamadas alternativas; diseñar comparación futura serial/paralela con temporales, excepciones y funciones múltiples sin ejecutar fase 0. LK-640.

### LKF-381 — Identificación del creador de lambda con capturas en namespace

**Componente:** compilador NativeSharp principal, construcción HIR de closures. **Estado:** diferencia visible en índices locales; alcanzabilidad y validaciones previas pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/HirBuildCoordinator.cpp:1710–1717, :1937–1942, :2005–2015, :2091–2115 y :2335–2356, SHA-256 e8910ed0dccf7e0e4d97cdbd41c516c5e2e99973dbb282db491a7e2ea159ff94. Las funciones de namespace usan ordinal compuesto memberCount + functionOrdinal y se registran en functionByNamespaceFunction_; buildLambdaFunction reconoce ese tipo de owner. Para una lambda con capturas sin parentLambdaOrdinal, markLambdaClosureCells busca el creador en functionByMember_ solo si el owner cabe en ese vector, que buildImpl dimensiona al número de miembros; si no lo encuentra emite “E11 lambda creator function is unavailable”. No se ha verificado si sema permite capturas en una función de namespace, si existe mapeo alternativo o si el caso es alcanzable; no hay reproducción. **Pendiente:** seguir binding y reglas de capturas, vector de índices, funciones anidadas y corutinas; preparar controles futuros con lambda sin captura y con captura de local/parámetro en namespace y método, sin ejecutar fase 0. LK-641.

**Contraste parcial de LKF-372 — getter de extensión:** A:/nsc-devel/src/Frontend/HIR/HirCallLowering.cpp:1091–1146, SHA-256 e578dbcdc4cbffc648d605a7dfc033de34c6cad640f9ba6bb602678dc357f86b. lowerExtensionPropertyGetterWithReceiver aplica wrapCallArgumentBorrow al receptor con hasOwnedTemporary local y retorna el nodo directamente; finishBoundCall, en :855–921, envuelve la llamada con wrapFullExpressionCleanup usando su indicador. Esa diferencia entre helpers no demuestra que el receptor pueda producir temporal owned en esta vía ni que falte limpieza en un llamador o etapa posterior. **Pendiente:** seguir clasificación, sema, envolturas externas, ownership de receptor y cleanup normal/excepcional; preparar casos futuros de getter de extensión con valor fresco y préstamo, solo si son válidos. LK-632. El archivo sigue en lectura parcial.

**Contraste NSC-473 de LKF-372:** A:/nsc-devel/src/Frontend/HIR/HirConversion.cpp:102–150, SHA-256 f59c30995b5dd96eef3cf314d83de3af372260d53772b3375c905ceb5b9a2a11. wrapCallArgumentBorrow solo crea OwnedTemporaryBorrow si el binding coincide con miembro y parámetro y su clase es FreshOwnedTemporaryToBorrowedParameter; entonces asigna ordinal local y de cleanup y marca hasOwnedTemporary. Esto precisa el indicador descartado en el helper del getter de extensión, pero no demuestra que ese binding se produzca para tal receptor ni que falte un wrapper externo o cleanup posterior. Seguir sema y llamadores, y preparar caso futuro sin ejecutar fase 0. LK-632.

**Contraste NSC-474 de LKF-372:** A:/nsc-devel/src/Frontend/HIR/HirExceptionAndCleanup.cpp:44–53 y :310–402, SHA-256 8a29cfa037715d7dc7c019cc3db8e12819f9908d44c67cd77d5786b92a53fa5f. wrapFullExpressionCleanup envuelve únicamente cuando recibe hasOwnedTemporary verdadero. El pase ownDroppableTemporaries recorre nodos, envuelve algunos resultados descartados y receptores y llama extendLentArgumentCleanups; esa última ruta parte de un FullExpressionCleanup ya existente. El archivo examinado no demuestra por sí solo que el getter de extensión carezca de otra envoltura externa ni que pueda recibir el binding relevante. **Pendiente:** trazar sus llamadores y sema hasta MIR/LLVM y comparar rutas con y sin temporal; no afirmar fuga. LK-632.

**Contraste parcial de LKF-314 — holes faltantes:** A:/nsc-devel/src/Frontend/HIR/HirExpressionMachine.cpp:82–143, SHA-256 0b1dcebc7358688df55f3ba5f2c23eaf1d9229a7cafd14ad52e2aecdc7dda502. buildInterpolationHandlerExpression comprueba que exista un operando por cada hole y devuelve error si falta; esto complementa la selección mediante mínimo descrita en NSC-458. En este helper no se ve una comprobación final de operandos sobrantes, pero la construcción de la lista y el parser pueden impedirlos o tratarlos antes. No hay desajuste reproducido. Seguir parser y binding y no inferir pérdida de evaluación. LK-581. Archivo en revisión parcial.

**Contraste parcial adicional de LKF-372 — ruta directa:** A:/nsc-devel/src/Frontend/HIR/HirExpressionMachine.cpp:771–831, SHA-256 0b1dcebc7358688df55f3ba5f2c23eaf1d9229a7cafd14ad52e2aecdc7dda502. La ruta directa de nombre/miembro que encuentra NamespaceFunctionCallBinding retorna lowerExtensionPropertyGetterWithReceiver directamente. Esto identifica un llamador del helper observado, pero no prueba ausencia de envolturas en el contexto exterior ni que el binding de préstamo fresco sea alcanzable; además puede existir ruta iterativa distinta. Seguir sema, raíz de expresión y pase de temporales hasta MIR antes de concluir. LK-632. Archivo aún en lectura parcial.

**Contraste parcial de LKF-377 y LKF-373:** A:/nsc-devel/src/Frontend/HIR/HirExpressionMachine.cpp:1302–1378, SHA-256 0b1dcebc7358688df55f3ba5f2c23eaf1d9229a7cafd14ad52e2aecdc7dda502. La ruta directa de arreglo ordinario recorre los índices y usa bits from-end únicamente para dimensiones inferiores a 64; la ruta directa null-conditional toma el primer índice y construye NullConditionalArrayLoad con receptor e índice. Esto coincide con la selección local observada en las rutas iterativas, pero no establece que el lenguaje admita índices múltiples condicionales ni que se usen máscaras por encima del límite. Pendiente parser/sema y backend; LK-633 y LK-638. Archivo en lectura parcial.


### LKF-382 — Contador de profundidad de expansión de macros

**Componente:** compilador NativeSharp. **Estado:** comportamiento local observado; contrato, configuración y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MacroExpansionEngine.cpp:208–260 y :266–283, SHA-256 67cacb5bfa3bc29a49ef354d80fb78736d2ad3a33963876a96dc4347dbc62485. expandMacroInvocations inicializa scope a cero, lo incrementa en cada invocación procesada y lo compara con maximumDepth; maximumExpansions limita separadamente la lista de expansiones. No se observa decremento de scope: esta comprobación local cuenta expansiones procesadas y no mide por sí sola anidamiento. Falta verificar valores efectivos, contrato y llamadores; no se ha reproducido un rechazo de fuente válida ni se ha establecido si es intencional. **Pendiente:** contrastar documentación y configuración; diseñar casos futuros de invocaciones independientes y anidadas alrededor de ambos límites y recursión, sin ejecutar fase 0. LK-642.

### LKF-383 — Búsqueda textual de directivas de macros

**Componente:** compilador NativeSharp. **Estado:** búsqueda local visible; filtrado y sintaxis admitida pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MacroExpansionEngine.cpp:145–150, :201–218 y :271–283, SHA-256 67cacb5bfa3bc29a49ef354d80fb78736d2ad3a33963876a96dc4347dbc62485. La búsqueda de #macro y #expand usa find sobre result.source; esas búsquedas no discriminan léxicamente comentarios o literales. El archivo aislado no establece qué texto entrega el llamador ni los filtros previos; no se ha reproducido diagnóstico espurio ni modificación de literal. **Pendiente:** rastrear entrada, preprocesado y gramática; preparar casos futuros con directivas en comentarios, cadenas y código, y delimitadores en argumentos. Conservar procedencia de expansión; sin ejecutar fase 0. LK-643.


### LKF-384 — Comprobación local de valores predeterminados de parámetros

**Componente:** compilador NativeSharp, modelo de miembros. **Estado:** alcance local visible; validaciones posteriores y comportamiento efectivo pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MemberModel/BuildAndDeclaration.cpp:1120–1146, SHA-256 6b62654bddda860f5eb99611af4463b9be6637040433d2770a5c81fbd3917cbe; A:/nsc-devel/src/Frontend/MemberModel/CallableMembers.cpp:117–175, SHA-256 8b24b506faa4c4c2d1845db372343537a078b89e8a3ae8442758065f41123c1c. validateDefaultParameter reconoce literales numéricos con signo y entrega el operando al helper; este comprueba categorías de tipo para enteros/caracteres y trata null como compatible con punteros, arreglos y ciertas referencias sin consultar aquí el valor numérico ni pointerNullable. Esto no demuestra aceptación final de un valor fuera de rango o null para un contrato no nulo: parser, otras validaciones, inserción del argumento o lowering pueden verificarlos o rechazar la sintaxis. **Pendiente:** seguir literal, signo, rango, nullabilidad, sustitución en llamada y conversión hasta diagnóstico y generación; diseñar controles futuros con límites enteros, desbordamiento, punteros nullable/non-null y valores predeterminados explícitos/omitidos. No ejecutar ni afirmar defecto reproducido en fase 0. LK-644 y LK-434.


### LKF-385 — Selección local de destino para implementación externa de trait

**Componente:** compilador NativeSharp, modelo de miembros. **Estado:** selección visible; invariantes y validación previa pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MemberModel/BuildSupport.cpp:87–111, SHA-256 5fedbfc9c6518d46efcdae6489141dd9ae64294a6f8d592c0f4467c996b658a8. collectExternalTraitImplMembers obtiene part.syntax->text(), recorre tipos y selecciona el primer identity.name() coincidente; en este recorrido local no compara namespace, módulo, visibilidad o identidad calificada. No se ha verificado cómo ProjectNameLookup construye estas partes, qué permite el parser o si la unicidad y accesibilidad se aseguran previamente; tampoco se ha reproducido selección equivocada. **Pendiente:** seguir el productor de externalTraitImplParts, resolución y restricciones; preparar casos futuros con nombres repetidos en namespaces y módulos visibles/no visibles, variando orden y calificación cuando sean legales. Diferenciar identidad confirmada de incertidumbre en LeakKiller; no ejecutar fase 0. LK-645 y LK-627.


### LKF-386 — Identificación local de ReadOnlySpan en parámetros params

**Componente:** compilador NativeSharp, validación de parámetros. **Estado:** criterios locales visibles; resolución e invariantes pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MemberModel/CallableMembers.cpp:42–89 y :178–205, SHA-256 8b24b506faa4c4c2d1845db372343537a078b89e8a3ae8442758065f41123c1c. validateParameterPack acepta sintaxis cuyo baseName sea ReadOnlySpan o termine en .ReadOnlySpan con un argumento, y en la ruta de especialización busca ReadOnlySpan seguido de marca de aridad dentro de templateKey; obtiene la clave del elemento. Estas condiciones locales no prueban identidad canónica del tipo de biblioteca ni que cualquier homónimo sea alcanzable: el parser, la resolución y otras validaciones podrían restringirlo. No hay aceptación incorrecta ni representación incompatible reproducida. **Pendiente:** rastrear resolución, tipo definitivo, pack y lowering de llamadas; diseñar casos futuros con tipo canónico, alias y homónimos calificados en otros namespaces si el lenguaje los admite. Preservar identidad y layout como desconocidos mientras falte contrato; no ejecutar fase 0. LK-646.


### LKF-387 — Restricciones de ownership en campos estáticos y accesores

**Componente:** compilador NativeSharp, modelo de miembros. **Estado:** restricciones visibles en código, sin ejecución. **Evidencia:** A:/nsc-devel/src/Frontend/MemberModel/MemberInitialization.cpp:346–375 y :510–544, SHA-256 e185410f0af3996e32a9475f8a1b7e3a4b6125e35358496f2f78db4d1c9333b1. validateStaticOwnedMember diagnostica un campo static declarado owned; el comentario indica que el backing sintetizado de propiedad automática está incluido. initializeAccessorMetadata conserva ownsAccessorStorage para un getter de propiedad o indexador owned, pero cambia declaredOwnership a None; el comentario distingue esta referencia sin calificador de un retorno Borrowed con relación de vida demostrada. No se ha ejecutado ni contrastado toda la generación de almacenamiento estático, cleanup de proceso o procedencia del getter; no inferir que toda referencia sin calificador sea segura o que exista fuga. **Pendiente:** seguir almacenamiento, generación de backing, llamadas y flujo de vida del getter y setter, así como contratos y diagnósticos; diseñar casos futuros de propiedad/índice owned, referencias retenidas y campos estáticos, con terminación normal y temprana, sin ejecutar fase 0. LK-647 y LK-632.


### LKF-388 — Propiedad transitiva de valores que contienen tuplas

**Componente:** compilador NativeSharp, consultas de propiedad y drop. **Estado:** diferencia entre recorridos locales visible; tipos legales y efecto posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MemberModel/OwnershipQueries.cpp:41–103 y :105–149, SHA-256 dac3bfc8e786bd07be0eabcb32a1d7582e05352b1e8220f6c25bfb41fe6fcc82. typeTransitivelyOwnsStorage y la construcción de su caché desenvuelven arreglos fijos y siguen dependencias nominales de structs, sin descomponer tuplas en esas rutas; typeTransitivelyHasCustomDestructor sí descompone tuplas. typeRequiresValueDrop combina ambas consultas con casos especiales de iterador y future. Por ello, una tupla o campo tupla que contenga struct con campo owned y sin destructor explícito requiere contrastar cómo se representan y procesan sus componentes en otras capas; no se ha establecido que esos tipos lleguen a esta consulta, que sean legales ni que no exista cleanup independiente. No hay fuga reproducida. **Pendiente:** rastrear construcción de caché y todos sus consumidores, tipos de tupla y arreglos, validaciones de union y generación de drop en HIR/MIR/runtime; diseñar casos futuros con struct owned dentro de tupla, tupla dentro de struct y arreglos fijos, con controles de destructor y sin él. Sin ejecutar fase 0. LK-648 y LKF-387.


**Contraste NSC-488 de LKF-386:** A:/nsc-devel/src/Frontend/MIR/Internal/MirNodeMachine.cpp:1997–2051, SHA-256 0714765b7f5a540065f71b72033f4e4a51e5d3ac3fb4228c55b36db482374fb2. El lowering ParameterPack resuelve la identidad del tipo y su layout; exige al menos dos campos iniciales de tipos T* e int, además de layouts de elemento y vista, y registra sus offsets. Esto añade una validación estructural posterior al reconocimiento textual observado; no acredita por sí solo identidad canónica ni demuestra que un homónimo legal llegue a MIR. Seguir sema, resolución, representación completa y backend. LK-646.


**Contraste NSC-489 de LKF-388:** A:/nsc-devel/src/Frontend/MIR/MirBuildCoordinator.cpp:310–404 y :413–434, SHA-256 d5215b168bcf780e34ddc5b2cc7216fd6decd3c82422683844d714c97b2d2e3d. En el plan de destrucción de una clase no importada, para campos que no se clasifican como liberación de heap directa se llama appendValueFieldDropStep; esta retorna sin añadir paso si memberModel.typeRequiresValueDrop(declaredTypeKey) es falso. Esto identifica un consumidor de la consulta examinada en NSC-486, no demuestra que un campo tupla con owner sea legal, alcance esa representación, carezca de otro cleanup o produzca fuga. Contrastar campo específico, caché, tipos, cuerpo de destructor y backend con caso futuro; sin ejecución. LK-648.


### LKF-389 — Hijos nulos en recorrido de índices de caché MIR

**Componente:** compilador NativeSharp, serialización y restauración de MIR. **Estado:** diferencia de tratamiento visible; alcanzabilidad y reproducción pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MIR/MirCache.cpp:383–410, :680–700, :769–804, :936–993, :1363–1378 y :1572–1584, SHA-256 d95c2d87ecd205390739e1b2ee9160790c06c905e4970f708fb812530c99c0a3; MirNodeMachine.cpp:2364–2370, SHA-256 0714765b7f5a540065f71b72033f4e4a51e5d3ac3fb4228c55b36db482374fb2. La serialización y restauración de árboles prevén hijos nulos y el lowering puede preservarlos. nodePreorderIndex y nodeByPreorderIndex apilan todos los hijos y, al visitar un nodo, acceden a children sin comprobar nulidad; se usan al serializar/reconectar acciones de cleanup y salidas no locales. No se ha establecido que un árbol MIR válido con hijo nulo y una búsqueda que lo atraviese alcance estas rutas con caché habilitada, ni hay fallo reproducido. **Pendiente:** contrastar invariantes de HIR/MIR y verificador, orden de nodos, hit/miss, sentinel y manejo de payload; diseñar casos futuros con hijos nulos antes/después de acción o salida, con y sin caché, sin ejecutar fase 0. LK-649.


### LKF-390 — Validación lineal de préstamos durante suspensión

**Componente:** compilador NativeSharp, construcción de frame MIR de poll. **Estado:** algoritmo local visible; restricciones previas y casos alcanzables pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/MIR/MirFunctionLowerer.cpp:350–384, :421–462, :487–504 y :1033–1067, SHA-256 ec3ac6064152b809a5b46d613e6306ea15cfe6a7a22ad68db4059bbf359d92b2. collectPollLivenessNodes ordena nodos mediante recorrido del árbol y registra última posición de LocalValue/LocalAddress y suspensiones; liveAcrossBoundary compara posiciones y validateNonLocalBorrowSources usa el resultado para rechazar algunos préstamos de fuentes no locales. collectBorrowSourcesNodes reconoce un BorrowValue que es el primer hijo de declaración local. buildPollFrame conserva todos los locales mediante appendCoroutineFrameLocalNodes; por tanto la observación sobre el análisis lineal se refiere a la validación del préstamo, no a que el frame omita todos los locales. No se ha verificado si sema, lowering o reglas adicionales impiden casos donde una referencia se vuelve a utilizar en una iteración posterior, ni si otros nodos expresan el uso o préstamo. No hay infracción de lifetime reproducida. **Pendiente:** rastrear CFG real, productores, wrappers, sema, verificador y reanudación; diseñar casos futuros con préstamo de parámetro/campo, uso antes de suspensión dentro de ciclo y uso en vuelta posterior, controles de rechazo y fuentes locales. Sin ejecutar fase 0. LK-650 y LKF-168.

**Contraste NSC-491 de LKF-389:** A:/nsc-devel/src/Frontend/MIR/MirFunctionLowerer.cpp:122–145 y :149–184, SHA-256 ec3ac6064152b809a5b46d613e6306ea15cfe6a7a22ad68db4059bbf359d92b2. Los recorridos que recopilan contratos de cleanup y validan sus referencias omiten explícitamente los nodos nulos. Esto distingue su tratamiento del recorrido preorder de caché, pero no prueba que un árbol válido con hijo nulo alcance serialización/restauración ni que el verificador final lo admita. LK-649.


**Contraste NSC-493 de LKF-385:** A:/nsc-devel/src/Frontend/NameLookup/TypeAggregation.cpp:135–185 y :404–420, SHA-256 8388c5c6e55b709f323a682cfb8ff8e35b580b7d2b0b55683f70208186393c18. Cuando un grupo no tiene declaración primaria, finalizeOrphanTraitImplementationGroup verifica que el proyecto posea un trait y conserva las partes en externalTraitImplParts_; moduleOwnsTraitImplementation compara baseName del trait con text de una declaración interface del mismo projectOrdinal. La agrupación de tipos usa identidad completa de pendingTypes, pero el vector de partes usado después por el modelo de miembros no se acompaña aquí de un ordinal de destino explícito. Esto identifica el productor y comprobación previa, sin establecer qué nombres calificados o homónimos admite el parser ni demostrar resolución errónea. Seguir construcción de pendingTypes, identidad, sema y llamada al colector; LK-645.


**Contraste NSC-494 de LKF-380:** A:/nsc-devel/src/Frontend/ProjectCallableResultMerger.cpp:31–148 y :212–257, SHA-256 853ea87f9500688653be1d8ccaf1ad4633ef7906b8570ea267ebe2e238dfc044. Esta fusión de resultados semánticos de funciones usa bases separadas para ámbitos, locales, cleanup, lambdas y préstamos; remapCommon ajusta cleanupRegionOrdinals y campos opcionales cuando existen, y remapNested trata capturas y estructuras particulares. Es un pase distinto del rebase HIR paralelo descrito en LKF-380: no demuestra que ordinales sintéticos HIR compartan ese espacio o queden corregidos por esta fusión. Seguir productores, identidades y umbrales de ambos pases; LK-640.


**Contraste parcial de LKF-389 — configuración de caché MIR:** A:/nsc-devel/src/Frontend/ProjectCompilation.cpp:1827–1859, SHA-256 ed7116ffec2a50144550997848689fbd86f5661957820e3740ada51d50671d78. compileProjectOopMir crea el modelo y configura panicProfile/panicHandler, pero esta ruta no configura una raíz de caché MIR; el comentario indica que la caché permanece desconectada. La API de ProjectOopMirModel permite configurarla por separado (MirCache.cpp:1488–1491). Esto acota la investigación a rutas que realmente habiliten dicha caché; falta buscar otros llamadores y verificar configuración efectiva. No se afirma fallo en compilación habitual ni reproducción. El archivo está en lectura parcial. LK-649.


**Contraste NSC-495 de LKF-383:** A:/nsc-devel/src/Frontend/ProjectCompilation.cpp:601–613 y :1986–2060, SHA-256 ed7116ffec2a50144550997848689fbd86f5661957820e3740ada51d50671d78. El coordinador decide si invocar comptime mediante búsqueda textual de marcadores; cuando se requiere, registra macroExpansions y agrega al SourceManager el texto resultante de comptime bajo la ruta original. Los diagnósticos authored usan offsets del resultado suministrado. Esto muestra que posteriores rangos pueden estar referidos al buffer transformado; no demuestra un mapa completo a la fuente original ni la ausencia de uno en otros componentes. Para LKF-383 sigue pendiente qué texto llega al expansor, tratamiento de literales y comentarios y gramática. LK-643.


**Contraste parcial de LKF-290 — restricciones genéricas:** A:/nsc-devel/src/Frontend/ProjectGenericConstraintModel.cpp:854–929, SHA-256 5213149d253f410943eb613dc362e6c46ef4f4a6fbe8c53d724f54717b1e1492. satisfiesTypeConstraint activa la vía especial Send/Sync por identity.name(); satisfiesThreadSafetyConstraint examina miembros declarados directos y asigna send/sync true a campos clasificados como Value sin recorrer aquí el tipo de esos campos; maneja de forma distinta ref, refmut y punteros crudos. Esta vía de restricciones genéricas es distinta del recorrido de capturas registrado en LKF-290. No se ha establecido si las identidades de marcador están reservadas, si otros pases comprueban tipos anidados, campos heredados o formas excluidas, ni si un caso inseguro es aceptado. **Pendiente:** contrastar language::evaluateThreadSafety, resolución de marcadores, sema y dispatch; diseñar casos futuros con agregados anidados y homónimos legales. Archivo en lectura parcial; sin ejecución. LK-562.


**Corrección NSC-502 de LKF-163:** A:/nsc-devel/src/Frontend/ProjectGenericModel.cpp:661–740, SHA-256 f4dc44fe96d65ef5af418d6f24fb1be62466feb99cb2ba23c240b17f792c412d. La implementación actual usa el nombre terminal para el segmento legible del nombre generado, pero para un tipo de usuario ordinario incorpora elementTypeKey completo al material de identidad y hash; por tanto el comentario anterior de interfaz no demuestra que el hash se limite al spelling terminal. Para un argumento que specializationForTypeKey reconoce como especialización, la búsqueda extrae el nombre terminal y consulta el índice de nombres, y usa specializationKey del registro encontrado; closedTypeKeyForTemplate selecciona además la primera plantilla por nombre. No se ha probado colisión, ambigüedad alcanzable ni error; contrastar unicidad, validación de consumidor y nombres generados entre módulos. LK-443 y LK-624.


### LKF-391 — Selección temprana de implementación de interfaz

**Componente:** compilador NativeSharp, resolución de implementaciones de interfaz y trait. **Estado:** comportamiento local visible; reglas de ocultamiento y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectInterfaceModel.cpp:1045–1105 y :1356–1417, SHA-256 b24ce09abc786a7cbdbd05f64e061c752ee0cb02e6f20d50c5face85355605fb. findImplementation y findStaticImplementation recorren miembros con firma coincidente y, al encontrar un método incompatible en propiedades como accesibilidad, static, retorno, referencia o lifetime, devuelven nullopt inmediatamente; no siguen con candidatos posteriores ni con la clase base en esa ruta. Si no se encuentra implementación, la primera ruta puede seleccionar un default de interfaz o emitir diagnóstico. No se ha establecido si el modelo permite varios candidatos de esa firma, si el método derivado incompatible debe ocultar la base según el lenguaje o si otras validaciones rechazan el caso; no hay selección errónea reproducida. **Pendiente:** contrastar unicidad, ocultamiento, herencia, controlled specializations y default, además de comprobaciones throws/noheap y tabla MIR; preparar casos futuros con candidato derivado incompatible y base compatible cuando sean legales. Sin ejecutar fase 0. LK-651.


**Contraste NSC-504 de LKF-388:** A:/nsc-devel/src/Frontend/ProjectMemberModel.cpp:263–277 y :518–536, SHA-256 9dbc48afdead43d0e016098198d1f41e36d44845746f4849cc8dcda917918b51. buildTransitiveOwnershipCache se invoca tras la recolección de miembros declarados, traits externos e importados, y antes de validateCopyTypes y validateRawUnions. Esto precisa el momento de construcción de la caché para la vía ordinaria del modelo; no establece que todas las formas de tipo, snapshots u otras rutas estén representadas ni demuestra pérdida de cleanup. Mantener pendiente la investigación de tuplas y consumidores. LK-648.


**Contraste NSC-506 de LKF-165 y LKF-383:** A:/nsc-devel/src/Frontend/ProjectMetaprogrammingModel.cpp:62–115, :188–221, :273–362 y :803–821, SHA-256 df6025b4735d39c204345af7f5b26b202c082bea398c2f5d4501a3d18f5ed7a1. El catálogo recoge datos estáticos de tipos y miembros; findAttributeTarget elige la declaración de menor offset inicial posterior a la directiva dentro de proyecto y ordinal de fuente, prefiriendo tipo sobre miembro en empate. Conserva en runtimeAttributes los usos con retención NativeRuntimeSection, pero emitsRuntimeMetadata devuelve false en este modelo. Esto no prueba que otro productor no emita atributos, ni que offsets de directiva y declaración difieran después de comptime. **Pendiente:** seguir transformaciones y mapa de offsets, declaraciones parciales/generadas y productores de secciones/PDB; conservar origen y estado desconocido sin atribuir una asociación errónea no reproducida. LK-446 y LK-643.


### LKF-392 — Contratos de lifetime entre declaraciones de una función de namespace

**Componente:** compilador NativeSharp, fusión de declaraciones de namespace. **Estado:** comparación local observada; reglas y otras validaciones pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectNamespaceFunctionModel.cpp:249–269, :506–539, :663–704, :765–864 y :866–969; SHA-256 18ec1af2696f70491194401fcce5065c354e00c6111192e6471f211c9a92d0bf. Cada candidata registra y valida lifetimes y restricciones; su firma incluye tipo, ownership y modo de paso. compatibleDeclarations compara varias propiedades pero no compara explícitamente lifetimes ni restricciones de ambas candidatas. Al fusionar conserva el símbolo existente y agrega la parte; al recibir un cuerpo actualiza definición e inlining, sin copiar ahí el contrato de lifetime de la candidata. No se ha demostrado que declaraciones incompatibles sean legales o se fusionen, que otro pase no las rechace, ni que ocurra una referencia inválida. **Pendiente:** contrastar reglas de declaración y definición, equivalencia por renombrado, otras validaciones, snapshots y consumo del contrato; diseñar casos futuros equivalentes y diferentes en ambos órdenes sin ejecutarlos en fase 0. LK-652.


### LKF-393 — Referencias de estado de namespace al reconstruir el modelo

**Componente:** compilador NativeSharp, estado interno del modelo de funciones de namespace. **Estado:** omisión local en limpieza observada; reutilización efectiva e impacto pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectNamespaceFunctionModel.cpp:442–460, :1547–1568, :1850–1858 y :2046–2057; SHA-256 18ec1af2696f70491194401fcce5065c354e00c6111192e6471f211c9a92d0bf. build invoca resetBuildState, que vacía índices de funciones, estados y referencias de funciones/extensión, pero no vacía explícitamente stateReferenceBindings_ ni stateReferenceBindingBySyntax_. bindStateReference agrega entradas y usa emplace por nodo; los accesores consultan esas colecciones. Si la misma instancia se reconstruye sin otra limpieza podrían permanecer enlaces y ordinales anteriores; con la misma dirección de nodo, emplace conserva la entrada previa. No se ha probado reutilización de instancia, vigencia del AST, acceso posterior ni efecto observable; no se afirma fuga del programa compilado. **Pendiente:** seguir propietarios, ciclo de vida, rutas incrementales y lectores; diseñar casos futuros de reconstrucción con estado eliminado, cambiado y nueva sintaxis frente a instancia nueva. Sin ejecutar fase 0. LK-653.


### LKF-394 — Aritmética de layout de Future sin comprobación local de overflow

**Componente:** compilador NativeSharp, layout de campos Future. **Estado:** operación local observada; límites y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectObjectLayoutModel.cpp:64–67 y :580–644, SHA-256 4cce71cf1f054212e58a9c9f0ff94531ec911673c8bed83f9073d3dc68990cb4. alignFutureOffset calcula offset + alignment - 1 y layoutFutureType suma tamaños y padding mediante uint64_t sin comprobación explícita en esas operaciones. La ruta obtiene layouts del resultado y error por valueLayoutForTypeKey y devuelve error si son desconocidos; otras reglas o productores pueden limitar tamaños antes de llegar aquí. No se ha demostrado overflow alcanzable, desalineación emitida ni corrupción. **Pendiente:** seguir construcción de tipos Future, límites de tamaño y alineación, layout de agregados y llamadas MIR/LLVM, incluidos arreglos anidados de LKF-117; diseñar casos futuros de límites sin reservar objetos enormes ni ejecutarlos en fase 0. LK-654.


**Contraste NSC-510 de LKF-117, LKF-171 y LKF-388:** A:/nsc-devel/src/Frontend/ProjectObjectLayoutModel.cpp:282–396, :404–493, :826–855 y :1018–1055; SHA-256 4cce71cf1f054212e58a9c9f0ff94531ec911673c8bed83f9073d3dc68990cb4. valueLayoutForTypeKey usa producto directo para arreglo fijo, mientras layoutTypeSyntax usa layoutArray con diagnóstico de overflow para campos. Referencias y varios tipos tienen tamaños fijos propios de esta vía, y tuplas usan computeWindowsX64AggregateLayout. La comprobación de solapamiento consulta typeTransitivelyOwnsStorage; hasTrivialStructLifetime descompone arreglos fijos y recorre structs, sin descomponer tuplas explícitamente en esa función. Es evidencia local: no establece tipos legales, desbordamiento alcanzable ni fallo de ownership; seguir validaciones y consumidores. LK-380, LK-440 y LK-648.


### LKF-395 — Valor ausente representado como cero en volcado MIR

**Componente:** compilador NativeSharp, volcado textual MIR. **Estado:** comportamiento local observado; invariantes de productores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectOopMirModelDumper.cpp:97–119 y :350–357; SHA-256 d8d102a81202272e830bd34e6cc246c72d19ee4780898de40d8afdaecc8bf60e. Para nodos de pruebas y casts, targetTypeOrdinal.value_or(0) imprime target-type=#0 cuando el ordinal es cero o está ausente; para dirección/carga de bit-field, bitOffset y bitWidth también usan value_or(0), y para AggregateElementStore con anchura presente lo hace bitOffset. La cabecera imprime versión de esquema, pero el texto por sí solo no distingue esos casos. Puede que los productores/verificadores garanticen presencia en todo MIR válido; no se ha demostrado un nodo válido incompleto ni un error de compilación. **Pendiente:** contrastar construcción, verificación, nodos restaurados y semántica de ordinal cero; diseñar casos futuros válidos e incompletos, sin ejecutarlos en fase 0. LK-655.


### LKF-396 — Inferencia sintáctica del tipo de resultado opaco

**Componente:** compilador NativeSharp, transformación de some Trait. **Estado:** alcance local de inferencia observado; casos admitidos y validaciones posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectOpaqueResultModel.cpp:326–395, :603–616 y :669–704; SHA-256 38c4e949e5546bbb65bab93318c488aa3d92d7a566065b4b29be25aa369439e2. collectValueTypes acumula por nombre con emplace recorriendo nodos, sin alcance léxico explícito en este helper; inferExpressionType reconoce un conjunto limitado de expresiones, usa nombres de llamada para consultar retornos y compara el spelling para condicionales. Las llamadas con el mismo nombre y retornos distintos llevan la entrada a null; una expresión desconocida o retornos de spelling distinto provocan diagnóstico de inferencia. Esto no prueba aceptación incorrecta con shadowing, ni que un caso sea legal o alcance el backend. **Pendiente:** seguir orden y ámbitos, precedencia semántica, sobrecargas, validación del retorno reescrito y contratos de move/borrow; preparar casos futuros de homónimos, sombras y expresiones no reconocidas sin ejecutar fase 0. LK-656.


### LKF-397 — Identidad y resolución textual de traits en resultado opaco

**Componente:** compilador NativeSharp, síntesis de some Trait. **Estado:** selección local observada; visibilidad y unicidad previas o posteriores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectOpaqueResultModel.cpp:92–103, :106–169, :582–599, :655–666 y :789–839; SHA-256 38c4e949e5546bbb65bab93318c488aa3d92d7a566065b4b29be25aa369439e2. Los mapas registran nombre corto con emplace y la búsqueda del trait prueba ese nombre antes del nombre calificado con namespace. La conformidad compara spelling y el nombre del destino de implementaciones; findAssociatedDefinition recorre implementaciones por nombre concreto y elemento asociado sin comprobar ahí el trait específico. La superficie heredada busca igualmente nombre directo primero. No se ha establecido si nombres homónimos son visibles, si los mapas ya están restringidos por proyecto o si otras validaciones previenen selección incorrecta; no se afirma error reproducido. **Pendiente:** contrastar resolución oficial, importaciones y alcance de unidades, traits homónimos o múltiples implementaciones, genéricos y miembros asociados; diseñar casos futuros de identidad y cambios de orden sin ejecutar fase 0. LK-657 y LKF-385.


### LKF-398 — Continuación de argumentos en escape de indexador por referencia

**Componente:** compilador NativeSharp, evaluación de procedencia de referencias devueltas por indexador. **Estado:** diferencia de flujo observada; condiciones legales y efecto pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectReferenceEscapeEvaluator.cpp:63–81, :222–239, :256–275 y :299–326, :359–378; SHA-256 c02106a50b4cd94cadeba1233811c18aa2b48b336360b8ece3d01b19dc5c6a36. Para un indexador resuelto que devuelve ref con lifetime distinto de this/static, startIndex crea un frame CallArguments y programa el primer argumento aplicable, pero no inserta ese frame en continuations. La ruta de llamada startCall sí lo inserta, lo que permite a resume seguir argumentos cuando corresponde. Así, localmente startIndex no dispone de esa continuación para visitar argumentos posteriores; una continuación exterior podría existir por otra expresión. No se ha demostrado que indexadores admitan varios argumentos vinculados al mismo lifetime, que esa combinación sea alcanzable, ni que otra comprobación evite un escape inseguro. **Pendiente:** contrastar reglas y binding de indexadores, mapeo de argumentos, origen por defecto, verificador de retorno y tratamientos de this/static; diseñar casos futuros de varios argumentos con orígenes diferentes, incluido control de llamada equivalente, sin ejecutar fase 0. LK-658.


### LKF-399 — Comprobación de longitud en lector de claves de tupla

**Componente:** compilador NativeSharp, lectura de identidad estructural de tupla. **Estado:** operación local sin guarda de suma observada; procedencia y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/ProjectTypeConversion.cpp:744–784; SHA-256 fdc0c228cd9046edf6ecd1019eb6d12e239744850d67e2958659e07318c8a3f7. tupleElementTypeKeys lee length con from_chars en size_t y verifica cursor + length > close antes de extraer la subcadena y avanzar cursor. La suma unsigned puede envolver si una clave contiene una longitud extrema; la condición local no comprueba explícitamente el desbordamiento. tupleTypeKey genera longitudes a partir de cadenas efectivas, por lo que claves producidas por esa vía no demuestran el caso. No se ha establecido si metadatos, caché u otra entrada puede suministrar claves arbitrarias, ni un acceso inválido o fallo observable. **Pendiente:** seguir productores, lectores y validación de NSMETA/cachés, límites de tamaño y uso de la identidad; diseñar casos futuros de longitudes extremas, truncadas y claves válidas sin ejecutar fase 0. LK-659.


### LKF-400 — Validación local de contratos de escape en snapshots

**Componente:** compilador NativeSharp, lectura de snapshots precompilados. **Estado:** ausencia local de validación de dominio observada; protección del contenedor y consumidores pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/Snapshot/SnapshotCodecSupport.cpp:65–77 y :102–122; SHA-256 6e4aff21362a40529c313e3144cec2d3617f6980c0b36c1c5327a38f04a44f1b. readSnapshotEscapes convierte cada byte directamente a language::EscapeClass y readSnapshotParameter convierte bytes a enums de modo de paso y ownership; estos helpers no comparan explícitamente sus valores con el dominio de cada enum. El reader sí puede registrar fallo de lectura y el contenedor u otro pase podría validar integridad, versión o valores. No se ha demostrado aceptación de snapshot inválido, ruta alcanzable con entrada alterada ni clasificación insegura en compilación. **Pendiente:** seguir formato, autenticidad/integridad, límites, validadores, restauración y consumidores de cada enum; preparar casos futuros truncados y con valores fuera de dominio y verificar rechazo conservador, sin ejecutarlos en fase 0. LK-660.


### LKF-401 — Precondición de tipo en verificación de llamadas HIR

**Componente:** compilador NativeSharp, verificador HIR general. **Estado:** precondición local observada; invariantes de productores y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/HIR/HirVerifier.cpp:724–745; SHA-256 c02d266487655bfa0796b70960a3499609fd69216b7a988cfa4716c2c38d6e60. verifyCallNode comprueba que symbol exista y sea Function, y a continuación accede a symbol()->type()->kind() sin comprobar explícitamente que type() sea no nulo. Después verifica tipo de retorno y cantidad de argumentos. Si un símbolo Function sin tipo pudiera llegar a esta ruta, la guarda local no lo rechazaría limpiamente antes de la desreferencia. No se ha determinado si constructores, productores o validaciones anteriores impiden ese estado, ni si puede llegar HIR no confiable; no hay fallo reproducido. **Pendiente:** seguir creación y invariantes de símbolos, flujo de verificación y restauración; diseñar caso futuro de HIR incompleto y control válido, sin ejecutarlo en fase 0. LK-661.


### LKF-402 — Propagación local del fallo de rama else en HIR

**Componente:** compilador NativeSharp, lowering HIR general. **Estado:** diferencia local observada; manejo del error y alcance de la ruta pendientes. **Evidencia:** A:/nsc-devel/src/HIR/Internal/IterativeHirLowering.cpp:638–680 y :1619–1623; SHA-256 d628ee29b41cf80c93135fcfafd7067079f18fd3597b8b79cf2bb894a71883e1. En estado 12, continueConditionalStatementFrame guarda el resultado de traducir else en third, pero decide devolver null solo si faltan first o second; un resultado null de else no participa en esa condición. HirVerifier.cpp:156–166 permite third ausente, como corresponde a un if sin else. La ruta de fallo puede establecer error_ y un llamador podría rechazar todo el lowering, por lo que no se ha demostrado que se emita código con else omitido ni que un caso fuente válido llegue a ese estado. **Pendiente:** seguir fail, contratos de resultado, llamadores, diagnósticos y verificador en la ruta general; preparar casos futuros con rama fallida y controles con y sin else, sin ejecutar fase 0. LK-662.


### LKF-403 — Cobertura condicionada de comprobaciones del header C

**Componente:** compilador NativeSharp, generador de encabezados C para host. **Estado:** comportamiento visible del texto generado; compilación de host no realizada. **Evidencia:** A:/nsc-devel/src/Interop/CHeaderGenerator.cpp:596–627 y :861–895; SHA-256 bcfd188c3735f6f8d0085558596cc99d4182e2750544a4486009f51c9408c249. El preámbulo define NS_STATIC_ASSERT y NS_ALIGNAS funcionales para C++ o C11+, pero en la rama C sin C11 los define vacíos. Las aserciones emitidas verifican tamaño y alineación de structs y offsets de campos, omitiendo campos bit-field y carriers anónimos; para promovidos también omiten bit-fields y layouts no encontrados. No se ha determinado qué compiladores o modos de host se aceptan oficialmente ni si otra herramienta verifica esos casos; no se ha generado ni compilado un header. **Pendiente:** definir versiones admitidas, política para C anterior a C11, verificación ABI de bit-fields/anónimos y cobertura con distintos compiladores y flags; diseñar casos futuros sin ejecutarlos en fase 0. LK-663.


### LKF-404 — Profundidad y alcance de descripción de tipos Clang

**Componente:** compilador NativeSharp, sesión de interoperabilidad Clang. **Estado:** límite de extracción visible; comportamiento de consumidores pendiente. **Evidencia:** A:/nsc-devel/src/Interop/Cpp/ClangSession.cpp:66–186, :341–362 y :749–752; SHA-256 539653c8df071db5eb715e3a2ca6143bb07ed1a29bc44b74ae4e0e33a41aa872. describeType solo construye componentes recursivos de elemento, firma de función y tipo subyacente de enum mientras depth < 64; al alcanzar ese límite conserva información exterior pero no rellena el componente anidado por esa vía, sin diagnóstico explícito en este helper. collectFreeFunctions y el recorrido de métodos excluyen plantillas de función descritas y contextos dependientes en esas rutas. Esto no establece que la importación posterior acepte información incompleta ni que no existan otras rutas para especializaciones; tampoco describe análisis de binarios sin fuente. **Pendiente:** seguir validador de CppType, decisiones y diagnósticos de importación, identidad y fallback ante profundidad, y distinguir declaración de plantilla de especialización concreta; diseñar casos futuros de 63/64/65 niveles y plantillas dependientes sin ejecutar fase 0. LK-664.


### LKF-405 — Proyección conservadora por familia de métodos C++

**Componente:** compilador NativeSharp, importación experimental de métodos C++. **Estado:** restricción visible en rutas examinadas; sin ejecución. **Evidencia:** A:/nsc-devel/src/Interop/Cpp/CppSemanticFunction.cpp:440–478, :495–529 y :870–989; SHA-256 58ffb51e66c6de1e02e05ad4c7a0bc71ac8109430374412c28ac31364495c730. Las proyecciones de métodos estáticos y de instancia requieren target Windows MSVC x64 y aplican filtros a acceso, tipo, convención y otros atributos. Los proveedores agrupan por identidad y nombre: si un miembro de la familia no se proyecta, o aparece el tipo de método opuesto en esa ruta, marcan la familia bloqueada y omiten sus métodos; también omiten familias con colisión de firmas proyectadas. El proveedor devuelve éxito tras recopilar las familias restantes en esta ruta. No se ha determinado qué diagnósticos producen otros componentes ni se ha probado un caso fuente; no implica que todas las sobrecargas C++ o llamadas indirectas fallen. **Pendiente:** contrastar reglas de uso, diagnósticos y tratamientos alternativos, preservando la familia y motivo de exclusión; diseñar casos futuros con sobrecargas soportadas y no soportadas, firmas que colapsan y mezclas static/instance legales sin ejecutar fase 0. LK-665.


**Contraste NSC-528 de LKF-404:** A:/nsc-devel/src/Interop/Cpp/CppSemanticFunction.cpp:70–86, :190–201 y :260–267; SHA-256 58ffb51e66c6de1e02e05ad4c7a0bc71ac8109430374412c28ac31364495c730. mapFunctionType rechaza profundidad mayor de 64, punteros sin elemento y punteros de función sin firma o prototipo; el camino de arreglos también exige elementos y límites. Esto muestra controles conservadores para esas formas en esta proyección; no establece el manejo de todas las formas de CppType o todos los consumidores. LK-664.


### LKF-406 — Campos estáticos C++ thread-local no proyectados en ruta experimental

**Componente:** compilador NativeSharp, proyección experimental de campos estáticos C++. **Estado:** restricción visible en fuente; no ejecutada. **Evidencia:** A:/nsc-devel/src/Interop/Cpp/CppSemanticRecord.cpp:539–575 y :583–616; SHA-256 c21ffe70ab56795cd1244402b2857580e140bb2ad9c17ec143adce6982adfe6c. projectCppSemanticStaticField exige target Windows MSVC x64, record completo, campo público y símbolo externo; devuelve nullopt si el campo es threadLocal. provideExperimentalCppStaticFields solo añade los campos que esa proyección acepta y finaliza con éxito si no hay otros errores. No se ha establecido cómo diagnostican usos posteriores, ni si existe otra ruta que maneje estos campos; esto no describe todos los TLS de C o C++. **Pendiente:** seguir resolución de acceso, diagnósticos, rutas alternativas y modelo TLS por módulo/hilo; preparar casos futuros de campo thread_local y control no TLS, sin ejecutar fase 0. LK-666.


### LKF-407 — Cobertura parcial del reporte textual ABI

**Componente:** compilador NativeSharp, LibraryAbiReporter. **Estado:** contenido de la ruta examinada visible; usos y artefactos generados pendientes. **Evidencia:** A:/nsc-devel/src/Interop/LibraryAbiReport.cpp:125–140 y :255–360; SHA-256 8cc83f90480ab9f92a2de60c76f5242075a9b0945da72854b5f7b30a0ade8402. write emite cabecera, tipos, miembros, tablas de despacho y exports; no recorre directamente los catálogos namespaceFunctions o namespaceStates. Para exports imprime nombre, tipo de retorno y tipos de parámetros, pero esa sección no incluye por sí misma ownership ni lifetime. Una función de namespace exportada puede aparecer como export, y otros reportes o metadatos pueden aportar sus contratos. No se ha generado un reporte ni comprobado que toda función pública de namespace esté ausente de otras secciones o rutas. **Pendiente:** contrastar productores, versionado, catálogo de funciones/estados y consumidores; conservar metadata estructurada y cobertura por sección sin inferir falta de ownership por ausencia textual. Diseñar casos futuros de función namespace exportada/no exportada y estado global, sin ejecutar fase 0. LK-667.


**Contraste NSC-533 de LKF-407:** A:/nsc-devel/src/Interop/LibraryApiDiff.cpp:51–96, :265–300, :355–367, :450–482 y :622–676; SHA-256 600c0831f40740369d92e24751f74edde5eff56a7d36af24447029fc9ea71ff5. A diferencia del reporte textual ABI, la proyección sourceSurface sí incorpora funciones y estados de namespace y, para funciones, contratos de parámetros, lifetime y resumen de escape. abiSurface incluye funciones y estados, pero parameterAbiContract se limita a tipo, forma de paso y pack; no equivale al contrato fuente. Los fingerprints son hashes de estas proyecciones, no identidad del binario ni prueba de seguridad dinámica. La existencia de estas rutas no demuestra que un consumidor de LeakKiller las invoque o pueda recuperar todos los campos desde la huella. LK-667.


### LKF-408 — Metadatos anteriores a 1.35 sin resumen de escape

**Componente:** compilador NativeSharp, escritura de metadatos de biblioteca. **Estado:** pérdida de precisión por versión visible en la ruta de downgrade; manejo efectivo por lectores pendiente. **Evidencia:** A:/nsc-devel/src/Interop/LibraryMetadataCodec.cpp:166–200, :551–565 y :634–644; SHA-256 a5a847cfad8ccda4189ac0bb10e241267886663639f337063fbc5a3517efd973. Para formato 1.35 o posterior normalizeWritableEscapeSummaries rellena los resúmenes vacíos con Unknown por parámetro y rechaza un conteo incompatible; para minor anterior a 35 validateMetadataBefore38 limpia los resúmenes de miembros y funciones de namespace. El comentario afirma que los consumidores antiguos manejan conservadoramente la ausencia; aún falta verificar lectores y consumidores concretos. La ausencia del resumen no significa que un parámetro no escape, ni demuestra un defecto o fuga. **Pendiente:** seguir escritura pública, decodificación y uso de resúmenes por versión, comprobar preservación de estado Unknown y diseñar casos futuros con contratos conocidos y desconocidos en 1.34/1.35 sin ejecutar fase 0. LK-668.

**Contraste NSC-535 de LKF-385:** A:/nsc-devel/src/Interop/LibraryMetadataConstruction.cpp:2710–2769; SHA-256 88f4c14639e5fd0e8a2d27e70e49bb62ceaa2cdc5800155efbed60812de5fe2a. El productor de NSMETA también busca el primer tipo cuyo identity.name() coincide con el texto de la parte externa y recopila todos los miembros con ese ordinal. Seguir si la semántica garantiza unicidad y qué valida el consumidor; no se ha reproducido una asociación incorrecta. LK-645.

### LKF-409 — Valor de respaldo del layout de enum en NSMETA

**Componente:** compilador NativeSharp, construcción de metadatos. **Estado:** comportamiento local observado; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/Interop/LibraryMetadataConstruction.cpp:2066–2090 y :2160–2202; SHA-256 88f4c14639e5fd0e8a2d27e70e49bb62ceaa2cdc5800155efbed60812de5fe2a. Si la consulta del modelo de layout no devuelve valor, appendEnumLayoutMetadata publica tamaño y alineación 4/4, aunque conserva por separado la clave del tipo subyacente. El fragmento no establece si una declaración válida puede llegar a ese respaldo ni si una validación anterior impide emitir metadatos inconsistentes. **Pendiente:** seguir consulta, diagnósticos y consumidores; preparar casos futuros de enums con tamaños y signos distintos, dependencias y ausencia de layout; evitar atribuir certeza al layout solo por este respaldo. Sin reproducción ni ejecución en fase 0. LK-669.

### LKF-410 — Fusión de fragmentos NSMETA y conformidades externas

**Componente:** compilador NativeSharp, fusión de metadatos. **Estado:** omisión local observada; condiciones de uso y efecto pendientes. **Evidencia:** A:/nsc-devel/src/Interop/LibraryMetadataMerger.cpp:221–235 y :461–531; SHA-256 8d7dcef2ffd5fc2cf2e838fde43f7e9939ea991f276d9f4adfff284734db3696. El candidato parte como copia del primer fragmento y combina explícitamente múltiples secciones, pero no vacía ni agrega traitImpls de fragmentos posteriores; tampoco compara languageFeatureFlags en sameHeader. La construcción de conformidades externas aparece en LibraryMetadataConstruction.cpp:2710–2769. No se ha verificado si los fragmentos admiten diferir en estos campos, cómo se invoca el merger ni si hay controles externos, por lo que no se afirma pérdida en un flujo real. **Pendiente:** seguir llamadores, contrato de fragmentos, serialización y consumidor; preparar casos futuros con conformidades distintas, cambios de orden, banderas y fragmentos idénticos. Mantener incertidumbre en correlación intermodular; no ejecutar fase 0. LK-670.

### LKF-411 — Contratos adicionales de métodos de traits externos en NSMETA

**Componente:** compilador NativeSharp, serialización de metadatos. **Estado:** diferencia de representación observada; efecto en importación pendiente. **Evidencia:** A:/nsc-devel/src/Interop/LibraryMetadataPayload.cpp:201–272, :425–530, :550–575, :1202–1245, :1681–1697, :1811–1831, :1834–1878 y :2004–2077; SHA-256 f912a640e9fb84022d6722e4af661a47fd20847573d7f29d801efc2acda354a2. Los métodos de traitImpls se escriben y leen con encodeMember/decodeMember, pero los catálogos separados de lifetime, escape, tipo de error, contratos FFI y calificadores enumeran metadata.members y otros catálogos ordinarios, sin enumerar traitImpls.methods. El productor construye esos métodos con buildMetadataMember (LibraryMetadataConstruction.cpp:2710–2769), que puede llenar campos adicionales. Esto describe la representación y una posible pérdida al transportar esos campos; falta contrastar si se reconstruyen desde otra fuente, qué combinaciones son legales y qué utiliza el importador. No hay reproducción. **Pendiente:** seguir cada campo desde productor a consumidor y formato vigente, comparar contratos de método ordinario y externo, preparar casos futuros por separado y decidir tratamiento conservador como información desconocida para LeakKiller; no ejecutar fase 0. LK-671, LK-645 y LK-670.

**Contraste NSC-538 de LKF-411:** A:/nsc-devel/src/Interop/LibraryMetadataVerification.cpp:1232–1285, :1332–1347 y :1795–1835; SHA-256 7eac95d9751ccdde3e86fa64e0c8809a7db2bcc312343dc310723bfd200bee22. Las funciones locales de referencias de tipos y de lifetime recorren miembros ordinarios y funciones namespace, sin recorrer traitImpls.methods. Esto acota esas verificaciones concretas; falta rastrear otras validaciones, importación y reconstrucción antes de concluir efecto. LK-671.

**Contraste NSC-538 de LKF-386:** A:/nsc-devel/src/Interop/LibraryMetadataVerification.cpp:1696–1752; SHA-256 7eac95d9751ccdde3e86fa64e0c8809a7db2bcc312343dc310723bfd200bee22. La verificación de params exige posición final, paso por valor, ausencia de valor por defecto y que typeKey contenga el texto ReadOnlySpan__g1__; esta comprobación local no compara identidad canónica completa. Contrastar las validaciones semánticas previas y del importador. LK-646.

### LKF-412 — Multiplicación del ancho SIMD en validación local

**Componente:** compilador NativeSharp, semántica ABI/SIMD. **Estado:** aritmética local observada; alcanzabilidad y efecto pendientes. **Evidencia:** A:/nsc-devel/src/Language/AbiSemantics.cpp:85–92, :95–118 y :127–141; SHA-256 6af1e8238946f760b0b62b41bfe773f567d97b956e8d913eeb2e7300c7e83b47. validateSimd exige potencias de dos, límites de laneBits y mínimo de laneCount, y compara laneBits * laneCount con 512 sin una guarda local antes de multiplicar. Con valores size_t extremos la aritmética sin signo puede envolver; parsePortableSimdType acepta un conteo representable por from_chars y delega a portableSimdType. No se han contrastado los llamadores, límites previos ni las fases posteriores, y no se ha reproducido que un programa válido alcance o aproveche esa ruta. **Pendiente:** revisar origen de entradas y límites, propagación de laneCount y diagnósticos; diseñar casos futuros cerca de límites y desbordamiento, sin ejecutarlos en fase 0. LeakKiller debe verificar operaciones de tamaño con aritmética comprobada y registrar incertidumbre. LK-672.

### LKF-413 — Longitudes y reserva del lector de identidad de función

**Componente:** compilador NativeSharp, identidad de tipos de función. **Estado:** límites locales observados; entrada alcanzable y efecto pendientes. **Evidencia:** A:/nsc-devel/src/Language/CallableSemantics.cpp:27–41, :229–294; SHA-256 cc936d3f14d783b24ae75dd7b7fb2283f27603765a1e2121b3942e17ecc2d186; un uso de parseFunctionTypeIdentity para claves fn: aparece en LibraryMetadataVerification.cpp:1092–1147. consumeLengthPrefixed comprueba cursor + length contra input.size() sin guarda local de desbordamiento; parseFunctionTypeIdentity reserva parameterCount obtenido del texto antes de comprobar que existan los parámetros. isFunctionTypeIdentity es noexcept aunque llama al parser que puede reservar memoria; otra ruta lambda sí captura excepciones. Falta determinar límites efectivos de la fuente, del payload y de validaciones previas, manejo de excepciones y si alguna entrada alcanza la situación; no hay reproducción. **Pendiente:** seguir todos los llamadores, formatos y cotas, estudiar excepciones y diseñar casos futuros de longitudes y conteos extremos, truncamiento y entrada válida; no ejecutar fase 0. LK-673 y LKF-399.

### LKF-414 — Ambigüedad de la clave de Future con resultado puntero

**Componente:** compilador NativeSharp, semántica async. **Estado:** limitación de representación expresamente visible; aceptación en lenguaje pendiente. **Evidencia:** A:/nsc-devel/src/Language/CoroutineSemantics.cpp:18–55; SHA-256 f0ac24d4c2bc02f384c1bbed48b44127d65b2286fee96c797e02948cfc02f4b9. futureTypeKey concatena el tipo interior y futureInnerTypeKey rechaza una clave que termina en *, tratándola como puntero a future; el comentario indica que future de puntero no es representable con ese spelling. Las consultas también aparecen en ProjectObjectLayoutModel.cpp:590 y :623 y HirAsyncLowering.cpp:453, :473 y :982. Falta establecer si el frontend rechaza esa combinación antes, si existe representación alternativa y qué comportamiento percibe el usuario. **Pendiente:** seguir construcción, parsing, lowering y diagnósticos de Future<T*> y Future<T>*; diseñar casos futuros sin ejecución. LeakKiller debe distinguir tipos y marcar la forma ambigua como desconocida. LK-674.

### LKF-415 — Identidad descriptiva de plan Future con campos parciales

**Componente:** compilador NativeSharp, planes async. **Estado:** insumos de identidad observados; uso y exigencia de unicidad pendientes. **Evidencia:** A:/nsc-devel/src/Language/CoroutineSemantics.cpp:71–99 y :103–174; SHA-256 f0ac24d4c2bc02f384c1bbed48b44127d65b2286fee96c797e02948cfc02f4b9. asyncFutureIdentity calcula un identificador con tipo de retorno, almacenamiento, tamaño y alineación de frame y número de estados, sin incluir contenido de slots o comportamiento de estados. Planes distintos con los mismos campos pueden producir la misma identidad incluso antes de considerar colisiones del hash. En la búsqueda estática de src/*.cpp solo se observaron asignaciones a opaqueFutureIdentity; falta seguir cabeceras, consumidores externos y garantías pretendidas. No se afirma un fallo funcional. **Pendiente:** determinar si es etiqueta diagnóstica o identidad ABI, seguir serialización y consumidores y diseñar casos futuros de planes distintos con iguales métricas. LeakKiller no debe tratarla como identidad única sin evidencia adicional. LK-675.

### LKF-416 — Un único fingerprint de contrato por módulo en registro de vida

**Componente:** NativeSharp, contrato de vida entre módulos. **Estado:** restricción local observada; integración y alcance pendientes. **Evidencia:** A:/nsc-devel/src/Language/CrossModuleLifetime.cpp:26–42 y :60–101; SHA-256 b30268f552793dbbff0c0e283e96cc87ac1f53e3256ff5cd2b87490870f987b4. El fingerprint incluye identidad de tipo y thunks; registerObject mantiene uno solo por producerModule y rechaza cualquier contrato con fingerprint distinto, incluso si el contador regresó a cero, ya que unregisterObject solo decrementa. Por tanto contratos distintos de un mismo productor no se mezclan en esta instancia del registro. Falta comprobar si el registro se usa en ejecución o solo como modelo, si se instancia por tipo y si el diseño exige efectivamente varios contratos por módulo. No se ha reproducido un fallo. **Pendiente:** seguir consumidores y ciclo de carga/descarga, multiplicidad de tipos y asignadores, alcance de cada instancia, colisiones de fingerprint y política unload; diseñar casos futuros de altas, bajas y recarga sin ejecutar fase 0. LeakKiller debe identificar asignador, módulo, tipo y generación separadamente. LK-676.

### LKF-417 — Cobertura por categoría y optimización en auditorías semánticas

**Componente:** NativeSharp, validadores de calidad del compilador. **Estado:** condición local observada; uso como puerta real pendiente. **Evidencia:** A:/nsc-devel/src/Language/IntegrationQuality.cpp:341–407 y :419–462; SHA-256 6456a91f5cbdccdcec7851c20ca448516823e504b8e1dd3c1cd8224061717bd3. validateSemanticDebugEvidence reúne categorías y observa O0 y O2 por separado; validateZeroCostAudit reúne tipos de costo y optimizaciones por separado. Estas comprobaciones no exigen cada combinación categoría × optimización aunque el mensaje de la segunda habla de todos los tipos con ambas optimizaciones. Falta comprobar quién invoca estas funciones y si la matriz se garantiza externamente; no se han ejecutado pruebas ni se afirma una cobertura real incorrecta. **Pendiente:** seguir productores y llamadores de evidencia, diseñar casos futuros con combinaciones ausentes y matrices completas, contrastar evidencias de PDB, MIR y ensamblado en O0/O2. LK-677.

### LKF-418 — Dos cálculos de frame de generador con distintas guardas

**Componente:** compilador NativeSharp, semántica de iteración. **Estado:** diferencia local observada; llamadores y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/Language/IterationSemantics.cpp:123–138 y :149–158, :210–281; SHA-256 de3fc812cb0ddfdfc746ce2c1a3da2f034fce8f9266c916dc14721b448c558de. computeGeneratorFrame suma y alinea tamaños sin guardas locales de desbordamiento, mientras layoutGeneratorFrame usa checkedAlignAndAdvance y comprueba la alineación final. No se ha establecido si el helper sin guardas recibe tamaños extremos desde programas válidos ni si es usado en el flujo productivo; no se afirma un fallo reproducido. **Pendiente:** seguir llamadores y límites de tamaño, contrastar ambos caminos y diseñar casos futuros cerca de SIZE_MAX y con alineaciones válidas, sin ejecución en fase 0. LK-678.

**Contraste NSC-544 de LKF-414:** A:/nsc-devel/src/Language/IterationSemantics.cpp:18–84; SHA-256 de3fc812cb0ddfdfc746ce2c1a3da2f034fce8f9266c916dc14721b448c558de. iteratorInnerTypeKey y asyncIteratorInnerTypeKey también rechazan claves terminadas en *, aunque el constructor concatena el tipo de elemento. Determinar si iteradores de elementos puntero son legales o reciben codificación alternativa y contrastar los lectores de tipo de error. LK-674.

### LKF-419 — Aritmética decimal y divisores truncados

**Componente:** compilador NativeSharp, semántica numérica. **Estado:** comportamiento local observado; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/Language/NumericSemantics.cpp:316–387; SHA-256 e0090e425c7fd091204b8ff171d084ea169d2d8105e9400034ec34eabaae1d19. evaluateDecimalArithmetic convierte a uint64_t los coeficientes del divisor para división y resto y los usa sin una comprobación local de cero; un coeficiente de 96 bits puede tener sus 64 bits bajos en cero. Suma/resta también usan magnitudes alineadas y el signo izquierdo en la rama local; el cálculo multiplica magnitudes antes de comprobar el límite final. Falta contrastar validaciones previas, semántica esperada y lowering; no hay reproducción. **Pendiente:** Seguir llamadores y contratos; diseñar casos futuros de divisor cero, coeficiente no nulo con bits bajos cero, signos, escala y límites. No ejecutar fase 0. LK-679.

### LKF-420 — Multiplicación entera unsigned en intermediario signed

**Componente:** compilador NativeSharp, semántica numérica. **Estado:** comportamiento local observado; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/Language/NumericSemantics.cpp:612–640 y :683–725; SHA-256 e0090e425c7fd091204b8ff171d084ea169d2d8105e9400034ec34eabaae1d19. evaluateIntegerArithmetic representa operandos unsigned de hasta 64 bits en __int128 con signo y multiplica antes de comprobar overflow. El producto de dos valores uint64 máximos excede el rango positivo de signed __int128; la política se aplica después. No se han seguido llamadores ni validaciones previas y no hay reproducción. **Pendiente:** Investigar entradas alcanzables y políticas checked, wrapping, saturating y unchecked; preparar casos futuros en torno a 2^127 y límites de 64 bits. No ejecutar fase 0. LK-680.

### LKF-421 — Aritmética extrema de punteros en memoria constante

**Componente:** compilador NativeSharp, semántica numérica. **Estado:** comportamiento local observado; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/Language/NumericSemantics.cpp:896–940; SHA-256 e0090e425c7fd091204b8ff171d084ea169d2d8105e9400034ec34eabaae1d19. ConstantMemory::offset convierte offset size_t a ptrdiff_t, calcula -delta cuando delta es negativo y suma current + delta; no hay guardas locales para offset mayor que PTRDIFF_MAX o delta igual a PTRDIFF_MIN. Falta verificar límites de asignación y productores de delta, así como usos reales; no hay reproducción. **Pendiente:** Seguir procedencia y límites del modelo de memoria constante; diseñar casos futuros en extremos de ptrdiff_t y asignaciones grandes. No ejecutar fase 0. LK-681.

### LKF-422 — Movimiento completo de valor parcialmente movido en helper local

**Componente:** compilador NativeSharp, modelo de propiedad. **Estado:** condición local observada; invariantes de entrada pendientes. **Evidencia:** A:/nsc-devel/src/Language/OwnershipSemantics.cpp:10–17 y :32–42; SHA-256 c769bfda201cef9ef189591608e677474897af988e26c61a8b61467992069649. moveWholeValue para un valor no Copy rechaza solamente si ningún campo está inicializado; si hay mezcla de campos inicializados y movidos, acepta y marca todos como no inicializados. classifyValueState distingue ese estado como PartiallyMoved; con vector vacío lo clasifica Moved. Falta establecer si el llamador exige estado completamente inicializado, cómo representa valores sin campos y qué exige el lenguaje; no hay reproducción de uso después de move. **Pendiente:** seguir inicialización de MoveState, llamadas, diagnósticos y cleanup; diseñar casos futuros de movimiento parcial seguido de movimiento completo y tipo sin campos. LK-682.

### LKF-423 — Longitud extrema en lectura de resumen de escape

**Componente:** compilador NativeSharp, serialización de resumen de escape. **Estado:** condición aritmética observada; fuente y límites pendientes. **Evidencia:** A:/nsc-devel/src/Language/OwnershipSemantics.cpp:94–124; SHA-256 c769bfda201cef9ef189591608e677474897af988e26c61a8b61467992069649. decodeEscapeSummary verifica encoded.size() < nameSize + 1 y después accede encoded[nameSize]; si nameSize es SIZE_MAX la suma sin signo puede envolver antes de la comparación. El parser sí valida el dominio de clases y asigna la salida solo tras completar la lectura. Falta determinar si una entrada externa puede llegar sin cotas previas y cómo se maneja el error; no hay reproducción. **Pendiente:** seguir productores y consumidores, límites del archivo y casos futuros de longitud extrema, vacíos y truncamiento. LK-683.

**Contraste NSC-550 de LKF-414:** A:/nsc-devel/src/LanguageService/Internal/MemberCompletionResolver.cpp:301–353 y :840–880; SHA-256 53ff9762b6cdd88bb07c88678e7731aa91256989da6e90a4a7ccced99cdb46ae. La presentación de Future, Iterator y AsyncIterator y las sugerencias intrínsecas de iteración consultan los helpers de claves antes registrados. La función de presentación puede devolver cadena vacía si no reconoce la clave; falta verificar si las formas ambiguas son alcanzables y cómo se muestra el resultado. LK-674.

### LKF-424 — Token UTF-8 conserva clase de literal después de error léxico

**Componente:** compilador NativeSharp, lexer. **Estado:** transformación local observada; efecto posterior pendiente. **Evidencia:** A:/nsc-devel/src/Lexer/Lexer.cpp:701–706 y :709–806, :880–916; SHA-256 86ead5d1e4e0265340d3db0f3d0e73e887c39267cada1ea9512efd71b645383b. lexUtf8String llama lexString y construye siempre un token Utf8StringLiteral con su rango y valor, incluso si lexString devolvió Invalid por error de secuencia o cadena sin terminar; lexString emite diagnóstico de error en esas rutas. Falta contrastar parser, valor del token y política de detención ante diagnósticos; no se afirma aceptación de programa inválido. **Pendiente:** seguir token y errores a AST y controlador; diseñar casos futuros u8 normales y raw, escapes inválidos y truncamiento. LK-686.

### LKF-425 — Interpolación cerrada con token inválido sin diagnóstico local

**Componente:** compilador NativeSharp, lexer. **Estado:** ruta local observada; diagnóstico posterior pendiente. **Evidencia:** A:/nsc-devel/src/Lexer/Lexer.cpp:919–997; SHA-256 86ead5d1e4e0265340d3db0f3d0e73e887c39267cada1ea9512efd71b645383b. Una llave de cierre aislada fuera de un hueco marca valid=false; si después aparece la comilla de cierre, la función devuelve Invalid directamente sin reportar ahí el diagnóstico InvalidStringLiteral. El camino de fin sin cierre sí lo reporta. Otras validaciones o el parser podrían diagnosticar el token; aún no se han contrastado y no hay reproducción. **Pendiente:** seguir tratamiento de Invalid y diagnósticos, comparar interpolación raw y regular y diseñar casos futuros con llaves aisladas, escapadas y anidadas. LK-687.

### LKF-426 — Limpieza silenciosa de temporales del enlazador

**Componente:** enlazador NativeSharp, directorio temporal. **Estado:** comportamiento local observado; consecuencias operativas pendientes. **Evidencia:** A:/nsc-devel/src/Linker/Internal/ScopedTemporaryDirectory.cpp:10–59 y :69–74; SHA-256 9ab2feed105e7b06820c3e964b513ddef84d01ba9c698b6405d5643883fcd270. Al destruir el objeto se llama remove_all con error_code, pero el resultado no se comunica; si keepRoot está configurado se conservan los archivos. No se ha verificado si otros mecanismos reportan fallos o si un caso concreto deja archivos. **Pendiente:** seguir llamadores, opciones de preservación y límites de vida de PDB y objetos; diseñar casos futuros de limpieza fallida, interrupción y conservación explícita sin ejecución. LK-688. Este hallazgo es del enlazador, no se incluye en el documento exclusivo del compilador.

**Contraste parcial de LKF-426:** A:/nsc-devel/src/Linker/LldLinker.cpp:673–746; SHA-256 f448d5915cb0d2fac527919b6f6ee8615cb34568ca7b8e3c51d4ecb493bc27af. En generación de import library, NSC_KEEP_LINK_TEMPS evita borrar rsp y log, pero .def y dummy.dll se eliminan incluso con esa opción; cleanupError tampoco se comunica en esta ruta. Contrastar rutas restantes y distinguir preservación parcial de una reproducción completa. LK-688.

### LKF-427 — Directorio de staging de enlace derivado de la salida

**Componente:** enlazador NativeSharp. **Estado:** estrategia local observada; concurrencia y protecciones externas pendientes. **Evidencia:** A:/nsc-devel/src/Linker/LldLinker.cpp:913–1005, :1183–1211 y :1370–1424, :1505–1558; SHA-256 f448d5915cb0d2fac527919b6f6ee8615cb34568ca7b8e3c51d4ecb493bc27af. En Windows prepareStableLinkInputs usa <outputPath>.nslink-inputs, llama remove_all antes de crearlo y usa nombres estables; finishStableLinkInputs lo elimina salvo preservación explícita. Dos operaciones que alcancen la misma salida compartirían esa ruta en estas funciones. No se ha establecido si los llamadores serializan el acceso o proporcionan salidas temporales únicas ni se ha reproducido interferencia. **Pendiente:** seguir transacciones y exclusión mutua, variaciones de ruta y conservación, preparar casos futuros de enlace simultáneo y fallo sin ejecución. LK-689.

**Contraste NSC-556 de LKF-426:** LldLinker.cpp:891–899 y :1183–1211 usa la opción de conservación para rsp/log y directorio estable; la publicación mueve o copia el PDB fuera antes de la limpieza. Fallos de remove/remove_all se guardan en error_code sin propagación local. LK-688.

### LKF-428 — Cobertura y terminación del catálogo de imports PE

**Componente:** enlazador NativeSharp, verificador PE. **Estado:** límites locales observados; validaciones externas pendientes. **Evidencia:** A:/nsc-devel/src/Linker/PeExecutableVerifier.cpp:416–461 y :471–523; SHA-256 1d62e07e5c7ba1c31e083bc56218a24b192c0486af6abc7b4c58cbb38d37826e. El recorrido visita hasta 4096 thunks por biblioteca y 128 descriptores; si alcanza el límite no exige un terminador nulo antes de retornar éxito. Los thunks por ordinal se saltan sin añadir entrada a importedSymbols. No se ha establecido si otros controles limitan el tamaño o validan terminación, ni se ha reproducido aceptación incorrecta. **Pendiente:** seguir consumidores y otros verificadores; diseñar casos futuros con imports por ordinal, límites, tabla truncada y terminador ausente. El componente es del enlazador, por ello no se incluye en el registro exclusivo del compilador. LK-690.

### LKF-429 — Recursos y concurrencia del cargador Unicode de DLL

**Componente:** enlazador NativeSharp, código auxiliar generado. **Estado:** rutas de IR observadas; ejecución y efectos pendientes. **Evidencia:** A:/nsc-devel/src/Linker/UnicodeDllLoader.cpp:211–276; SHA-256 55a98aa313520677b05fc63c7a313a1f972546feeb1e0b28f513255aada3bb03. El resolvedor reserva path con HeapAlloc y salta a missing tras fallar GetModuleHandleExW, GetModuleFileNameW o la búsqueda de separador, sin HeapFree visible en esas ramas; missing lanza excepción no continuable. En la primera resolución concurrente, la lectura y escritura del módulo son atómicas, pero entre ambas se llama LoadLibraryExW sin compare-exchange ni liberación visible de referencias redundantes. Falta establecer alcance del manejador de excepción, garantías del sistema, inicialización y ciclo de vida real; no se ha reproducido fuga ni carrera. **Pendiente:** seguir llamadores y política de unload, diseñar casos futuros de fallos, excepciones manejadas y primeras llamadas concurrentes; instrumentar heap y referencias de módulo. Sin ejecutar fase 0. LK-691.

### LKF-430 — Reutilización incremental de ejecutable y PDB acompañante

**Componente:** enlazador NativeSharp, constructor de ejecutables. **Estado:** comprobaciones locales observadas; validación externa pendiente. **Evidencia:** A:/nsc-devel/src/Linker/WindowsExecutableBuilder.cpp:668–710 y :713–759; SHA-256 7022485867d2354695491e46484cb261fce3f95de7292924eb7fefb470569e5f. La reutilización incremental exige coincidencia de clave, verificación PE y, si hay debug, que exista el PDB esperado; esta función no lee el PDB ni compara su GUID/age con el RSDS del ejecutable. El enlace nuevo en linkAndVerifyExecutable verifica PE localmente (:454–482). Falta seguir validaciones del controlador y publicación, y establecer si una sustitución o corrupción del PDB puede sobrevivir al flujo completo; no se ha reproducido. **Pendiente:** contrastar ruta incremental y limpia, controles de identidad externos y política de caché; diseñar casos futuros con PDB ausente, distinto y truncado sin ejecución. LK-692. Hallazgo del enlazador, excluido del documento exclusivo del compilador.

**Contraste NSC-560 de LKF-426:** A:/nsc-devel/src/Linker/WindowsLibraryBuilder.cpp:32–36, :200–228 y :755–775; SHA-256 2d0dc748719c01246277ea93b35f5951a18c05f24c62628eb6333718f67fd593. El directorio temporal creado por este constructor pasa keepRoot vacío, a diferencia del constructor de ejecutables que lee NSC_KEEP_LINK_TEMPS. LldLinker puede conservar sus propios rsp/log y staging por separado; no se concluye que todos los artefactos DLL se pierdan. Seguir llamadas y rutas para determinar qué evidencia sobrevive. LK-688.

### LKF-431 — Identidad de enlazador memorizada por ruta durante el proceso

**Componente:** enlazador NativeSharp, caché de bibliotecas de importación. **Estado:** comportamiento local observado; uso en proceso prolongado pendiente. **Evidencia:** A:/nsc-devel/src/Linker/WindowsRuntimeImports.cpp:369–405 y :463–503; SHA-256 455cd639b6971ee5e4f777cd492504f75a4a450f82df5588f5dddd1869810292. importLibraryLinkerIdentity calcula tamaño y SHA-256 la primera vez y los memoriza en un mapa estático por ruta, sin volver a consultar el archivo al reutilizar esa ruta en el mismo proceso. Si la herramienta se sustituye en esa ruta, la clave local seguiría usando la identidad memorizada; falta comprobar vida del proceso, sincronización de instalación y otras invalidaciones. No se reprodujo reutilización incorrecta. **Pendiente:** seguir llamadores de larga vida y política de actualización de toolchain, diseñar casos futuros de cambio de contenido en misma ruta y comparar caché con reconstrucción sin ejecución. LK-693. Hallazgo del enlazador, excluido del registro exclusivo del compilador.

### LKF-432 — Corrección parcial silenciosa de rangos de procedimiento CodeView

**Componente:** compilador NativeSharp, postprocesado CodeView de objetos COFF. **Estado:** política local observada; efectos y garantías previas pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Internal/CodeViewProcedureRanges.cpp:98–165, :189–233 y :238–296; SHA-256 01d73e53e3ee525e8ab97c3602526ac5805a640d80bad2239766198a4ca9d45c. Si una subsección o registro no se interpreta, una función no se resuelve o el epílogo no se decodifica, los recorridos omiten o detienen el procesamiento local; apply puede devolver éxito sin modificación o guardar cambios de otros procedimientos. Si el desensamblador no está disponible sí devuelve error. Falta contrastar validación previa COFF, llamadores y diagnósticos posteriores; no se ha demostrado un PDB defectuoso. **Pendiente:** identificar cobertura y métricas por procedimiento, casos futuros con instrucciones no reconocidas, líneas ocultas, funclets y registros truncados, y efecto en stepping/breakpoints; sin ejecutar fase 0. LK-694.

**Contraste NSC-564 de LKF-432:** A:/nsc-devel/src/LLVM/Internal/CoffObjectFile.h:14–19 y :34–44; SHA-256 862e0cbffea598156119b86542cba52deb02fbffaece82186a62ff866580bbf2. El contrato declarado limita este lector al COFF normal y relocationTarget a ADDR32NB/SECREL; bigobj se trata como no AMD64 y no se modifica. Seguir la implementación y llamadores para establecer alcance real y diagnósticos. LK-694.

### LKF-433 — Traza LLVM opcional en ruta fija y truncada

**Componente:** compilador NativeSharp, telemetría de compilación LLVM. **Estado:** comportamiento local observado; uso en builds y concurrencia por contrastar. **Evidencia:** A:/nsc-devel/src/LLVM/Internal/ColdLlvmTrace.cpp:13–45 y :47–95; SHA-256 eadef6029d22cabdb8a4fe40e9a3047d021a6e4b8e3814e1ab81728084559985. Cuando kColdLlvmTraceEnabled está activo, abre A:/temp/nsc-llvm-code-unit-timing.log con trunc; si no abre, retorna sin comunicar error desde este constructor. El mutex mostrado protege la instancia, no demuestra coordinación entre instancias o procesos. Registra tiempos de compilación y caché, no ejecución del programa. Falta determinar configuración real, llamadores y sincronización externa; no se ha reproducido pérdida de datos. **Pendiente:** seguir switch y productores, diseñar casos futuros de sesiones simultáneas, ruta inaccesible e interrupción, sin ejecución en fase 0. LK-695.

### LKF-434 — Ancho y signo locales de enum en lowering LLVM general

**Componente:** compilador NativeSharp, lowering LLVM general. **Estado:** selección local observada; rutas y representación semántica pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Internal/Implementation.cpp:51–97 y :112–142; SHA-256 a9f556e6e287cd2ae38954ca46d482b8e9ea8747852f78b03c4cb5cf294a9c93. isSignedInteger trata TypeKind::Enum como signed e integerWidth le asigna 32 bits, sin consultar en estos helpers el tipo subyacente. No se ha establecido si enums de otros tamaños o signos llegan a esta ruta, si el tipo ya fue normalizado o si usan el lowering OOP separado; no hay reproducción. **Pendiente:** seguir los llamadores de ambos helpers, representación semántica y backend para enums con tipos subyacentes distintos; diseñar casos futuros de casts, comparación y almacenamiento sin ejecutar fase 0. LK-696 y LKF-409.

### LKF-435 — Negación de desplazamiento inmediato en aritmética de puntero

**Componente:** compilador NativeSharp, lowering LLVM general. **Estado:** operación local observada; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/LLVM/Internal/ImplementationExpressions.cpp:397–436; SHA-256 a8c98c9614ad7295b9d260b59e64caaa9d8229d7ed8d87a4261c6ce3e0433bb3. Para PointerSubtract con inmediato, lowerPointerArithmetic convierte el inmediato a int64_t y aplica negación en C++ antes de crear la constante LLVM; no hay guarda local para INT64_MIN, cuya negación no cabe en int64_t. No se ha establecido el dominio del inmediato, las validaciones MIR/SSA ni la ruta de código fuente que lo produce; tampoco se ha reproducido. **Pendiente:** seguir tipo y productores de inmediato, precondiciones y ruta OOP; diseñar casos futuros de límites con punteros válidos y diagnósticos apropiados, sin ejecución en fase 0. LK-697.

### LKF-436 — Alineación y volatilidad en cargas y stores LLVM generales

**Componente:** compilador NativeSharp, lowering LLVM general. **Estado:** emisión local observada; invariantes y rutas pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Internal/ImplementationStatements.cpp:288–325 y :342–371; SHA-256 fbfb02ab6e193f4d638435ec2c8eeb5595f1c7e1e127a603f1d3ef1acb2f3459. Las rutas Load/Store y LoadSymbol/StoreSymbol emiten LLVMBuildLoad2/LLVMBuildStore sin establecer alineación o volatilidad explícita en estos fragmentos. Falta establecer propiedades de MIR, construcción de punteros, flags, target y si estas formas admiten acceso desalineado o volatile; otras rutas o validaciones pueden restringirlas. No se ha reproducido comportamiento incorrecto. **Pendiente:** seguir origen de punteros, representación y validación previa; comparar lowering OOP y diseñar casos futuros de campos packed, punteros, volatile y acceso alineado, sin ejecución en fase 0. LK-698.

**Contraste NSC-004/008 de LKF-433:** A:/nsc-devel/src/LLVM/Internal/LlvmInstrumentationSwitches.h:3–13; SHA-256 f003aecb5e9eeadbb1514e23f78512e3e293fa872a1d5d81bf492044cb9e8e73. En esta versión kColdLlvmTraceEnabled y kLoweringPhaseTraceEnabled son false. El comentario indica una traza separada del programa generado, también desactivada mediante kRuntimeDebugTraceEnabled=false, con eventos de llamadas, reserva, liberación y acceso a campo; el comentario no prueba cobertura ni funcionamiento. Seguir implementación y configuración de builds. LK-695 y LK-699.

### LKF-437 — Caché ThinLTO aceptada sin comprobar contenido en el callback

**Componente:** compilador NativeSharp, backend ThinLTO. **Estado:** comportamiento local observado; consecuencias y validaciones posteriores pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Internal/ThinLtoBackend.cpp:529–557 y :566–594; SHA-256 ec24763c4789573a27923c237a21606a7949b2b4a82b0bc2f6c7e3f48c929e0d. Una entrada con nombre de clave LLVM se acepta como acierto si es archivo regular y la copia tiene éxito, sin verificar contenido ni correspondencia con la clave en este fragmento. La publicación usa un temporal con ordinal pero sin identificador de proceso y considera los errores de copia o renombre como mejor esfuerzo; la poda fallida tampoco detiene el build. Investigar las validaciones posteriores y el aislamiento de compilaciones concurrentes: no se ha demostrado corrupción ni una compilación afectada. **Seguimiento:** LK-700 y LK-544.

### LKF-438 — Indicadores de verificación heredados en la ruta preparada LLVM

**Componente:** compilador NativeSharp, optimizador LLVM. **Estado:** comportamiento local observado; contratos de llamadores pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/LlvmOptimizer.cpp:366–375, :410–430 y :456–466; SHA-256 8c33eff5ea8743636e83725f6f0c98b1c5dee4cf6967b53e0e1adbfeb4ee89aa. optimizePrepared llama a optimizeImpl con prepareModule=false; prepareOptimizationModule marca verifiedBefore=true sin verificar allí. Si además es O0 y no se requiere inlining obligatorio, la ruta rápida marca verifiedAfter=true y retorna sin efectuar otra verificación local. El comentario indica que el lowering ya verificó el módulo y que solo se adjunta configuración de target. Deben contrastarse todos los llamadores y posibles cambios intermedios; los indicadores por sí solos no prueban que ambas verificaciones se hayan ejecutado en este método. No se ha demostrado que un módulo inválido llegue a emisión. **Seguimiento:** LK-701.

**Contraste NSC-579 de LKF-438:** A:/nsc-devel/src/LLVM/LlvmTargetMachine.cpp:243–258 y :275–296; SHA-256 1d1100f1b0ca0764da88b739517285a3bc34d576902574ed9fdc42c4f3725ac8. emitPrepared pasa prepareModule=false y omite la verificación y configuración locales de emitImpl; emit las ejecuta. Es un contrato de preparación previa, no prueba de un defecto alcanzable. Seguir cada llamador y toda mutación entre verificación, optimización y emisión. LK-701.

### LKF-439 — Límite local de tamaño en reserva de cadenas LLVM OOP

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** límite visible en fuente; alcance por rutas y contrato público pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmAggregatesAndStrings.cpp:455–484, :487–505 y :630–690; SHA-256 f78ad27155513df5fa7a9c3d908e0d1fd58f17e45f5765882979bd5d2458622c. allocateNativeString emite una guarda unsigned que exige length <= 0x7fffffff, con trap AllocationSizeOverflow si no se cumple; después reserva length + 9 bytes para cabecera y terminador. Esta función se usa en las rutas revisadas de concatenación, substring y FromCharArray. El límite se expresa sobre longitud en bytes para este layout; no se ha acreditado que alcance a todos los productores, literales o compiladores. Contrastar especificación, representación UTF-8 y demás rutas antes de proponer cambios o diagnosticar un programa. No se ejecutaron casos de frontera. **Seguimiento:** LK-702.

### LKF-440 — Terminación de argumentos en llamadas dinámicas LLVM

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** diferencia local observada; alcanzabilidad y garantías previas pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmCalls.cpp:597–628 y :966–1019; SHA-256 5626a3849623f4c33a4ae3e1cd8a51f597c7961e1ff09df29144a5cac812bdca. collectCallArguments comprueba terminated() después de bajar cada argumento y devuelve el valor terminal; prepareDynamicCallArguments baja argumentos y continúa preparando tipos y copias sin esa comprobación local. Lo usan rutas virtual y de interfaz (:1022–1051 y :1188–1232). Contrastar si MIR o el llamador impiden argumentos que terminen el bloque y si otra capa los maneja; no se ha demostrado IR inválido ni un fallo alcanzable. **Seguimiento:** LK-703.

### LKF-441 — Orden local de Dispose y liberación en using propietario

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** orden local observado; semántica de excepción y rutas externas pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmCleanup.cpp:450–531 y :285–310; SHA-256 e1f892d7dcd7e0a9fac27e10bb1c63207fe2d0602a79227f1bf7f3771b13c438. emitUsingDispose emite Dispose potencialmente excepcional antes de limpiar el slot propietario y llamar a la entrada de destrucción en la continuación normal. En este fragmento no se observa un finally propio alrededor de Dispose que garantice esa destrucción si la llamada transfiere control por unwind. Es necesario seguir la cadena de cleanup externa, contrato de Dispose y prohibiciones de excepciones antes de concluir si hay fuga alcanzable o comportamiento incorrecto; no se ejecutó el escenario. **Seguimiento:** LK-358 y LK-307.

### LKF-442 — Reserva temporal de match en el bloque de evaluación

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** emisión local observada; ubicación final y consecuencia pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmConditionalsAndNullAware.cpp:36–53; SHA-256 1b01722454bdb596503b8912cb31f4c0cda22959f1f26b38cdb8e1987728c4da. Cuando el valor gobernante no es referencia, lowerMatchGoverningValue emite LLVMBuildAlloca con el builder actual y almacena el valor; en ese fragmento no la sitúa explícitamente en el bloque de entrada. Si se alcanza desde un bucle y la reserva permanece dentro del bucle tras las transformaciones, puede repetirse durante la activación y crecer el uso de pila. Comprobar constructos que alcanzan esta ruta, optimización, generación de máquina y posibles restauraciones antes de afirmar desbordamiento o defecto reproducido. No se ejecutaron pruebas. **Seguimiento:** LK-704.

### LKF-443 — Guarda de suma en concatenación condicional

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** diferencia local observada; invariantes de longitud pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmConditionalsAndNullAware.cpp:782–805, SHA-256 1b01722454bdb596503b8912cb31f4c0cda22959f1f26b38cdb8e1987728c4da; contraste OopLlvmAggregatesAndStrings.cpp:455–468 y :487–504, SHA-256 f78ad27155513df5fa7a9c3d908e0d1fd58f17e45f5765882979bd5d2458622c. La ruta condicional suma longitudes y pasa el resultado a allocateNativeString sin la comprobación explícita de desbordamiento de suma que aparece en la concatenación ordinaria; la reserva sí comprueba el total resultante contra 0x7fffffff. Determinar si todas las longitudes de entrada están acotadas de forma que la suma i64 no pueda desbordarse, incluidas fronteras externas y literales; una cabecera corrupta no demuestra por sí sola un programa válido afectado. No se ha demostrado fallo alcanzable ni se ejecutaron pruebas. **Seguimiento:** LK-702.

### LKF-444 — Escritura condicional de elemento y cleanup de valor previo

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** emisión local observada; tipos admitidos y obligaciones externas pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmConditionalsAndNullAware.cpp:970–1031; SHA-256 1b01722454bdb596503b8912cb31f4c0cda22959f1f26b38cdb8e1987728c4da. Tras comprobar el índice, finishNullConditionalArrayStore emite un store directo; en este fragmento no se observa destrucción del elemento anterior ni actualización explícita de liveness. Determinar si parser, sema y MIR permiten esta operación sobre slots propietarios o valores con drop, y si otra ruta ya realiza las obligaciones; no se ha demostrado fuga ni omisión alcanzable. Contrastar también con LKF-373/LKF-377 sobre el alcance de índices admitidos. **Seguimiento:** LK-705.

### LKF-445 — Alineación de escritura al inicializar campos de agregado

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** atributo local observado; layout y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmConstruction.cpp:298–339; SHA-256 ff9cdf7dddcfd1034aabb8572e520cbc93eee3146495e95ba9dd14bc3fcf82a9. initializeAggregateField calcula la dirección mediante el desplazamiento del campo y en la ruta ordinaria aplica al store structAlignmentFor(child->typeKey) cuando supera uno; en el fragmento no se ajusta explícitamente por alineación del contenedor y offset. Contrastar qué devuelve ese helper para cada tipo, cómo se construyen los offsets y si un agregado empaquetado con campo menos alineado alcanza esta ruta. Una alineación LLVM mayor que la garantizada por la dirección sería problemática, pero no se ha demostrado un caso válido ni un fallo. **Seguimiento:** LK-698.

**Contraste parcial de LKF-107:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmConstruction.cpp:443–577 y :619–715; SHA-256 ff9cdf7dddcfd1034aabb8572e520cbc93eee3146495e95ba9dd14bc3fcf82a9. La construcción ordinaria reserva y llama al inicializador mediante buildPotentiallyThrowingCall; en ese fragmento no instala una limpieza local específica de la reserva. La construcción placement con metadatos de asignador personalizado sí crea una limpieza de fallo y la usa como destino de unwind del constructor. Investigar cleanup externo, inicializadores posteriores, construcción parcial y obligaciones de cada ruta antes de concluir que hay fuga. LK-362 y LK-365.

### LKF-446 — Terminación durante limpieza de excepción no coincidente

**Componente:** compilador NativeSharp, lowering de excepciones LLVM OOP. **Estado:** secuencia local observada; semántica y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmControlFlow.cpp:200–218 y :447–468; SHA-256 b0109d93dcfed411a6f69ca1092e3ac96b3dc65f5d24524740c4bc825f6c7e36. emitUnmatchedTryCatch recorre limpiezas activas y luego emite el relanzamiento y unreachable; en ese bucle no comprueba localmente terminated() tras cada cleanup, a diferencia de otras rutas de limpieza. Verificar qué acciones o finally pueden terminar el bloque, restricciones semánticas y cambios de inserción realizados por sus generadores; no se ha demostrado IR inválido ni fallo alcanzable. No se ejecutaron pruebas. **Seguimiento:** LK-307.

**Contraste NSC-585 de LKF-446:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmControlFlow.cpp:780–795; SHA-256 b0109d93dcfed411a6f69ca1092e3ac96b3dc65f5d24524740c4bc825f6c7e36. La limpieza al salir de for comprueba terminated() antes del bucle de acciones, pero no repite esa comprobación tras cada acción en el fragmento. Determinar si más de una acción puede quedar activa y si una de ellas puede terminar el bloque; no se demuestra fallo alcanzable. LK-307.

### LKF-447 — Resultado de reserva de frame de corrutina sin guarda local emitida

**Componente:** compilador NativeSharp, lowering LLVM de corrutinas. **Estado:** ruta local observada; contrato de asignador y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmCoroutines.cpp:117–170; SHA-256 8307bcb5c79197bee2920910dd49121a53a34bb204c3025e3358a2a95021f235. Cuando falta un frame y asyncFrameSize es distinto de cero, ensureCoroutineFrame emite una llamada al asignador y comprueba en C++ si el LLVMValueRef de la instrucción es nulo; después emite el store del resultado y la inicialización del frame. No aparece en ese fragmento una comparación emitida del puntero devuelto en ejecución antes de usarlo. Seguir el contrato real del asignador, las rutas que llegan aquí y posibles guardas o terminación internas: si pudiera retornar null, la inicialización podría acceder a una dirección nula. No se ha reproducido ni demostrado que el asignador retorne null en esta configuración. **Seguimiento:** LK-706.

### LKF-448 — Espera activa de inicialización estática local

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** comportamiento local observado; costo y bloqueo pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmStaticLocals.cpp:38–71; SHA-256 dd0b7884c3976fe0461bee8468b32fd1230ba678eb6906d8376a955574e0ac19. Si otro hilo inicializa el estático, el código emitido repite cargas atómicas mientras el estado es 1; en ese bucle no aparece espera o cesión. La recursión identificada como mismo hilo toma otra rama. Investigar trabajo del inicializador, dependencias entre hilos y cambios de estado antes de concluir bloqueo permanente o costo concreto. No se ejecutaron pruebas. **Seguimiento:** LK-198.

### LKF-449 — Inicializador terminal de using y store posterior

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** comprobación local pendiente; alcanzabilidad no demostrada. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmStructuredExits.cpp:259–281; SHA-256 98bfc2ddb654636b3ad4e707c9111a69a03438657f50d765aef66d903bc85e57. lowerUsingRegion evalúa el inicializador, verifica el LLVMValueRef y emite store antes de activar la región de cleanup; en ese tramo no comprueba terminated() entre evaluación y store. Investigar si sintaxis/MIR admiten una expresión terminal y si alguna ruta previa la maneja; no se ha demostrado IR inválido ni fallo. **Seguimiento:** LK-703 y LK-358.

**Contraste paralelo de LKF-445:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:555–558; SHA-256 73662e8fdf82706cd897f2a241ba3add393ef18c8cbbe24f441aa60ce21f4c5d. structAlignmentFor retorna la alineación del layout de struct si existe y 1 si no se encuentra. Por tanto no se debe asumir que aplica alineación natural mayor a uno a escalares sin layout. Queda por contrastar si un struct anidado en campo empaquetado alcanza la escritura con menor alineación efectiva. LK-698.

### LKF-450 — Offset de datos de arreglo en metadatos debug

**Componente:** compilador NativeSharp, emisión LLVM de información debug. **Estado:** diferencia local observada; tipos alcanzables y PDB final pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmDebugInfo.cpp:650–716, SHA-256 ee300f44b44fa80b05b845cc34672ef0b55dc5ec368ac3d3169270b16461561e; A:/nsc-devel/src/LLVM/Lowering/OopLlvmArraysAndStores.cpp:78–84 y :1318–1326, SHA-256 fec0429337961ff15509ea89fe7ecfe25f5a04967e6a6a17ea59da3325be1e40. debugHeaderReference asigna a Data el offset de la cabecera sin redondear por alineación del elemento; las rutas de acceso y reserva citadas redondean la cabecera a esa alineación. Si el lenguaje admite un elemento cuya alineación requiere padding adicional en esta representación, los metadatos podrían señalar un offset distinto. Verificar tipos/rank alcanzables, transformaciones CodeView/PDB y consumidor antes de afirmar error visible; no se ejecutaron pruebas. **Seguimiento:** LK-707.

### LKF-451 — Mapa de tipos CodeView por módulo y contribución

**Componente:** NSLink, productor PDB. **Estado:** comportamiento local observado; validez de entrada y consecuencias pendientes. **Evidencia:** A:/nslink-devel/src/pdb/CodeViewDatabase.cpp:396–435 y :446–540; SHA-256 40c2eceb57b57bf95b5afc6bb93321a1a730bb69f475f58df7d1ae39dca775db. Al procesar cada contribución .debug$T se limpia y reemplaza el mapa de tipos del módulo; las contribuciones .debug$S se procesan después. Investigar si un módulo puede tener varias contribuciones válidas con mapas distintos y cuál es la asociación requerida antes de afirmar remapeo incorrecto. No se ejecutaron pruebas. **Seguimiento:** LK-708.

### LKF-452 — Callbacks de asignador bajo wrappers nounwind

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** emisión local observada; contratos y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp:50–83 y :230–252; SHA-256 73662e8fdf82706cd897f2a241ba3add393ef18c8cbbe24f441aa60ce21f4c5d. El wrapper de sidecar llama al destructor y thunk antes de liberar sidecar y se marca nounwind; el thunk invoca Deallocate y también se marca nounwind. Contrastar si destructores y Deallocate tienen garantizado no lanzar, y el comportamiento de unwind y cleanup si esa precondición no se cumple. No se ha demostrado una ruta válida que lance ni una fuga. **Seguimiento:** LK-365.

### LKF-453 — Ancho de coeficiente y divisor en conversiones desde decimal

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** operaciones locales observadas; dominio y consecuencia pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmFieldsAndConversions.cpp:358–409 y :432–456; SHA-256 dc4414499f28b99df91fa8c9b7d4676a240ffc449eba8b83b4a7d8571cebbf4f. El comentario describe coeficiente de 96 bits y escala 0–28, pero las rutas a entero y flotante enmascaran 64 bits y usan divisor de 64 bits; 10^20 excede ese rango. Contrastar representación y valores admitidos, guardas anteriores y semántica de conversión antes de afirmar un fallo alcanzable. No se ejecutaron pruebas. **Seguimiento:** LK-679.

### LKF-454 — Rango y redondeo al convertir flotante a decimal

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** operaciones locales observadas; contrato y dominio pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmFieldsAndConversions.cpp:459–501; misma huella de LKF-453. La ruta rechaza NaN e infinito de entrada, multiplica el valor absoluto por 10^6 y emite FPToSI a i64 sin guarda visible en este fragmento para el valor escalado fuera de rango; el comentario habla de redondeo sin operación explícita de redondeo en el fragmento. Contrastar semántica del lenguaje, dominio alcanzable y controles previos; no se ha reproducido efecto ni se infiere corrupción de memoria. **Seguimiento:** LK-679.

### LKF-455 — Alcance del oráculo de breakpoint PDB

**Componente:** validación de NSLink, no compilador. **Estado:** inspección estática del gate; pruebas no ejecutadas. **Evidencia:** A:/nslink-devel/tests/pdb/WinDbgBreakpointGate.cpp:61–62 y :85–164; SHA-256 afbb3f2e320e2aa6d8e0f572e41faaa2a097fe28f932f64de206180fa617219e. El gate busca líneas y comprueba parada y cercanía de IP; la comparación de sufijo usa una ruta construida por el propio gate con el nombre de archivo y no consulta allí resolución inversa IP→fuente ni checksum. Esto limita lo que esa comparación demuestra; contrastar nombres homónimos, causa de parada y demás gates antes de extrapolar. No se ha demostrado fallo del depurador. **Seguimiento:** LK-169/LK-424.

### LKF-456 — Campos sin delimitación en clave de caché de enlace

**Componente:** NSLink, caché de artefactos. **Estado:** colisión estructural de serialización observada; alcanzabilidad y efecto pendientes. **Evidencia:** A:/nslink-devel/src/driver/LinkArtifactCache.cpp:15–20 y :47–68, SHA-256 01e9a1e83235c37d47c1d42802200fc980418380298cabbc634c83c1750ec82c; A:/nslink-devel/src/binary/Sha256.h:18–21, SHA-256 a3071c3fff46daec731bb7ae56dd4d391cb8e263697f6f68c368eef5436c1ae2. addField pasa el literal de un NUL como string_view construido desde C-string, de longitud cero; por ello el separador no aporta bytes. Con el resto de campos iguales, la lista de exports [aexportb] y [a,b] serializa el mismo texto de campos export. Determinar si ambas solicitudes son válidas, si pueden compartir raíz de caché y qué verifica la restauración; la ambigüedad antecede al hash, no es una colisión criptográfica. No se ha reproducido una reutilización incorrecta. **Seguimiento:** LK-709.


### LKF-457 — Profundidad del recorrido de reservas MIR

**Componente:** compilador NativeSharp, lowering LLVM OOP. **Estado:** recursión observada; límite efectivo y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmFunctionDefinition.cpp:491–510, SHA-256 f6f43e0451649e6323a8bdd608f1f7b0d363173672b8d9b3d2e338ca9e60c0c3. collectLocalAllocas recorre recursivamente los hijos MIR sin un límite local visible; consumo de pila depende de la profundidad alcanzable. Contrastar límites previos, forma del MIR y plataforma; no se ha probado que exista una entrada aceptada que agote la pila. **Seguimiento:** LK-710.


### LKF-458 — Lectura de archivo frente a cambios concurrentes

**Componente:** NSLink, lectura binaria. **Estado:** patrón de lectura observado; impacto pendiente. **Evidencia:** A:/nslink-devel/src/binary/ByteBuffer.cpp:21–64, SHA-256 ed4bdf2e37e28ea9009e983bf3f708776b31c59062ed54593330fff0f9727668. Consulta tamaño antes de abrir y leer esa cantidad; detecta lectura corta, pero en este fragmento no verifica crecimiento o sustitución con el mismo tamaño ni rango del cast a streamsize. Contrastar contratos de entrada, plataforma y protección concurrente; no se ha reproducido lectura incorrecta. **Seguimiento:** LK-711.

### LKF-459 — Alcance ARM en copia de Dr. Memory

**Componente:** herramienta de referencia, no NativeSharp. **Estado:** limitación local de referencia observada. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/fastpath_arm.c:46–63 y :72–86, SHA-256 29697cd9514a1e56c8fa0aa48d3f7e8ff1a48b6c1225e8393cef00ab6da0c9d6. Las rutas indicadas contienen ASSERT_NOT_IMPLEMENTED y retornos de reserva; no extrapolar a otras versiones, rutas ni al soporte ARM de NativeSharp. **Seguimiento:** LK-712.


### LKF-460 — Acumulación de ordinales en ensamblador en línea

**Componente:** compilador NativeSharp. **Estado:** cálculo observado; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmInlineAssembly.cpp:146–183, SHA-256 795961f5aa1dd994c2acf1aa655ab0e98095ff77ccf6dde1a779ecf4451ff01d. El índice decimal de un placeholder se acumula en size_t sin guarda local de overflow antes de comprobarlo contra operandos; si llegan suficientes dígitos, puede envolver. Contrastar validaciones y longitud admisible antes de afirmar una entrada aceptada o fallo. **Seguimiento:** LK-713.

### LKF-461 — Emisión opcional y deduplicación temprana de variables debug

**Componente:** compilador NativeSharp. **Estado:** rutas locales observadas; alcanzabilidad y contrato pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/Support/DebugInfoEmission.cpp:193–209, :245–261, :317–344, :391–427 y :448–497, SHA-256 5a3096ada5808585c484b94731f12e22ea9dd127f66935ecc173f81c8d6f831d. Varias rutas retornan éxito sin emitir información cuando faltan datos; local y catch registran ordinal antes de algunas comprobaciones, lo que impediría reintentar localmente con ese ordinal. Contrastar invariantes, diagnósticos y necesidad de reintento; no se ha demostrado PDB incompleto para entrada válida. **Seguimiento:** LK-714.

### LKF-463 — Representación y desplazamientos de literales decimal

**Componente:** compilador NativeSharp. **Estado:** tipos y límite locales observados; alcanzabilidad y efectos pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp:41–63, SHA-256 ab2c33b2c8fec3c094847155b18801446c77f06e197cf136690892feaec88697; Support/LlvmTypeMapping.cpp:77–81, SHA-256 fdc07dbeb87fe82662708d26ff477bf5a019f0e45e8d4071136da44175e727b5; Support/NumericHelpers.cpp:57–61 y :88–96, SHA-256 773980314ffb04a7070520f54cf839b8b1beab47235b29ab55d6acedafe91abd. Decimal se mapea a i128 pero el shift de escala y signo se construye con constante de desplazamiento i32; contrastar contrato LLVM e IR generado. La ruta usa coeficiente uint64 y puede rechazar valores mayores pese a describirse almacenamiento de 96 bits; contrastar dominio admitido y lowering alternativo. No se ha reproducido error ni ejecutado verificador LLVM. **Seguimiento:** LK-679.


### LKF-462 — Alias débil hacia ImageBase sintético en NSLink

**Componente:** NSLink. **Estado:** hipótesis condicionada, no defecto demostrado. **Evidencia:** A:/nslink-devel/src/link/GlobalSymbolTable.cpp:265–275, SHA-256 11cdc1750c999d4341b25b092584df9895eee4a1fa59c7bc8d93e515d03a93b9; contraste parcial src/link/RelocationEngine.cpp:150–210, SHA-256 ea46627347fab5fa2daf4c67bcb9b481f64ddc2c00dbec9123a548f345ddba55; src/link/DebugSectionPolicy.cpp:130–170, SHA-256 44449bf37cd3af8bf8cd85b96a3e53b0c3ca7e37604e1f640d995e44fb4003ed. La copia de fallback no copia localmente la marca syntheticImageBase que otros consumidores distinguen. Comprobar si un alias válido puede alcanzar esa ruta y sus efectos antes de concluir que existe fallo. **Seguimiento:** LK-715.


### LKF-464 — Condición contradictoria para lectura de parámetro capturado

**Componente:** compilador NativeSharp, corrutinas. **Estado:** condición local observada; necesidad y alcance pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp:104–151, SHA-256 ab2c33b2c8fec3c094847155b18801446c77f06e197cf136690892feaec88697. Dentro de una rama que exige node->typeKey == typeKey, se evalúa typeKey.ends_with("*") && !node->typeKey.ends_with("*"); ambas comparaciones de sufijo no pueden diferir para cadenas iguales, de modo que la desreferencia comentada no se alcanza por esa ruta. Contrastar productor, convención de tipos y rutas alternativas para determinar si se requiere; no se ha demostrado resultado incorrecto en un programa válido. **Seguimiento:** LK-716.


### LKF-465 — Aritmética y comparación del carrier decimal

**Componente:** compilador NativeSharp, lowering LLVM. **Estado:** operaciones locales observadas; dominio y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmOperators.cpp:177–204, :291–312, :419–503 y :506–555; SHA-256 88f12e02f2d8fc6ae98d2eb453f03f9ae21a3de37775d81c6d307b11b7fcc188. La ruta opera coeficientes con máscara/lanes de 64 bits aunque describe 96; división y resto usan divisor truncado sin guarda local visible; la escala derecha en esas rutas se extrae sin la máscara de seis bits aplicada a la izquierda. La comparación del carrier usa ICmp sin normalización visible en este fragmento. Contrastar productores, valores admitidos y reescrituras previas; no hay fallo reproducido. Complementa LKF-453 y LKF-463. **Seguimiento:** LK-679.


### LKF-466 — Rango de asignación COMMON en layout

**Componente:** NSLink. **Estado:** conversión local observada; límites previos pendientes. **Evidencia:** A:/nslink-devel/src/link/SectionLayout.cpp:201–227, SHA-256 cc7d69ad6912b8dad250a0243a5638c2226dd551f8a9784e9ae6f7babe733bbe. La colocación COMMON convierte resultados a 32 bits sin una comprobación local explícita análoga a la de contribuciones ordinarias. Contrastar límites garantizados, sumas y objetos válidos o malformados antes de concluir desbordamiento alcanzable. **Seguimiento:** LK-717.

### LKF-467 — Controles de medición y escape en telemetría NSLink

**Componente:** NSLink. **Estado:** implementación local observada; entradas y efectos pendientes. **Evidencia:** A:/nslink-devel/src/link/LinkTelemetry.cpp:21–48 y :239–260, SHA-256 6bf9080f9c45a099385a63f97f470dc181e07252409a5322b92953ce8beabcf9. Al fallar medición de memoria retorna ceros; la función de escape JSON cubre comillas, barra inversa, LF, CR y tab, sin escapar localmente otros controles. Determinar si los nombres admiten tales caracteres y cómo se interpretan los ceros; no se ha reproducido JSON inválido. **Seguimiento:** LK-717.


### LKF-468 — Offset cero ambiguo al resolver campos future

**Componente:** compilador NativeSharp, corrutinas. **Estado:** fallback local observado; invariantes y alcance pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp:496–525, SHA-256 ab2c33b2c8fec3c094847155b18801446c77f06e197cf136690892feaec88697. futureFieldOffset devuelve cero si no encuentra layout o miembro, indistinguible de un offset legítimo cero; resolveAwaitPollTarget usa ese resultado para cargar pollFn. Contrastar si el frontend garantiza layout y miembro válidos en todas las rutas y si hay otras comprobaciones; no se ha demostrado una entrada válida que alcance el fallback o produzca una llamada incorrecta. **Seguimiento:** LK-718.


### LKF-469 — Estado y limpieza en fallo de inicialización estática

**Componente:** compilador NativeSharp. **Estado:** hipótesis condicionadas, no defectos reproducidos. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmStaticState.cpp:403–463 y :466–560, SHA-256 dbaf3ef689c8fe9eca75d2b89b09f0ef9f4a1e8cec9bb2a912389fd896667a80. Hay espera activa mientras el estado indique inicialización; investigar progreso y reentrada, sin equipararla a ciclo infinito. El fragmento emite store tras bajar una expresión sin comprobar localmente terminador y la ruta de fallo no muestra limpieza de todos los campos anteriores. Contrastar contratos de lowerExpression, inicialización global y destrucción antes de afirmar IR inválido o fuga. El inicializador decimal pasa por double y coeficiente uint64; ver LKF-463. **Seguimiento:** LK-719.


### LKF-470 — Cobertura parcial de ancho en prueba de unwind

**Componente:** pruebas de NSLink, no compilador. **Estado:** límite de aserción observado; sin ejecución. **Evidencia:** A:/nslink-devel/tests/coff/CorpusTests.cpp:489–525, SHA-256 e6ce2ccdc147fc0f82dbb15de6a33ba0e9b49979feaaa46681fee9b1fcb14581. La prueba compara data[0] y data[4] para valores descritos como de 32 bits, por lo que esas aserciones no comprueban por sí solas los cuatro bytes de cada valor. Contrastar fixture y resto de cobertura; no es prueba de defecto del producto. **Seguimiento:** LK-720.


### LKF-471 — Valor de error de await para tipos de resultado

**Componente:** compilador NativeSharp, corrutinas. **Estado:** operación local observada; alcance y contrato pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp:870–936, SHA-256 ab2c33b2c8fec3c094847155b18801446c77f06e197cf136690892feaec88697. En el camino de error sin transferencia de excepción y resultado no void se construye un LLVMConstInt de -1 con resultType, cuyo tipo depende de la expresión; contrastar si siempre es entero en esa ruta o si pueden llegar punteros, flotantes o agregados. Verificar semántica del error y restricciones previas antes de afirmar IR inválido o resultado incorrecto. Sin pruebas. **Seguimiento:** LK-721.


### LKF-472 — Cobertura y contenido de la traza de reservas debug

**Componente:** compilador NativeSharp, wrappers de runtime. **Estado:** alcance local observado; otras rutas pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmRuntimeDeclarations.cpp:74–138, SHA-256 77bf73c22c1f0c59cbcd29724d4ecb276642cf52a4bc6f91f848bfffd52033b0. Los wrappers consultados requieren la bandera de instrumentación y debug Full y escriben mensajes constantes ALLOCATE y FREE-BEGIN sin dirección ni tamaño. En el fragmento ALLOCATE sigue a la llamada sin comprobar resultado y FREE-BEGIN precede a free; no indican necesariamente reserva exitosa o liberación completada. Investigar otras rutas y contratos antes de generalizar cobertura. **Seguimiento:** LK-722.


### LKF-473 — Sincronización de enlace dinámico y drop de iterator

**Componente:** compilador NativeSharp. **Estado:** patrones locales observados; concurrencia e invariantes pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmThunksAndWrappers.cpp:467–551 y :826–913, SHA-256 d4091283b66390f80612ec6010723e2164c8171047b9e0721f23f0c275c27df9. La caché de binding no TLS usa carga/store ordinarios sin atómico visible; investigar exclusión externa y primeras llamadas concurrentes. El drop de iterator llama un puntero sin guarda nula visible, a diferencia del future; contrastar estados válidos y layout antes de afirmar carrera o llamada inválida. **Seguimiento:** LK-723.


### LKF-474 — Opciones de optimización de NSLink

**Componente:** driver NSLink. **Estado:** comportamiento local observado; contrato de precedencia pendiente. **Evidencia:** A:/nslink-devel/src/driver/CompatibleDriver.cpp:638–653, SHA-256 7b8391dfed8ad7b95690ebd086f42f760de9e1cbe317ce9c61bed8ecbcbdbf79. /opt:ref e /opt:icf activan banderas; /opt:noref y /opt:noicf no las modifican en esta función, incluso si ya se activaron antes. Contrastar contrato deseado y contexto antes de concluir defecto. **Seguimiento:** LK-724.

### LKF-475 — Alcance de /libpath y validación de artefactos

**Componente:** driver NSLink. **Estado:** rama y validación local observadas; otras etapas pendientes. **Evidencia:** A:/nslink-devel/src/driver/CompatibleDriver.cpp:75–136 y :589–605, SHA-256 7b8391dfed8ad7b95690ebd086f42f760de9e1cbe317ce9c61bed8ecbcbdbf79. /libpath se acepta sin acción en esa rama; seguir resolución de bibliotecas para determinar impacto. validateArtifact verifica firmas iniciales, no estructura integral ni identidad cruzada PE/PDB en esta función; no se descarta validación en otra etapa. **Seguimiento:** LK-724.


### LKF-476 — Frame de generador y fallo de reserva

**Componente:** compilador NativeSharp. **Estado:** guarda local observada; contrato de OOM pendiente. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmStatements.cpp:169–210, SHA-256 f7b8de299bf95cb646d395815271dfdd75c2cdedbe2be69c8bda377300fd2038. Se comprueba el objeto LLVM de la llamada, no el puntero retornado en ejecución, antes de usarlo para inicializar el frame; contrastar contrato del asignador y guardas externas. La ruta alternativa de yield :510–514 conserva una dirección de alloca en self; investigar alcanzabilidad y vida antes de afirmar uso inválido. **Seguimiento:** LK-725.

### LKF-477 — Aristas y raíces de análisis de funciones

**Componente:** compilador NativeSharp. **Estado:** diferencias locales observadas; cobertura por otras rutas pendiente. **Evidencia:** A:/nsc-devel/src/LLVM/ProjectOopFunctionAnalysis.cpp:50–100, :205–249 y :311–319, SHA-256 b161c01edad10f175df14311e53f4ffaa3d1e059fbd3ba7ac9adbbd1d652205b. La consulta de referencias generales atiende disposeContractMemberOrdinal, no visible explícitamente en escaneo de alcanzabilidad; el bucle de definiciones no recorre allí inicializadores de campo, aunque otra consulta los considera para declaraciones. Contrastar invariantes y rutas alternativas; no se ha demostrado omisión de código. **Seguimiento:** LK-726.


### LKF-478 — Ejemplos NSASM: strlen y registros preservados

**Componente:** ejemplos de NSASM, no compilador. **Estado:** hipótesis estáticas; ABI y ejecución pendientes. **Evidencia:** A:/nsasm-devel/examples/avanzados/cadenas.asm:40–50 y :74–78, SHA-256 09b6b5daccb1131301c58d8150024949d837d965c2440e393a98593b812c9b16; consola.asm:127–156, SHA-256 bf8f1d88b559c72565e1f63dc32c0438646b50e1157022ae388608b608ef6b45; vectores.asm:102–189, SHA-256 77c828826a1679b10527a9024514c7daa82f0566688326028995d8addda8f16c. En strlen ECX sobrescribe RCX usado como base posteriormente; las otras rutinas modifican RDI o XMM6/XMM7 sin restauración visible. Verificar ruta y convención antes de concluir error ejecutable. **Seguimiento:** LK-727.

### LKF-479 — Alcance del gate de ejemplos NSASM

**Componente:** script de validación NSASM. **Estado:** flujo estático observado, sin ejecución. **Evidencia:** A:/nsasm-devel/build_and_test.py:935–1039 y :1168–1274, SHA-256 24673345c163ae2ac13f5734b198efd96b3fa58f9c0e82412e202a8f5a3898bc. Ejemplos se ensamblan, pero no se ejecutan individualmente; consola es condicional y se busca texto sin comprobar allí código de salida; cero ejemplos y consola ausente pueden quedar sin error en ramas citadas. Contrastar fixtures, entorno y contrato. **Seguimiento:** LK-727.


### LKF-480 — Anchura en corrección de división firmada

**Componente:** compilador NativeSharp. **Estado:** cálculo local observado; anchos alcanzables pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/SignedDivisionCorrection.cpp:18–36, SHA-256 20480a2be205f06f4853753733d5a27836286a88b5db38fba3668fc7ded6dc88; cabecera SignedDivisionCorrection.h, SHA-256 c8083d2cc595e30f62c3c7d8ff0481e279cdd1330e770998d2f53ebb02a4a8bb. El helper usa bit 63 para width >= 64 y calcula width-1 para anchos menores; para widths mayores que 64 no representa el mínimo de todo ese ancho y width cero requiere verificar precondición. Investigar si los consumidores admiten estos anchos, la prueba divisorProvenNonZero y guardas previas; no se ha demostrado defecto alcanzable. **Seguimiento:** LK-728.


### LKF-481 — Publicación secuencial y restauración de artefactos NSLink

**Componente:** NSLink. **Estado:** comportamiento local observado; fallos y lectores pendientes. **Evidencia:** A:/nslink-devel/src/binary/AtomicFile.cpp:109–164 y :173–241, SHA-256 044e3710a89fc3d8250930cda8fd80804642cc076ae078ed49276522f44eccdf. Los locks coordinan publicadores Windows, pero respaldos y publicación de varios archivos ocurren secuencialmente; un lector externo sin esos locks no tiene en este método una vista instantánea del conjunto. La restauración intenta recuperar respaldos e ignora errores de reemplazo. Contrastar lector, sistema de archivos y recuperación; no se reprodujo fallo. **Seguimiento:** LK-729.

### LKF-482 — Campos COFF de alineación y relocación extendida

**Componente:** lector COFF de NSLink. **Estado:** alcance de lectura observado; validez y resultado pendientes. **Evidencia:** A:/nslink-devel/src/coff/CoffObjectReader.cpp:131–235 y :396–469, SHA-256 1382a00e0440da1344b60f7961baac68f6944e731a63d80d7fd66fd827334587; CoffSectionHeader.cpp:46–111, SHA-256 788755c5a5f13f9e341ac7c78b47af50d78cf1d288114dd7aadb1581b42efa6a. En estas rutas no se observa diagnóstico explícito para alineación codificada 15 y la conversión consultada la limita; la cabecera expone relocación extendida, sin rama visible correspondiente en el bucle del lector. Seguir consumidores y formato, analizar casos válidos o alterados antes de concluir error. **Seguimiento:** LK-730.


### LKF-483 — Ruta HIR para await con productor suspendible

**Componente:** compilador NativeSharp. **Estado:** rechazo condicional local observado; rutas alternativas pendientes. **Evidencia:** A:/nsc-devel/src/Frontend/HIR/ExpressionStatementMachine/HirInitializationPrimaryFrames.cpp:172–202, SHA-256 0e248bad5da8a303160e7b794dbd4af78b7c419abe89f7c60d5e0234549fdcb8. Para un await cuyo operando es llamada resuelta a miembro y awaitProducerCanSuspend retorna true, esta ruta devuelve error de requerir lowering de máquina de estados. Identificar condiciones, etapa y alternativas antes de generalizarlo a todos los await o suspensión. **Seguimiento:** LK-731.


### LKF-484 — Fuerza de evidencia del reporte de capacidades

**Componente:** reporte de capacidades del runtime NativeSharp. **Estado:** clasificación declarativa observada. **Evidencia:** A:/nsc-devel/src/Runtime/RuntimeCapabilityReport.cpp:14–31, :48–78 y :92–106, SHA-256 837b5d5c5610f3926d27fe90879b214f4dee99d8accb5e752be52b8d9dc8917d. Consulta nombres de símbolos ABI; en memoria compartida y continuaciones, la condición que deja missing vacío no comprueba separadamente ciertos contratos descritos en la rama faltante. El target de salida es literal. Ready aquí no demuestra binario enlazado, funcionalidad o target efectivo; investigar consumidores y versión. **Seguimiento:** LK-732.

### LKF-485 — Alcance de validación de tabla de excepciones PE

**Componente:** NSLink. **Estado:** controles locales observados; etapas posteriores pendientes. **Evidencia:** A:/nslink-devel/src/pe/PeExceptionTable.cpp:26–114, SHA-256 3c5bf6430594c196bf0493625e239d24cb629e4506fc9670e99dd9ae6211b904. Se verifica múltiplo de 12, orden y entradas con inicio duplicado, pero este método no verifica explícitamente rangos Begin/End ni accesibilidad o formato de UnwindInfo. Contrastar resto de pipeline y validez de entrada; no se ha demostrado fallo de unwind. **Seguimiento:** LK-733.


### LKF-486 — Límites y concurrencia de relocaciones NSLink

**Componente:** NSLink, motor de relocaciones. **Estado:** operaciones locales observadas; contratos e invariantes pendientes. **Evidencia:** A:/nslink-devel/src/link/RelocationEngine.cpp:15, :98–109, :256–326 y :376, SHA-256 ea46627347fab5fa2daf4c67bcb9b481f64ddc2c00dbec9123a548f345ddba55. ADDR32NB y SECREL suman valores de 32 bits y ADDR64 suma 64 bits sin comprobación local visible; contrastar semántica del addend, límites de productores y necesidad de overflow. El contador applied_ es static y no atómico: investigar si se permiten pipelines concurrentes en el mismo proceso y sincronización externa. No se reprodujo error ni carrera. **Seguimiento:** LK-734.


### LKF-487 — Límites de intervalos en copia de Dr. Memory

**Componente:** herramienta de referencia, no NativeSharp. **Estado:** restricciones locales documentadas en fuente; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/common/heap.c:810–851 y :915–931, SHA-256 bed13986694c028f9478e34e5804e839b2e267a50040f91cd39e8180fe3c1863. La eliminación parcial supone que el intervalo no abarca varios nodos y la consulta completa indicada no cubre varios nodos contiguos. No extrapolar a otras versiones o a LeakKiller. **Seguimiento:** LK-735.

### LKF-488 — Fixups de base para símbolos absolutos

**Componente:** NSLink. **Estado:** hipótesis condicionada; formato y ruta válida pendientes. **Evidencia:** A:/nslink-devel/src/pe/BaseRelocationBuilder.cpp:24–66, SHA-256 5e8b25cb31621dc72c50d286f659b09f2b700b7a06ef0932efd53a0c68eb5311; RelocationEngine.cpp:181–213, SHA-256 ea46627347fab5fa2daf4c67bcb9b481f64ddc2c00dbec9123a548f345ddba55. collect selecciona fixups por tipo ADDR64/ADDR32 sin consulta visible de definición, mientras el motor distingue símbolos absolutos. Investigar si esos símbolos pueden generar relocaciones válidas y si deben permanecer constantes tras reubicación; no se ha reproducido problema. **Seguimiento:** LK-736.


### LKF-489 — Auxiliares COFF desconocidos sin bytes en representación

**Componente:** NSLink. **Estado:** discrepancia local entre declaración y representación. **Evidencia:** A:/nslink-devel/src/coff/CoffAuxSymbol.h:67–70 y :99–117, SHA-256 a15277ddb9f0cbec4b7832ecfe15c27bf2b0663f1f15144fbeb905c6f7b68a24; CoffAuxSymbol.cpp:53–66, SHA-256 352fc16d81a1cc0d13be1daef39880522c4bf4f6b82d4e03886b5a158f467074. La cabecera afirma conservar bytes crudos para auxiliares Unknown, pero esa ruta avanza sin guardarlos en los campos de la clase. Contrastar dumper y buffer original antes de cuantificar impacto; no se afirma pérdida del archivo fuente. **Seguimiento:** LK-737.

### LKF-490 — Identidad de dirección repetida en referencia Memcheck

**Componente:** copia de referencia Valgrind, no NativeSharp. **Estado:** cautela expresada en fuente, sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/valgrind-3.28.0/memcheck/mc_malloc_wrappers.c:231–269, SHA-256 de23389f4098dc556426e48f953b32e6ccb84579a17a1dec4d8e4fc376de11fb. El código contempla direcciones repetidas entre bloque padre y anotación custom, con advertencia de fragilidad del autor. No generalizar esa observación a otras versiones o a LeakKiller. **Seguimiento:** LK-738.


### LKF-491 — Alcance del volcado textual MIR

**Componente:** compilador NativeSharp, MirDumper específico. **Estado:** límite de salida observado; sin ejecución. **Evidencia:** A:/nsc-devel/src/MIR/MirDumper.cpp:71–218, SHA-256 55fa7ccbec6afea129b1439696556feda5945c5a954b8ec6bbbc71aef38f265f. El volcado de este namespace muestra CFG, operandos, tipos e identificadores locales, pero no imprime allí fuente por instrucción ni identidad de build. No extrapolar a OOP MIR, PDB u otros flujos; no asumir que el texto sea contrato reversible o una grabación. **Seguimiento:** LK-739.


### LKF-492 — Vistas y resultados ambiguos en objetos COFF

**Componente:** NSLink. **Estado:** contratos locales observados; usos y consecuencias pendientes. **Evidencia:** A:/nslink-devel/src/coff/CoffObjectFile.cpp:16–19 y :46–106, SHA-256 bf1445d1ea7ce3cd729cfcb5fa232dba3f57bd340744e9c7d9130913ed364278; CoffObjectFile.h:27–35 y :86–94, SHA-256 f4655317e3077d61f5e2b64dcd01a8e656ff8167f3d1a319834125b6f65f7ecb. setBuffer reemplaza el dueño sin actualizar visiblemente la tabla de cadenas que contiene vista no propietaria; verificar usos y ciclo de vida antes de afirmar invalidación activa. Consultas de tamaño, nombre y relocaciones pueden devolver valor vacío también para índice inválido; contrastar precondiciones de consumidores. **Seguimiento:** LK-740.


### LKF-493 — Alcance del volcado textual SSA

**Componente:** compilador NativeSharp, SsaDumper específico. **Estado:** límite de salida observado; sin ejecución. **Evidencia:** A:/nsc-devel/src/SSA/SsaDumper.cpp:74–237, SHA-256 63df928e68c5be6c5325ccbc8fe17214299df4b40a0f2b9f3c29b3067d5178c0. Muestra bloques, phi, tipos y operaciones, pero no imprime allí fuente por instrucción ni identidad de build. Si falta SsaFunction para una función imprime prototype; contrastar invariantes antes de inferir pérdida. No extrapolar a modelos internos, PDB ni otros flujos. **Seguimiento:** LK-739.


### LKF-494 — Errores y estado parcial al agregar objetos al grafo

**Componente:** NSLink. **Estado:** flujo local observado; contratos de entrada y recuperación pendientes. **Evidencia:** A:/nslink-devel/src/link/LinkGraph.cpp:59–90 y :188–195, SHA-256 5ab118a2cff7e49d0098b7e2feef5921a539b4bac0e57254386620ea241f7829. addObject ignora el resultado de sectionData en la ruta indicada y puede retornar false por COMDAT sin clave tras añadir archivo/secciones/símbolos, sin rollback local visible. Contrastar validación previa y si consumidores descartan el grafo ante fallo; no se ha demostrado corrupción ni fallo de reintento. **Seguimiento:** LK-741.


### LKF-495 — Alcance del volcado SSA optimizado

**Componente:** compilador NativeSharp, OptimizationDumper específico. **Estado:** alcance de salida observado; sin ejecución. **Evidencia:** A:/nsc-devel/src/Passes/OptimizationDumper.cpp:116–268, SHA-256 fc8bac23bf4689bb85503e3a0e7d0af3128f159c1deb220817e32d7e5abc9283. El texto muestra bloques, operaciones y phi, pero no imprime allí fuente por instrucción, identidad de build ni configuración de pasadas. No inferir ausencia de datos en modelos internos u otras herramientas ni equivalencia semántica por etiqueta de optimización. **Seguimiento:** LK-739.


### LKF-496 — Selección EXACT_MATCH y relocaciones COMDAT

**Componente:** NSLink. **Estado:** comportamiento local observado; contrato y efecto pendientes. **Evidencia:** A:/nslink-devel/src/link/ComdatResolver.cpp:13–31 y :90–203, SHA-256 ef0ff03eb309f66963a4a132df0080331a99c5a1eb455a39f18f7f1b77e492f6. EXACT_MATCH usa comparación de tamaño y bytes sin comparar relocaciones en esta función; también procesa la regla de la sección entrante sin comparación visible con la del ganador. Contrastar especificación, productores y reglas mixtas antes de concluir defecto. **Seguimiento:** LK-742.

### LKF-497 — Cobertura de 64 bits en caso de referencia

**Componente:** prueba de referencia Valgrind, no NativeSharp. **Estado:** limitación documentada por autores; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/valgrind-3.28.0/memcheck/tests/pointer-trace.c:24–32, SHA-256 505ea06d30da5e24e4298dd43663a0a9838a5410f5874bcd4447d904a228ecc4. El propio caso advierte que la variante de 64 bits puede mostrar salida semejante sin cubrir adecuadamente el escenario; no extrapolar a otra prueba ni a LeakKiller. **Seguimiento:** LK-743.


### LKF-498 — Propagación de fallo de cleanup de expresión completa

**Componente:** compilador NativeSharp. **Estado:** flujo local observado; propagación y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionMachine.cpp:701–733, SHA-256 b4f369f826f6411553400333e4521bbf740701213ffaf809f49a2e1e79bd1108. Si emitCleanupAction falla en el bucle, la ruta agrega nullptr a values y continúa con los demás cleanups, después agrega el valor original; contrastar cómo el llamador consume la pila y el error, si el fallo es alcanzable con MIR válido y las invariantes de frames. No se ha demostrado compilación incorrecta ni ejecutado reproducción. **Seguimiento:** LK-744.


### LKF-499 — Alcance de comprobaciones phi en verificador MIR

**Componente:** compilador NativeSharp, MirVerifier específico. **Estado:** alcance local observado; validaciones adicionales pendientes. **Evidencia:** A:/nsc-devel/src/MIR/MirVerifier.cpp:120–315, :334–354 y :822–835, SHA-256 0b7d2ea22da5ea79a58653dadf2a7a7bb020bce97a281fd1dcc10514bac65a6a. En esta función los entrantes phi se comprueban por bloque presente y tipo, sin comprobación explícita de predecesores completos o dominancia. Las comprobaciones de forma de alloc/free y bounds tampoco constituyen análisis temporal de propiedad. Investigar contratos y verificadores adicionales; no se ha demostrado IR inválido producido. **Seguimiento:** LK-745.


### LKF-500 — Detección de rutinas de sincronización en referencia

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** stub local observado; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/perturb.c:148–163, SHA-256 ed002c5021849efb26ea4bef5a16d2c15ef89cc1134dfcf814526b384344a27c. La función indicada para identificar rutinas de sincronización retorna false y el comentario indica trabajo pendiente; no extrapolar a otras rutas o versiones. **Seguimiento:** LK-746.


### LKF-501 — Nombres de sección PE limitados a ocho bytes

**Componente:** NSLink, escritura PE. **Estado:** truncamiento observado; impacto y casos válidos pendientes. **Evidencia:** A:/nslink-devel/src/pe/PeImageWriter.cpp:238–243, SHA-256 a68d0d8ae01b1d62f219fc4eb83b6f47336bb45fbc591ba11dc1d255273c7080; src/binary/BinaryWriter.cpp:58–69, SHA-256 4fd093865102e67fd0c12be5b9831bc3917549c00869293a119732d585496d43. El escritor usa campo fijo de ocho bytes y writeFixedString trunca silenciosamente texto más largo. Contrastar validez, nombres admitidos y colisiones de prefijos en layout; no se ha reproducido ambigüedad real. **Seguimiento:** LK-747.


### LKF-502 — Cobertura de campos en identidad MIR y orden de ordinales

**Componente:** compilador NativeSharp, identidad de caché MIR OOP. **Estado:** ausencia local y precondición observadas; identidad global pendiente. **Evidencia:** A:/nsc-devel/src/LLVM/ProjectOopMirCacheIdentity.cpp:18–156 y :519–537, SHA-256 0723803674055d8d7dcdcf7de596bbbad0cbd5f9d1e7aa55734eb64e767b28dd. El archivo no serializa explícitamente allocatorDeallocateMemberOrdinal, asyncFrameSize ni asyncFrameLocals; determinar si derivan de otros datos o se cubren en la clave global y si pueden cambiar independientemente. binary_search presupone ordinales ordenados; revisar contrato y llamadores. No se ha demostrado reutilización incorrecta. **Seguimiento:** LK-748.


### LKF-503 — Límite y estados de lectura de fuente

**Componente:** compilador NativeSharp, SourceManager. **Estado:** límite local observado; otras rutas pendientes. **Evidencia:** A:/nsc-devel/src/Source/SourceManager.cpp:19–64 y :80–128, SHA-256 552713df7dbc419c2dd216a4756b1a51fbfd015e5bcb46307a4b8669e0cb0416; SourceAccessRecording.cpp:9–38, SHA-256 94ded8503c18b58faba75db220419b2dc232d1316284079b3d6337904d24b1dd. loadFile rechaza archivos mayores de 64 MiB y resultados de texto vacíos pueden representar rangos inválidos o contenido vacío; no generalizar ese límite a addBuffer. getFile noexcept llama un registro con inserción potencialmente asignadora: investigar OOM y contrato antes de afirmar fallo. El registro de archivos del compilador no es recording del programa. **Seguimiento:** LK-749.


### LKF-504 — Restauración parcial de tablas en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** FIXME local de referencia; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/instru.c:522–525, SHA-256 3ed3c531e873acec5bb10c9d3959d59c2ff30cd9dd3864bf2c53434c86067077. Un comentario señala que una restauración parcialmente fallida puede dejar tablas inconsistentes; no extrapolar a otras versiones ni a LeakKiller. **Seguimiento:** LK-750.


### LKF-505 — Límites locales de layout fijo de arrays

**Componente:** compilador NativeSharp, TargetDataLayout. **Estado:** contrato de API observado; consumidores pendientes. **Evidencia:** A:/nsc-devel/src/Target/TargetDataLayout.cpp:60–92 y :385–401, SHA-256 3122450527e7f429fbcc78ca3eb53aa2e7b9e8c863895f4b87967e015830a480. layoutArray rechaza count cero en esta API y alignUp puede rechazar un valor extremo aun ya alineado si falla la suma previa. Contrastar representación, rangos y llamadores; no generalizar a arrays dinámicos ni afirmar caso válido fallido. **Seguimiento:** LK-751.


### LKF-506 — Aserción incondicional en caso de prueba sin GC

**Componente:** prueba NSLink. **Estado:** flujo de código observado; resultado pendiente. **Evidencia:** A:/nslink-devel/tests/link/WeakGcTests.cpp:389–408, SHA-256 716673be7dabee500ffabd5880f8ce86f7e4e84f2e9aad2540e27c7ec5b21f55. Una aserción usa OR true y una segunda lectura reutiliza la misma variable de objeto; investigar efecto sobre entrada Start y resultado de prueba. No se ejecutó ni se atribuye defecto al producto. **Seguimiento:** LK-752.

### LKF-507 — Heurísticas de pila en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** límites documentados en código de referencia. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/stack.c:51–115 y :162–253, SHA-256 83ab0c859a1e0639ceb82b1a642ba9eafc276cf70aa367f506266e8a2a888f29. Comentarios y ramas reconocen falsos positivos/negativos al inferir cambio de pila y límites no confiables; una modalidad también puede alterar contenido de pila. No extrapolar a otra versión o a LeakKiller. **Seguimiento:** LK-753.


### LKF-508 — Vida de descripción temporal en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** FIXME local de referencia; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/syscall.c:393–421, SHA-256 bae1a89353550745b9b972783e39719855516b5b0e48d9a850c8eed7eef5e9ed. Un comentario advierte la posible vida insuficiente de una descripción temporal si se retiene en una pila; verificar rutas y versión de esa referencia antes de atribuir un fallo. No extrapolar a LeakKiller. **Seguimiento:** LK-754.


### LKF-509 — Fase de argumentos de llamada estática limitada a ocho bits

**Componente:** compilador NativeSharp, lowering iterativo. **Estado:** conversión local observada; límites previos y alcance pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionMachine.cpp:1146–1171, SHA-256 b4f369f826f6411553400333e4521bbf740701213ffaf809f49a2e1e79bd1108. La fase avanza mediante static_cast<uint8_t>(index + 1) mientras se compara con current->children.size(); si una llamada con 256 o más operandos llega a esta ruta, el contador puede volver a cero y no avanzar. Contrastar límite semántico de argumentos, productor MIR y rutas alternativas antes de concluir ciclo alcanzable. No se ejecutó reproducción. **Seguimiento:** LK-755.


### LKF-510 — Progreso del coordinador de continuaciones

**Componente:** compilador NativeSharp, parser. **Estado:** invariante local observada; rutas alcanzables pendientes. **Evidencia:** A:/nsc-devel/src/Parser/ParserContinuationCoordinator.cpp:42–83 y :85–195, SHA-256 4a26f0a177aca8a3d1162c7ec7bae6d1341414a4830d4407f8828a36177ea9ca. drain espera resultado y usa assert de trabajo no vacío; un dispatch sin publicar ni encolar violaría esa precondición. Existe default que retorna en un dispatch; contrastar enum, productores y tratamiento de errores antes de afirmar ruta alcanzable o falta de progreso. **Seguimiento:** LK-756.


### LKF-511 — Seguimiento de procesos en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** FIXME local; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/syscall_windows.c:524–532, SHA-256 eaba4ae799d17698c98edad428207260b52c594b458f0f9805af987cf17ae234. El código contiene un FIXME sobre seguimiento de creación de procesos; contrastar rutas y versión de la referencia, sin extrapolar a LeakKiller. **Seguimiento:** LK-757.

**Ampliación de LKF-509:** En OopLlvmExpressionMachine.cpp:1255–1297 y :1387–1412, ValueConstruct, AllocateAndConstruct y AggregateInitialize también convierten index + 1 a uint8_t frente a argumentCount o children.size(); investigar límites de aridad y de elementos para cada productor antes de afirmar falta de progreso alcanzable. Misma huella de la entrada principal.


### LKF-512 — Precondición de emisión MIR tras terminador

**Componente:** compilador NativeSharp, MirBuilder. **Estado:** interacción local observada; alcanzabilidad pendiente. **Evidencia:** A:/nsc-devel/src/MIR/MirBuilder.cpp:970–985, SHA-256 1178a071714f72ba22e03462e7583b61e541fe6b629e004090d96d33ad2ded32. emit puede retornar nullptr si no hay bloque o ya terminó; algunos consumidores usan resultado sin comprobar. Investigar ensureWritableBlock y rutas alcanzables antes de afirmar fallo. El modelo conserva SourceRange aun cuando el volcado no lo imprima. **Seguimiento:** LK-758.


### LKF-513 — Terminación de cadenas en índice de biblioteca COFF

**Componente:** NSLink. **Estado:** lectura local observada; formato y casos pendientes. **Evidencia:** A:/nslink-devel/src/archive/CoffArchiveReader.cpp:247–263, :297–327 y :342–357, SHA-256 aa9d09958d5b4d6968b91ed482a24426d94a7dd18f60c705255ae0fcf9e6a31a. Las rutas citadas recorren nombres hasta NUL o fin sin comprobación local explícita de terminador hallado antes de avanzar. Contrastar formato y comportamiento de entradas malformadas, sin afirmar fallo reproducido. **Seguimiento:** LK-759.

### LKF-514 — Restricciones de extracción de bibliotecas NSLink

**Componente:** NSLink. **Estado:** límites locales observados; otras rutas pendientes. **Evidencia:** A:/nslink-devel/src/link/ArchiveResolver.cpp:39–93, :98–151 y :178–187, SHA-256 9220e3c95e19e34e9f73ce039951e28a8082328cd26e30a726f6d9ec2bc6b8c1. La resolución se limita a 1000 rondas y la ruta de short imports rechaza nombre exportado vacío por ordinal. Contrastar cadenas válidas, configuración y otras rutas antes de generalizar. **Seguimiento:** LK-759.

### LKF-515 — Stubs de llamadas macOS en referencia

**Componente:** copia Dr. Memory, no NativeSharp. **Estado:** alcance local observado; rutas genéricas pendientes. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/syscall_macos.c:33–87, SHA-256 cf666d749e91111d815dfdade8c744628012dfc81db9aad033e5267e52609960. Algunas funciones están vacías y el procesador específico indicado devuelve no manejado; no implica ausencia total de soporte de la herramienta ni afecta al compilador NativeSharp. **Seguimiento:** LK-757.


### LKF-516 — Profundidad recursiva en parser y clonación

**Componente:** compilador NativeSharp. **Estado:** recursión local observada; límites y rutas pendientes. **Evidencia:** A:/nsc-devel/src/Parser/Parser.cpp:59–159 y :524–571, SHA-256 e37639d917dd3fbed09287390c428222acb9bbed55a28ff1399844de1f365418. La clonación de tipos/nodos y análisis de agregados anidados emplean recursión cuya profundidad depende de sintaxis alcanzada; contrastar rutas activas y límites previos antes de afirmar agotamiento de pila alcanzable. **Seguimiento:** LK-760.


### LKF-517 — Límite de nombres en short import

**Componente:** NSLink. **Estado:** patrón de lectura observado; contrato y resultado pendientes. **Evidencia:** A:/nslink-devel/src/archive/ShortImportObject.cpp:29–108, SHA-256 ad28e5a9f9c216748216c927779ecc899354bd514f8c0bfc4ee2280db1233518. La lectura de dos cadenas usa SizeOfData como límite en ambas sin descontar visiblemente lo ya consumido; BinaryReader limita al miembro restante. Investigar si bytes adicionales permiten exceder el payload declarado y si el formato lo prohíbe; no se ha reproducido un fallo. **Seguimiento:** LK-761.

**Actualización de LKF-510:** el contraste de ParseContinuationCoordinator.h con el dispatcher identifica case para los 75 WorkKind declarados, sin variantes extra, según paralelo-compilador-36.md. Se descarta la hipótesis de una variante declarada sin case en esa comparación; siguen pendientes progreso y publicación de cada handler, y sincronía con productores.


### LKF-518 — Ambigüedad de líderes y conteos en referencia Memcheck

**Componente:** copia de Valgrind, no NativeSharp. **Estado:** límites documentados en referencia; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/valgrind-3.28.0/memcheck/mc_leakcheck.c:52–88, :1451–1459 y :1883–1902, SHA-256 d15d7fe93d1f28d3afbbde5087eb124db9e92d57eb7ec7645f02319eb29af4db. Los comentarios describen elección arbitraria de líder para ciertos ciclos/referencias compartidas, posibles bytes indirectos en varias vistas y caducidad de una lista tras frees. No extrapolar a LeakKiller ni a otras versiones. **Seguimiento:** LK-762.

**Ampliación LKF-509, stackalloc:** OopLlvmExpressionMachine.cpp:1713–1737 usa el mismo contador uint8 para current->children.size() de extensiones de stackalloc. Contrastar límite de dimensiones y productores antes de considerar alcanzable el wrap. Misma huella registrada en LKF-509.


### LKF-519 — Alcance de exportaciones PE de NSLink

**Componente:** NSLink. **Estado:** interfaz y conversiones locales observadas; otras rutas pendientes. **Evidencia:** A:/nslink-devel/src/pe/PeExportBuilder.cpp:34–143 y :145–208, SHA-256 d1cfc7b0d20bad574d5ad8c8d91ea006b5dbfdcbdc53c85ae17b815db0c14d65. La ruta recibe nombres y genera ordinales relativos; comentarios reservan ordinal explícito, DATA y forwarder para trabajo futuro. El índice se convierte a uint16 sin guarda local visible: contrastar límites de entrada/PE y validaciones previas. No se ha reproducido fallo ni se generaliza a otras rutas. **Seguimiento:** LK-763.


### LKF-520 — Alcance de sombra y registros en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** límites locales documentados en fuente; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/shadow.c:551–655 y :1482–1947, SHA-256 7f85f44df69685d9135aaca04328dc275bf88642d9b7a61ce03c80a4777cd33e. La operación indicada de guardar/restaurar rango no implementa una ruta de modo 4:1; el archivo también señala seguimiento de punto flotante pendiente y ramas SIMD ARM no implementadas. Alcance sujeto a configuración y rutas alternativas de esa copia; no extrapolar a NativeSharp ni a LeakKiller. **Seguimiento:** LK-764.


### LKF-521 — Atributos de EXPORT descartados en directivas COFF

**Componente:** NSLink. **Estado:** pérdida de atributos observada estáticamente; efecto binario pendiente. **Evidencia:** A:/nslink-devel/src/link/DirectiveParser.cpp:137–150, SHA-256 2e9f4ed300574ad0b72f69da9b990323fca78b9ff668b4f7867495e63b7bdb87; LinkPipeline.cpp:287–290, SHA-256 94b94aa3134daff3cfb7833e56615192492f470165a4a46a01c95cfc444c5e61; PeExportBuilder.cpp:34–143, SHA-256 d1cfc7b0d20bad574d5ad8c8d91ea006b5dbfdcbdc53c85ae17b815db0c14d65. En la ruta .drectve el parser conserva el texto anterior a la primera coma y retorna éxito; el pipeline pasa esos nombres al generador, por lo que atributos como ordinal o DATA no se conservan en esa representación. Contrastar efectos por atributo y otras rutas; no se ha ejecutado. **Seguimiento:** LK-763.


### LKF-522 — Consumo de NODEFAULTLIB en directivas

**Componente:** NSLink. **Estado:** búsqueda y flujo local observados; política completa pendiente. **Evidencia:** A:/nslink-devel/src/link/DirectiveParser.h:40, DirectiveParser.cpp:133–136 y LinkPipeline.cpp:254–292; hashes y búsqueda acotada detallados en A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-44.md. La búsqueda por noDefaultLibraries en archivos cpp/h de src encontró declaración y almacenamiento, sin lectura directa por ese identificador; la ruta de pipeline revisada recorre defaultLibraries. Contrastar otras vías y bibliotecas explícitas antes de afirmar incompatibilidad global. Sin pruebas. **Seguimiento:** LK-763.


### LKF-523 — Intervalos y solapamiento en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** discrepancia local documentada; clientes y efecto pendientes. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/common/redblack.h:64–84, SHA-256 ff1e8e724d30c939952219af59f5d9c1b272f336178f01e1a3d8213b384ae329; redblack.c:403–434 y :543–565, SHA-256 7705600acdbc12c3479d46ef70069e146209c6d18723aa692a44aee7ffbbc46f. La comprobación completa de superposición se condiciona a DEBUG y una consulta describe fin cerrado frente a implementación de fin abierto. Contrastar precondición de intervalos disjuntos y clientes; no se ha reproducido fallo. **Seguimiento:** LK-765.

### LKF-524 — Captura SP en referencia x86

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** hipótesis estática; macros y ABI pendientes. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/common/asm_utils_x86.asm:100–116, SHA-256 7e7e57472b69fda3d576ec16069dbb5940db00bac926cdea6bd0a6125e4f9c5c. La ruta de captura ajusta un registro usado para la salida IP antes de guardarlo como SP; contrastar expansión de macros, ABI y clientes antes de concluir que el valor sea incorrecto. Sin ejecución. **Seguimiento:** LK-765.


### LKF-525 — Propagación del tipo Data en importaciones

**Componente:** NSLink. **Estado:** diferencias de representación observadas; efecto pendiente. **Evidencia:** A:/nslink-devel/src/archive/ImportLibraryWriter.cpp:305–342, SHA-256 aeba1544ec5cef614ba1f18cd1458976e29d73b002f1e1c073c5afe5b3234140; ArchiveResolver.cpp:119–145 y PeImportBuilder.cpp:51–138, hashes en paralelo-depurador-46.md. El escritor codifica isData; las descripciones de la ruta del resolver y builder revisada conservan DLL/nombre sin ese tipo explícito y generan thunk/IAT. Contrastar referencias a variables, formatos y consumidores antes de concluir fallo ejecutable. **Seguimiento:** LK-766.

### LKF-526 — Límites del escritor de bibliotecas COFF

**Componente:** NSLink. **Estado:** límites locales observados; entradas y contratos pendientes. **Evidencia:** A:/nslink-devel/src/archive/CoffArchiveWriter.cpp:76–188, SHA-256 d5065bbfbe62c116f01a24219b7d47231e0442416f2a918bc6b8309b8c79a8ed. build limita miembros a 65535; hay cálculos de tamaños/cursor sin comprobación local explícita en algunas operaciones previas a conversiones. Contrastar formatos, productor y límite final antes de afirmar overflow alcanzable. **Seguimiento:** LK-766.


### LKF-527 — Cobertura de recursos GDI en referencia

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** alcance local de referencia; sin ejecución. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/gdicheck.c:309–341 y :376–380, SHA-256 c7923027705e00ae42340a7aad8552f19710076223cd94bba3825361646580a0. Se describe una comprobación multihilo desactivada por defecto y creaciones no observadas con inyección tardía. No extrapolar a otras versiones ni a LeakKiller. **Seguimiento:** LK-767.


### LKF-528 — Validación de descriptor de desestructuración

**Componente:** compilador NativeSharp, lowering LLVM. **Estado:** flujo local observado; productor y alcanzabilidad pendientes. **Evidencia:** A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionMachine.cpp:2088–2118, SHA-256 b4f369f826f6411553400333e4521bbf740701213ffaf809f49a2e1e79bd1108. El bucle agrega fallo si un descriptor es nulo o malformado, pero usa continue y después accede a children.front() para operandos; investigar invariantes del productor y propagación del error antes de afirmar fallo alcanzable. Sin reproducción. **Seguimiento:** LK-768.

**Ampliación LKF-509, tuplas y packs:** la misma conversión a uint8 aparece en :2111–2118 y :2145–2149 al comparar con operandCount o children.size(). Contrastar límites de productores y cardinalidad aceptada antes de afirmar wrap alcanzable.


### LKF-529 — Atributos de exportación en ruta de archivo .def

**Componente:** NSLink. **Estado:** pérdida local de atributos observada; consecuencias pendientes. **Evidencia:** A:/nslink-devel/src/driver/SdkImports.cpp:198–252, SHA-256 3c610a193c3967db9e1ff74ecafcefdfd98a3ab8a2ea03f31750518848dd9e04; CompatibleDriver.cpp:891–915 y ImportLibraryWriter.cpp, detalles en A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-48.md. El parser .def conserva nombres sin campos ordinal/DATA; la ruta observada construye símbolos con isData predeterminado falso. Corroborar efectos e interfaces alternativas; no se ha ejecutado. **Seguimiento:** LK-763/LK-766.


### LKF-530 — Rutas ARM de pila en referencia Dr. Memory

**Componente:** copia de Dr. Memory, no NativeSharp. **Estado:** stubs locales observados; cobertura global no revisada. **Evidencia:** A:/nsc-memory-tools-reference/drmemory/drmemory/stack_arm.c:40–126, SHA-256 3523502565750656529db2ce5953e1b57eb8f5547fbf983f205b46148fb016cd. Varias funciones de instrumentación de pila indican no implementado; no extrapolar a otras rutas, versiones o al compilador NativeSharp. **Seguimiento:** LK-764.


### LKF-531 — Múltiples secciones de recursos PE

**Componente:** NSLink. **Estado:** restricción explícita en código; sin ejecución. **Evidencia:** A:/nslink-devel/src/pe/PeResourceSection.cpp:109–143, SHA-256 ec2d9be2c2be844dfc993bbef4cf9af5cc8323ba7046fb575d56b153319c9acf. prepare omite secciones descartadas, selecciona las que empiezan por .rsrc y devuelve ResourceMalformed al encontrar una segunda, indicando que esa ruta no las fusiona. Revisar productores y otras rutas antes de generalizar; la escritura parcial y límites de finalize quedan por investigar. **Seguimiento:** LK-769.


### LKF-532 — Manifiesto histórico de corpus no coincide con dos documentos actuales

**Componente:** infraestructura documental del enlazador NSLink; no es una limitación demostrada del compilador ni una falla funcional del enlazador.

**Evidencia:** A:/nslink-devel/MANIFEST_SHA256.txt, SHA-256 8ddd1ce5204f9719631e795b45128b561bb2dcccc87dc025a5b0e2da7c6d5c56, declara para corpus/README.md la huella 19805b346e8f30363207c937e9f4c4106ab9acab7460434fea457a88c097d6f9, mientras su huella actual es 97c9afbca4ea505f95eeb5f0938e083b0e5a4d59d799c48b2c82aef57246af3e. Para nsc_actions/001_objeto_de_runtime.md declara 28ed5c9850d8734badc3879a2cc87b4b2797509f24a4b68351747acfd24b1837 y la huella actual es 9553c2cc688e9d4a310681166b78bbfa30ce3c0fbd88d4e2d814f26d3f1fb2ce. Las huellas de los tres objetos LLVM examinados sí coinciden con el manifiesto. Detalle en A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-57.md.

**Límite y seguimiento:** discrepancias observadas únicamente en comparación de archivos actuales; se desconoce causa, revisión histórica y efecto de terminaciones de línea. Revisar procedencia y alcance antes de actualizar cualquier manifiesto; no concluir corrupción ni cambiar archivos. Sin ejecución de binarios o pruebas.


### LKF-533 — Hipótesis pendiente: extensión de vistas sobre arreglos y cadenas

**Componente:** compilador NativeSharp, lowering LLVM. **Estado:** investigación estática pendiente; no se ha demostrado una limitación alcanzable ni se ejecutaron pruebas.

**Observación:** en A:/nsc-devel/src/LLVM/Lowering/OopLlvmExpressionMachine.cpp:4621–4652 y :4712–4744, SHA-256 b4f369f826f6411553400333e4521bbf740701213ffaf809f49a2e1e79bd1108, las guardas locales aceptan count cero o comprueban índice inicial dentro del largo. En estos fragmentos no se observa validación de que count sea no negativo y no exceda length-index antes de construir la vista.

**Incertidumbre:** los productores MIR, contratos de rest capture o guardas previas podrían garantizar esos invariantes; el count de dos operandos también debe examinarse en contexto. No se ha establecido que un programa fuente pueda generar parámetros inválidos ni que exista acceso fuera de límites.

**Seguimiento:** LK-801: rastrear productores y rutas para ambos tipos de vista; comprobar signo, overflow y alcance de vista vacía; preparar casos futuros de índices y longitudes extremos. No modificar código ni presentar hipótesis como defecto confirmado.


**Ampliación LKF-170, contraste estático adicional:** A:/nsc-devel/tests/cc1/c20_property_compound/Main.ns:11–21 y A:/nsc-devel/tests/unit/Cc1CorrectnessTests.cpp:3714–3809 (SHA-256 475809b41bfffaf6d8636a75facc9f2f729ba20b9cca9b7c51e8031fb79abf70) documentan la restricción y tres expectativas de rechazo para receptor llamada, índice llamada y receptor campo. ConstantExpressionClassifier.cpp:58–135 fue leído íntegro con SHA-256 373b6f226f95feda424bf7eeaca14c601c339e90cf376e19700d2732205c40e2; acepta formas conservadoras y rechaza esas clases de expresiones. El código actual corrobora estáticamente el rechazo en asignación compuesta a propiedades/indexadores; no se ejecutaron pruebas ni se demostró comportamiento binario. El diagnóstico propone extraer a local; cualquier transformación debe preservar orden y vida. No es restricción general de llamadas o campos.


**Ampliación LKF-532:** el informe A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-71.md registra tres discrepancias adicionales del mismo manifiesto histórico: docs/certificacion/2026-09-09_nsl30_pdb_gate.txt (manifest 3b13984806f0c507954d61b86258c33da8479ad158c7d2affc669a956d616050; actual 998345dfacc805506b2e4313b2af45daa25e8f885255acbf6c79f32ed8159e44), docs/certificacion/2026-09-10_dbghelp_pdb_gate.txt (manifest 6ecb904a3adcb30c2d0fc7a6b97b962fdd6da3a82c775559cced3f6f2e97cc86; actual aa778f2f3c5935b7030cb4cb250be2094e1eb01f8b6f5567a152031285789637) y nsc_actions/README.md (manifest 98262943d4294537ef8eca7c0f9358fa615b14fc6951afd2ba3ae2ccab3aff15; actual 91d546e74b4ff75889efa12f9859c585de96c4aa60dd7f2e8ca1a3c2b13a5baf). Causa y revisión desconocidas; no se concluye corrupción ni se actualiza manifiesto.


**Ampliación LKF-533:** contraste selectivo A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-74.md: HirConversion.cpp:54–94 produce vista completa con inicio cero; PatternParser.cpp:339–355 y :1239–1255 construye condición Length >= fixedCount y count Length-fixedCount; :737–752 sitúa bindings de una ruta en el thenBlock. Con la misma longitud y dominancia, esas condiciones sostienen el límite. Aún se requiere verificar MatchArm, let-else, foreach, otros productores y estabilidad del receptor; la hipótesis no está demostrada ni debe presentarse como defecto reproducido.

**Ampliación LKF-532, documentos históricos PDB y CodeView:** las siguientes seis huellas del manifiesto difieren de los archivos actuales; informes paralelo-depurador-72 y 73. No se determinó causa ni revisión; no se infiere corrupción ni cambio semántico.

- markdowns/22_2026-09-09_identidad_pdb_pe_rsds.md: manifiesto 01b16a3c6c51836575a739a37c6eb86e49ab3e08af83992368357d44013df778; actual f6d57bcebb7695262dd457a84a4e2f6f58b7b31cb884945aa2c70f5a37611390.
- markdowns/24_2026-09-09_gate_final_pdb_nsc.md: manifiesto 286a23e1929b80a0988347167018ebf648aacd3269c996ba25167c88bdf4796f; actual d85df379ff7db4d70b076d8150042051531a3e54c761e932c044ff0c985a9f09.
- markdowns/25_2026-09-10_dbghelp_simbolos_publicos.md: manifiesto 3816cbb8d3169988d20c57d69f49eab0169b28ea3d2ddff61aec888e6263b9db; actual 525e406ee2e342cc78d590972c50b5ff0ed707564905490fc445215dd9996091.
- markdowns/17_2026-09-09_consumo_codeview.md: manifiesto befa45002e99ea4b1aadf8fd60aaebd6c97e674a03bbb7c56c54ec60302d6376; actual 2653c49de26d294fadcf2ce5ccda01a892616e405ffd46b240c6259402c1ea9c.
- markdowns/18_2026-09-09_consolidacion_codeview_c13.md: manifiesto dbf2ad1c3861c485baaf8098a91c06fa7be969224cdd802b69dee9979d1ab83b; actual 85671968ca56b97ae7dc36283637dcddf55f0e75caefbfc383ab2b44a8a5a2d4.
- markdowns/21_2026-09-09_simbolos_scopes_y_rangos.md: manifiesto a041a6f2ec8c96f406f1b092c0955110a3e8d2c50358504e0c871efcc577910f; actual d77409b83e10fcc9a33b2bb3acb70e7f2156702c9d111348f8ef9f93ef7cbb85.

**Ampliación LKF-532, escritor y entrega PDB:** las siguientes huellas difieren del manifiesto histórico. Causa y revisión desconocidas; no se infiere corrupción. Informes paralelo-depurador-74 y 75.

- markdowns/19_2026-09-09_escritor_msf_pdb.md: manifiesto 537006394f043302588b4bf0e59a64cb7e66220a336e4e80584f47ae4aa82f63; actual 7c6527b02f2fd61d46e22df8e237c1e1bdf3c2d413eae06d3097d77acabd322c.
- markdowns/20_2026-09-09_familias_de_tipos_codeview.md: manifiesto a44b5d442e8a9eddb5b7f6cf864b4936cab5dc1f6e147ff77fa25cb758bc9f57; actual 72e54b55914dfa048dd43b46679e773d741144acdc9a89ac253de63e506cdaca.
- markdowns/23_2026-09-09_robustez_pdb_atomicidad.md: manifiesto b5b44f5b518dc00c4fd3a29e327ed4c86e2635985b1a87d51327e4404da48a76; actual 14856c0e83797a76d09322f8039321189efad1f8ed8d15f59f0481e6d9d90d3e.
- markdowns/16_2026-09-09_cli_pdb_compatible.md: manifiesto 3b71c809514b9ccfd0f3cf7b56169bd852585010d621bd2ee1663cf7acc98e63; actual 0e5db6479e66450219942f993c3e234bde5dd18751a8c277ede26a22695c8db6.
- markdowns/26_2026-09-11_comdat_asociativas_sin_clave.md: manifiesto 364c5ff62b12890d21cfd90b9a2a3cb7b1ae7bdf37694fbc032f1e5bb2cbc792; actual cd1d446daa2e0ca61acfd82bb5ded10ef3abeded1c6986a7ce29548bee489085.

**Ampliación LKF-532, caché y COMDAT:** diferencias de huella observadas entre manifiesto histórico y documentación; causa y revisión desconocidas, sin inferir corrupción. Informe paralelo-depurador-76.

- docs/3.0/TANDA-35-NSLINK-O0-TELEMETRIA-CACHE.md: manifiesto 9b1bb2684842854fad81156eb9b588d3bc66979fb0dd1a636e7b3c95e92a89f4; actual f1f96086c77edd37b70bef578456a11eca3ebdedda0f7772163081c2601dc6fc.
- docs/3.0/TANDA-36-ALINEACION-NSLINK-3.0.md: manifiesto fe4f9f6aa2e3f6f0bb3674cdfd3de60d9329d9b6108a145e8a44ae4b7bb1978e; actual a59ae928e6ef4c01063396cae85d6fdbc5fdd4aa0f8a81af919bed30cd311e41.
- markdowns/27_2026-09-11_comdat_extraidos_de_bibliotecas.md: manifiesto 5bca10818a39427fd80b05d8a4b0842ad2bee55fe0510177dcad901b02ab6e94; actual f5f624f0001ce0ef8e868b441c67ea81e79d7196c7e49c82986ca964a95101a4.


### LKF-534 — Catálogos históricos no constituyen cobertura ejecutable

**Componente:** infraestructura de pruebas compiler-suite de NativeSharp, no compilador ni depurador. **Estado:** exclusión y denominador constatados por lectura estática; no se ejecutó validador, build ni pruebas.

**Evidencia:** A:/nsc-devel/tests/compiler-suite/controller/integration/validate_source_registration.py:26–44 y :195–247, SHA-256 a1f192fa4d59cfb035ffc7c18ac35864ab99d83637bcd568a097cb16477b5ef6, describe catorce módulos de fase 1 como documentación inactiva, entre ellos catálogos LLVM/runtime/interop y NSC, y verifica la lista y registros activos. El TestRegistry.h actual consultado parcialmente solo enumera Required y KnownProductDefect; InfrastructurePending aparece en soporte histórico inactivo. Informes A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-82.md y paralelo-compilador-84.md; el segundo corrige la posible interpretación del primero.

**Alcance y seguimiento:** los nombres de esos catálogos no son pruebas activas, pasadas ni pendientes seleccionables. Localizar pruebas independientes activas para cada comportamiento y oráculos relevantes antes de cuantificar cobertura; no concluir que no existan otras pruebas ni que falle la compilación. LK-811. No corresponde a los documentos exclusivos de limitaciones del compilador, linker o debugger.


### LKF-535 — Riesgo condicionado de bloqueo en captura del arnés ASAN

**Componente:** infraestructura de pruebas compiler-suite, no compilador, linker ni debugger. **Estado:** hipótesis por lectura estática; no reproducida.

**Evidencia:** A:/nsc-devel/tests/compiler-suite/host/support/AsanSubprocess.h:111–138, SHA-256 1b4ec5b37ecfefc8670180ea3b16871b242716f53db43e396a1e615bfb76a537, espera el proceso con INFINITE antes de leer su pipe combinado. Si el hijo llena el pipe antes de salir, el padre podría esperar al hijo mientras este espera espacio. No se verificaron volúmenes efectivos, capacidad, control externo ni alcanzabilidad. Detalle en A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-91.md.

**Seguimiento:** contrastar controlador y uso real, y planear prueba futura con salida acotada, drenaje concurrente y cancelación. Sin ejecutar procesos ni modificar producto.


### LKF-536 — Hipótesis: clasificación de placement como reserva de arena

**Componente:** compilador NativeSharp, explorador estático de costos. **Estado:** observación localizada, alcance y reproducción pendientes.

**Evidencia:** A:/nsc-devel/src/Frontend/ProjectCostExplorer.cpp:90–101, SHA-256 b9dada4de66edd5d0d5032eecba6c4e09d58fa99cdd4e5e9dac40a5542e58dd5, incrementa arenaAllocationSites y registra causa arena para MIR PlacementConstruct sin comprobar en ese switch la procedencia del almacenamiento. A:/nsc-devel/tests/integration/debug_pdb_extreme/Producer.ns:245–250 muestra placement sobre stackalloc; no se ha demostrado que ese ejemplo llegue al explorador con ese nodo. Informe A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-94.md.

**Seguimiento:** trazar productor y consumidor MIR, alcance de explain y origen del almacenamiento; distinguir construcción en memoria existente, reserva de arena y evento dinámico. No se ha ejecutado ni demostrado un conteo erróneo o defecto de memoria.


**Ampliación LKF-536:** el contraste estático en A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-95.md enlaza PlacementConstructionExpression del parser con HIR PlacementConstruction, MIR PlacementConstruct, la clasificación incondicional de ProjectCostExplorer y la opción --explain-project-costs. La ruta depende de binding y layout válidos y de que el modelo alcance el explorador; no se ejecutó el ejemplo ni se observó el reporte. La evidencia sustenta una posible clasificación engañosa de placement en stack o almacenamiento prestado como arena, no un error de memoria.

**Contexto ASAN:** LlvmNativeSanitizer.cpp, SHA-256 9caf68db1210d588fbf04e4af8e5d5d68599f7994dffa133dfd114c0bdb248ff, añade atributo sanitize_address y ejecuta pase ASAN en su ruta; la declaración del driver de debug allocator missing no demuestra ausencia global de ASAN. Selección y cobertura dinámica pendientes.


### LKF-537 — Inconsistencias en el contrato DAP del depurador

**Componente:** documentación de NSDBG. **Estado:** discrepancias documentales verificadas estáticamente; funcionamiento no probado.

**Evidencia:** A:/nsc-debugger/docs/DAP_CONTRACT.md, SHA-256 3a8dfa57fe6dccd4704f01f03fa418937b27206a2e7968ed49eadac9d80300fd: líneas 148–150 presentan frames inline como futuros, mientras 180–200 describen su presentación disponible. El contraste parcial de DebugEngine.cpp:1990–2028, DbgHelpSymbolProvider.cpp:508–535 y DapSession.cpp:2015–2044 muestra una ruta de construcción y serialización de frames inline; no demuestra cobertura completa. La tabla y sección de data breakpoints se repiten tres veces. En offset de byte cero-basado 7278, línea 124, aparece BEL 0x07 seguido de llowPartial, en lugar de la clave ASCII allowPartial. Detalles y huellas de fuentes en A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-94.md y paralelo-depurador-95.md.

**Seguimiento:** conciliar cronología y texto bajo ownership del depurador, eliminar control y duplicaciones solo en fase autorizada, y contrastar contrato con implementación y pruebas. El límite funcional de inline se registra por separado en LKF-131; no se añade duplicado funcional. Sin ejecución ni cambios en NSDBG.


- **Ampliación LKF-537 — copia de distribución DAP:** A:/nsc-debugger/share/nsdbg/docs/DAP_CONTRACT.md SHA-256 8ec3395cb30d3244dea6d8bd1d0f9f7992797f400e872c1b1d30ff85b0213e45 difiere de A:/nsc-debugger/docs/DAP_CONTRACT.md SHA-256 3a8dfa57fe6dccd4704f01f03fa418937b27206a2e7968ed49eadac9d80300fd. La copia share conserva inline como futuro, repite data breakpoints e incluye BEL antes de llowPartial; docs añade secciones de inline, async y módulo/PDB. Diferencia documental, sin determinar qué paquete se instala ni comportamiento real. Ver [dictamen-categorias-contratos-d27.md](A:/nsc-leak-killer/docs/fase-0/dictamen-categorias-contratos-d27.md); LK-1165.

- **Contraste de empaquetado LKF-537:** CMakeLists.txt:265–274 del debugger instala documentación desde docs/, no demuestra instalar la copia share preexistente. Ver [lectura-probe-results-lkf-612.md](A:/nsc-leak-killer/docs/fase-0/lectura-probe-results-lkf-612.md); sin inspección de paquete instalado ni ejecución.

### LKF-538 — Declaración estática de namespace y enlace local de archivo

**Componente:** parser del compilador NativeSharp. **Estado:** restricción declarada por código actual en esta ruta; no reproducida mediante compilación.

**Evidencia:** A:/nsc-devel/src/Parser/AttributeParser.cpp:811–840, SHA-256 ea22374187adef644e0c53e59dc5e3aaa66d7d6c822b311fed1596b4531ba7b4, en el fallback de declaración global rechaza static fuera de parsingExtensionBody_ con diagnóstico que indica que declaraciones static de namespace/enlace local a archivo no se soportan y propone estado interno con nombre único.

**Alcance y seguimiento:** no generalizar a campos o métodos static ni a otros contextos; contrastar parser, reglas semánticas y casos de interop C que requieran enlace interno. Documentar impacto en identidad de símbolos y análisis de módulos de LeakKiller. No se ejecutaron pruebas ni se modificó producto.


**Ampliación LKF-536, fixture candidato:** A:/nsc-devel/tests/unit/NativeSharp3T06PinningTests.cpp:91–174, SHA-256 865a3fa8ff92c1b40f73596837a41993c6134c8400daa9ac993c351076b09fb1, plantea placement sobre stackalloc y espera un nodo placement-construct en el dump MIR. Es una expectativa no ejecutada y no comprueba el reporte de costos; permite preparar un contraste dirigido con la misma revisión, texto/JSON y origen del almacenamiento.


### LKF-539 — Límite de offset explícito de campo

**Componente:** parser del compilador NativeSharp. **Estado:** límite explícito observado en código; no reproducido mediante compilación.

**Evidencia:** A:/nsc-devel/src/Parser/AttributeParser.cpp:1231–1254, SHA-256 ea22374187adef644e0c53e59dc5e3aaa66d7d6c822b311fed1596b4531ba7b4, acepta un literal entero para at(N), pero diagnostica y descarta offsets mayores que 0xFFFFFF bytes. El límite aplica al offset explícito de campo en esa ruta; no describe por sí solo el tamaño máximo de cualquier objeto ni un fallo de acceso en ejecución.

**Seguimiento:** verificar si otras rutas de declaración y metadata comparten el mismo límite, documentar efecto en bindings C/Clang y ABI, y diseñar diagnóstico de LeakKiller que preserve valor y ubicación. Preparar pruebas futuras de borde sin ejecutar en fase 0.


### LKF-540 — Versiones aceptadas incompletas en documentación de VS Code

**Componente:** documentación de integración del depurador NSDBG. **Estado:** discrepancia documental corroborada estáticamente; versiones instaladas y handshake no ejecutados.

**Evidencia:** A:/nsc-debugger/docs/VSCODE_INTEGRATION.md:10–16, SHA-256 b62c6de9b94ec118d131001bc6d6f8132fc7a0403cb6a0ca9bf28b41ea50248e, menciona 0.9.x/1.x. A:/native-sharp-vscode/src/debugger-contract.ts, SHA-256 d991405470c16213a01622e9b52b28324054e4bf554768f7c836f27a1e6cff72, acepta major 0 con minor >=9 y major 1–3, sujeto a Windows/x64 y protocolo esperado; el test existente incluye 3.0.0. El código fuente del depurador declara 3.0.0/protocolo 1 según contraste parcial de A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-104.md.

**Seguimiento:** conciliar documento con política y versiones reales de extensión y depurador; validar handshake de artefactos instalados y capabilities en fase posterior. No se demuestra incompatibilidad funcional ni se ejecutaron pruebas.


### LKF-541 — Guía de diagnóstico recomienda dependencias y versión antiguas

**Componente:** documentación y configuración de empaquetado de NSDBG. **Estado:** discrepancia estática verificada; paquete instalado no inspeccionado.

**Evidencia:** A:/nsc-debugger/docs/TROUBLESHOOTING.md:35–41,51–57, SHA-256 ddba339543c67a3728132ca3aa4dc86426b29ff11c62260907c45d04a5796b7d, aconseja restaurar LLVM-C.dll y refiere NSDBG 1.0. A:/nsc-debugger/docs/THIRD_PARTY.md y CMakeLists.txt actuales describen LLVM estático y rechazan esa DLL como fallback; tests/CheckPackage.cmake espera versión 3.0.0. CMake instala docs y el test exige la guía. Rutas, hashes y límites en A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-106.md.

**Seguimiento:** contrastar paquete real, imports y documentación instalada antes de actualizar la guía bajo tarea autorizada posterior. No inferir defecto funcional, eliminar archivos o ejecutar el test de paquete en esta fase. El PDB del depurador es distinto de los PDB del programa objetivo.


### LKF-542 — Posible interferencia entre ejecuciones concurrentes de fixture

**Componente:** infraestructura de pruebas compiler-suite. **Estado:** riesgo condicionado por lectura estática; no reproducido ni atribuido al compilador.

**Evidencia:** A:/nsc-devel/tests/unit/ProjectCallableWaveDriverTests.cpp:111–127, SHA-256 d73a2bcc694e8e3ec889ab836dc6162e93a7a64b647d236b8f00ece3ca34629f, usa una subcarpeta fija del directorio temporal y la elimina antes y después del escenario. Dos instancias con la misma raíz podrían interferir. No se verificó si el controlador serializa o aísla las ejecuciones ni se ejecutó la prueba. Informe A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-113.md.

**Seguimiento:** verificar aislamiento real del controlador y, en una fase futura, diseñar raíces únicas y conservación de evidencia antes de paralelizar pruebas. No ejecutar ni modificar el fixture en fase 0.

### LKF-543 — Restricción sintáctica de payload borrowed directo en variant

**Componente:** compilador NativeSharp, parser de declaraciones. **Estado:** restricción explícita en el código revisado, no defecto demostrado. **Evidencia:** A:/nsc-devel/src/Parser/DeclarationParser.cpp:823–866, SHA-256 950829011e5140dfd4c8c0d13183d7dbe90becf5642f706bbf6d61b3f8fd938c. La comprobación rechaza un calificador de ownership Borrowed directamente en el tipo de payload y el diagnóstico propone envolver el préstamo en una vista struct borrowed. La ruta también rechaza payload void y más de un tipo separado por coma, remitiendo a tupla. **Pendiente:** contrastar semántica y representaciones inferiores, cobertura de formas anidadas, documentación y vigencia; especificar cómo LeakKiller modelará almacenamiento y vida sin suponer que se admiten todas las formas prestadas. Sin compilación ni ejecución. LK-844.

### LKF-544 — Métodos genéricos de trait rechazados por object safety en ruta actual

**Componente:** compilador NativeSharp, parser de interfaces y traits. **Estado:** restricción explícita de la vertical indicada por el diagnóstico; no se afirma defecto ni prohibición de todas las funciones genéricas. **Evidencia:** A:/nsc-devel/src/Parser/DeclarationParser.cpp:613–637, SHA-256 950829011e5140dfd4c8c0d13183d7dbe90becf5642f706bbf6d61b3f8fd938c. Al procesar un método de trait con parámetros genéricos, emite error de object safety y sugiere mover el parámetro al trait. **Pendiente:** contrastar las rutas semánticas, casos dinámicos y estáticos, documentación y planes actuales; diferenciar limitación deliberada de implementación incompleta y preservar identidad de especialización al analizar llamadas. Sin compilación ni ejecución. LK-845.

### LKF-545 — Rutas históricas ausentes en mapa de conformidad

**Componente:** documentación de conformidad NativeSharp. **Estado:** brecha de trazabilidad, no defecto funcional demostrado. **Evidencia:** A:/nsc-devel/spec/NativeSharp_1_0_conformance.map, SHA-256 15dab04579149031e7cbd2aad3868b8bb66f48f41df8449a91042137bc67ccf0; informe paralelo-compilador-124. Seis referencias apuntan a dos rutas que no existen en el checkout: src/Frontend/ProjectOopHirModel.cpp y src/Frontend/ProjectOopMirModel.cpp. **Pendiente:** identificar reemplazos, conservar historia y reconciliar versiones de especificación y evidencia antes de actualizar el mapa; sin ejecutar pruebas. LK-852.

### LKF-546 — Modos restringidos en parámetros promovidos a campos

**Componente:** compilador NativeSharp, parser de constructor primario de clase. **Estado:** diagnóstico explícito observado; no defecto funcional demostrado. **Evidencia:** A:/nsc-devel/src/Parser/DeclarationParser.cpp:1421–1482 y :1485–1515, SHA-256 950829011e5140dfd4c8c0d13183d7dbe90becf5642f706bbf6d61b3f8fd938c. El parser informa error cuando el parámetro promovido contiene texto auxiliar y el diagnóstico indica que no puede ser params, ref, out o in; genera un campo y el constructor sintetiza su asignación. **Pendiente:** verificar la procedencia del texto auxiliar, casos reales de cada modo y tratamiento de ownership en binding/lowering antes de definir alcance completo; sin compilar ni ejecutar. LK-858.

### LKF-547 — PDB propio del debugger separado del ejecutable e instalación

**Componente:** debugger NativeSharp, configuración CMake. **Estado:** política estática de ubicación observada; no se ejecutó build ni instalador. **Evidencia:** A:/nsc-debugger/CMakeLists.txt:233–252 y :265–268, SHA-256 7cd8d27d1ea7e8bb763a1c37f4274a0a9d5edd68be9011a07cacd414ced5fa6d; informe paralelo-depurador-118. La salida personalizada de nsdbg puede quedar separada de su PDB, cuyo destino está fijado en el árbol de build; la instalación descrita no lo incluye en el prefijo. No confundirlo con el PDB del programa depurado. **Pendiente:** decidir archivo o distribución de símbolos privados del propio debugger y verificar rutas e identidad en una compilación futura; no afirmar PDB ausente sin observar artefactos. LK-862.

**Ampliación LKF-546:** En A:/nsc-devel/src/Parser/DeclarationParser.cpp:1687–1778 la ruta de struct también rechaza params/ref/out/in y, específicamente, cualquier calificador de ownership distinto de None; el diagnóstico cita owned/borrowed y sugiere declarar campo y constructor explícitos por el contrato T13 y partial-drop. También rechaza campo promovido protected. Es restricción explícita de esta ruta, sin extrapolar a todos los campos o constructores. Verificar semántica actual y casos anidados; sin ejecutar. LK-858.

### LKF-548 — Posible divergencia de destino del constructor de nsdbg 3.0

**Componente:** debugger NativeSharp, constructor gráfico. **Estado:** riesgo condicionado por flujo y destino; no se comprobó uso, instalación ni sobrescritura. **Evidencia:** A:/nsc-debugger/nsdbgbuilder.pyw:35,149,250, SHA-256 a7d7a6051ac95c5841a29c1c6e793cb2721bedb0d9d4d0d9ce4957d88fb3246b; A:/nsc-debugger/CMakeLists.txt:121,233–252, SHA-256 7cd8d27d1ea7e8bb763a1c37f4274a0a9d5edd68be9011a07cacd414ced5fa6d; informe paralelo-depurador-120. El documento 3.0 plantea aislamiento en C:/native-sharp-3.0; el constructor conserva un destino C:/native-sharp/bin y pasa una salida a CMake. Raíz de descubrimiento y salida son parámetros distintos. **Pendiente:** revisar flujo completo, elección y propiedad de destino, rama y configuración persistida antes de concluir incompatibilidad; no ejecutar ni modificar instalaciones en fase 0. LK-868.

**Evidencia adicional LKF-546:** A:/nsc-devel/tests/unit/NativeSharp3T02PromotedFieldTests.cpp:240–246, SHA-256 a03398db3168cee8c99c736bdc0c77dfa56ae676f2adf063396c59fede00d02d, espera rechazo de un campo promovido owned string con mensaje partial-drop contract. Es aserción escrita, no ejecutada. El fixture también busca nombres en bytes de objeto; eso no valida por sí mismo CodeView o PDB. Informe paralelo-compilador-132.

### LKF-549 — Alcance de extern y vectorcall en parser

**Componente:** compilador NativeSharp, parser de declaraciones externas. **Estado:** diagnósticos y rutas observados; no se ha comprobado comportamiento mediante compilación. **Evidencia:** A:/nsc-devel/src/Parser/DeclarationParser.cpp:2097–2148, SHA-256 950829011e5140dfd4c8c0d13183d7dbe90becf5642f706bbf6d61b3f8fd938c. La ruta informa que las variables extern de namespace no están soportadas en NativeSharp 3.0 y orienta a una única definición de estado o frontera de función extern C. En esa misma ruta extern vectorcall acepta una declaración delegate y rechaza otras formas. **Pendiente:** contrastar sintaxis alternativa, importación C/Clang, ABI por target, documentación y semántica de otras rutas; no extrapolar la limitación a todas las llamadas vectorcall o globales externas. LK-876. Sin ejecución.

**Ampliación LKF-548:** lectura íntegra de A:/nsc-debugger/nsdbgbuilder.pyw, SHA-256 a7d7a6051ac95c5841a29c1c6e793cb2721bedb0d9d4d0d9ce4957d88fb3246b, informe paralelo-depurador-125. La GUI presenta destino fijo sin selector, lo transmite en NSDBG_PRODUCT_OUTPUT_DIR y deshabilita tests; al concluir verifica existencia de nsdbg.exe, no su identidad o hash. Permanecen sin verificar el uso real, propiedad de la ruta y cualquier escritura o sobrescritura.

### LKF-550 — Posible cambio de configuración durante build del constructor configurable

**Componente:** debugger NativeSharp, nsdbgbuilder.py. **Estado:** hipótesis estática sin reproducción. **Evidencia:** A:/nsc-debugger/nsdbgbuilder.py, SHA-256 44fff04e069e11b9c6954798b951c35d4f2961e8e1cf6b79931ffa4240c03adc; informe paralelo-depurador-126. El constructor lee campos en etapas posteriores y la lectura no encontró deshabilitación explícita de configuración, árbol ni salida durante el build. Si la interfaz permite editarlos, la configuración inicial podría diferir de la usada al continuar o comprobar el resultado; también comprueba existencia de nsdbg.exe, no su identidad. **Pendiente:** inspeccionar condiciones completas y verificar en entorno autorizado si el cambio es posible y tiene efecto; no se ejecutó ni observó fallo. Los archivos .py y .pyw comparten QSettings, pero .pyw no usa output_directory. LK-881.

### LKF-551 — Anotación stopOnEntry y valor al omitir en launch

**Componente:** debugger NativeSharp, esquema y adaptador DAP. **Estado:** divergencia contractual estática; efecto en cliente no verificado. **Evidencia:** A:/nsc-debugger/contrib/nativesharp-debug.schema.json, SHA-256 758169a05699ecc264033a5b96af445895ceec6c9f3a55a7046d2557434b7254, anota default true; A:/nsc-debugger/src/dap/DapSession.cpp:1626–1627 y :1669, SHA-256 549697a1d2a7eb0dada1649a2060100b34719ebbafa1af7961bdcdc892ef6913, usa false al omitirse en launch y true en attach. El default de JSON Schema es anotación: el cliente puede o no enviarlo. **Pendiente:** contrastar cliente y contrato de versión; especificar stopOnEntry en instrucciones de LeakKiller y no afirmar falla observada. Sin ejecutar. LK-885.

### LKF-552 — Entrada estándar NUL al capturar flujos en launch DAP

**Componente:** debugger NativeSharp, captura de flujos del proceso. **Estado:** comportamiento observado estáticamente para la ruta de captura indicada; impacto de ejecución pendiente. **Evidencia:** A:/nsc-debugger/src/core/StandardStreamPipes.cpp, SHA-256 8667b9412ac2d71d568d6e08493435c2e0594d2097251ec9f833477c0d4d9ec5, crea pipes de stdout/stderr y abre NUL para stdin; DapSession.cpp:1630 habilita captureStandardStreams en launch y DebugEngine.cpp:685–724 usa la captura condicional. Un programa interactivo podría recibir EOF o comportarse distinto; no se ha ejecutado y no se extrapola a attach o todas las rutas de consola. **Pendiente:** contrastar alternativas de entrada, lectores y drenaje, y decidir cómo LeakKiller reportará cobertura y canales de entrada/salida. LK-889.

### LKF-553 — Lector PeImage mínimo limitado a imagen mapeada AMD64

**Componente:** debugger NativeSharp, lector de encabezados en memoria. **Estado:** alcance estático observado; no defecto demostrado. **Evidencia:** A:/nsc-debugger/src/core/PeImage.h y PeImage.cpp, SHA-256 f44afd22e801690d3326e441b341d1978885de27ac72f406bcb2cc68f3a92e08 y eb1a460bc72dc99b32821e865d71244b0c6fb07b80e6ae7b5d9b3d0bac94248c; informe paralelo-depurador-131. Exige PE32+ AMD64 y lee campos mínimos mediante memoria del proceso; no recorre secciones ni extrae directorio debug, RSDS, PDB o instrucciones en este componente. No se extrapola a otros proveedores de símbolos o lectores. **Pendiente:** identificar componentes que hacen esas funciones y diseñar vía separada para EXE/DLL en disco y símbolos ausentes; sin ejecución. LK-891.

### LKF-554 — Cálculo de dirección y procedencia de ruta de imagen

**Componente:** debugger NativeSharp, recuperación de ruta remota. **Estado:** observación estática y posible límite, sin error reproducido. **Evidencia:** A:/nsc-debugger/src/core/ImagePath.cpp, SHA-256 116f57b42d565c7748d364a4a4646b5f55da91dc33e3f3c23d14e9331abb29f8; informe paralelo-depurador-132. readRemoteString limita la cadena a 32 Ki caracteres y devuelve vacío ante lectura incompleta o ausencia de NUL; en la expresión address + index * sizeof(Character) no se aprecia comprobación explícita de desbordamiento. La interfaz devuelve ruta sin explicar la fuente elegida o causa de ausencia. **Pendiente:** contrastar invariantes del consumidor, sumas, caracteres, lecturas parciales y efecto real; no afirmar vulnerabilidad o fallo observado. LK-895.

### LKF-555 — Transferencias parciales y límite del servicio de memoria

**Componente:** debugger NativeSharp, MemoryService. **Estado:** contrato y límites observados en fuente, sin prueba ejecutada. **Evidencia:** A:/nsc-debugger/src/core/MemoryService.cpp, SHA-256 3f7d62c4f1dcc2b34e413802866364499a7569a0d1b832495bd6640fa203521e; informe paralelo-depurador-133. Limita cada operación a 1 MiB y puede devolver true para lectura con sufijo unreadableBytes no cero. Para escritura allowPartial=false verifica regiones antes, pero una operación posterior puede fallar o ser parcial; no se observa rollback transaccional. La política de autorización y estado pertenece a consumidores no examinados en este lote. **Pendiente:** preservar bytes reales y diagnóstico, contrastar consumidores y carreras; no afirmar falla reproducida ni exigir escritura como función de LeakKiller. LK-897.

### LKF-556 — Desensamblado textual y causa ambigua de Invalid

**Componente:** debugger NativeSharp, DisassemblyService x64. **Estado:** alcance observado en fuente, sin ejecución. **Evidencia:** A:/nsc-debugger/src/core/DisassemblyService.cpp, SHA-256 22de65587103e481cb35a687dac20cb341f58beeea068f47107da8b8472dee8b; informe paralelo-depurador-134. El servicio decodifica con LLVM y clasifica el texto de mnemonic y operandos; si no hay contexto LLVM o no obtiene instrucción devuelve db/Invalid para avanzar. En ese componente el resultado no distingue por sí mismo todas las causas de Invalid ni describe semántica completa de accesos de memoria. **Pendiente:** contrastar consumidores y disponibilidad, inicio de instrucción, bytes truncados y suma address+offset; no atribuir defecto observado ni extender la conclusión a otros decoders. LK-899.

### LKF-557 — Límites asimétricos y garantías del transporte DAP

**Componente:** debugger NativeSharp, DapTransport. **Estado:** alcance observado en código; sin pruebas ejecutadas. **Evidencia:** A:/nsc-debugger/src/dap/DapTransport.cpp, SHA-256 0e2b460eb1ca0b7da0b4f0e2410e725d9603a9343da26326c7001cd8d92d655c; informe paralelo-depurador-135. La lectura limita cabeceras a 8 KiB/64 líneas y cuerpo a 16 MiB; write serializa y usa mutex, pero no muestra el mismo límite de tamaño de salida ni confirmación persistente del receptor. El mutex evita intercalación de bytes, no demuestra orden causal global. **Pendiente:** verificar productores y cliente, decidir paginación o referencia externa para reportes y manejo de desconexión/pérdida; no presentar comportamiento no probado como falla. LK-902.

### LKF-558 — Estado de corutina deducido de nombres de símbolo

**Componente:** debugger NativeSharp, presentación de frames. **Estado:** alcance observado; sin ejecución. **Evidencia:** A:/nsc-debugger/src/core/FramePresentation.cpp, SHA-256 917c06d5a21c7b70177516b09e6bdc36e40cc44c676ed7a2bc0e380af9855463; informe paralelo-depurador-136. La presentación deduce clase y estado a partir de marcadores en nombres de símbolo, no lee el estado del objeto de runtime en esta función; None puede significar que no hubo reconocimiento. **Pendiente:** contrastar productor de nombres, símbolos y versiones, preservando procedencia e incertidumbre. LK-905.

### LKF-559 — Resúmenes visuales y recorrido recursivo de valores

**Componente:** debugger NativeSharp, visualizadores. **Estado:** comportamiento estático, sin fallo reproducido. **Evidencia:** A:/nsc-debugger/src/core/NativeSharp3Visualizers.cpp, SHA-256 13b5fedfb86639d2abe581151d82cf5b05b6523d1fa775db01ad62c47822da9f; informe paralelo-depurador-137. Reconoce categorías por convenciones textuales; limita resúmenes a 16 hijos/512 bytes, mientras recorre hijos recursivamente y no declara un presupuesto propio de profundidad en ese archivo. Eso no demuestra ausencia de límites en productores. **Pendiente:** contrastar construcción del árbol y presupuestos, conservar valores brutos y estados unavailable/truncado; no presentar resumen como snapshot completo. LK-906.

### LKF-560 — Restricciones de modificadores y layout de variant

**Componente:** compilador NativeSharp, parser de atributos de declaración. **Estado:** diagnósticos explícitos observados; no son defectos reproducidos. **Evidencia:** A:/nsc-devel/src/Parser/AttributeParser.cpp:995–1022 y :1056–1072, SHA-256 ea22374187adef644e0c53e59dc5e3aaa66d7d6c822b311fed1596b4531ba7b4; informe paralelo-compilador-149. En esta ruta una declaración variant rechaza el modificador borrowed, readonly y atributos de layout pack/align/size. Es independiente de LKF-543 sobre payload borrowed directo; no implica que toda referencia prestada a una variante esté prohibida. **Pendiente:** contrastar contrato de versión, rutas semánticas y diagnósticos con casos reales; sin compilación ni ejecución. LK-910.

### LKF-561 — Evaluación de ambos operandos lógicos en expresiones del debugger

**Componente:** debugger NativeSharp, evaluador. **Estado:** comportamiento estático observado; contrato vigente e impacto sin ejecutar. **Evidencia:** A:/nsc-debugger/src/core/ExpressionEvaluator.cpp:539–557, SHA-256 18c9e58495326bc11527221964d13384db1ded8999c7d2b9ccf2db0f74c72c38. logicalOr y logicalAnd resuelven ambos operandos antes de combinarlos, mientras DEVELOPMENT_PLAN.md:795 enuncia cortocircuito como objetivo histórico. Esto puede afectar expresiones como false && (1 / 0), sujeto a contrastar versión y ejecución. **Pendiente:** validar contrato y casos dirigidos, conservando error y generación de parada; no atribuir al compilador. LK-913.

### LKF-562 — Aritmética de índice de puntero sin comprobación local explícita

**Componente:** debugger NativeSharp, evaluador. **Estado:** riesgo estático condicionado; no se ha observado falla. **Evidencia:** A:/nsc-debugger/src/core/ExpressionEvaluator.cpp:1028, SHA-256 18c9e58495326bc11527221964d13384db1ded8999c7d2b9ccf2db0f74c72c38. elementAt calcula dirección mediante base + index * tamaño sin comprobación explícita local de overflow. **Pendiente:** verificar límites de tipos, consumidores y materialización, y diseñar pruebas con índices extremos; no afirmar explotación ni error reproducido. LK-914.

### LKF-563 — Conversión de longitud de texto a int en Utf

**Componente:** debugger NativeSharp, conversión UTF-8/UTF-16. **Estado:** observación estática condicionada; impacto y accesibilidad sin verificar. **Evidencia:** A:/nsc-debugger/src/core/Utf.cpp, SHA-256 3077d7aaa88cd2d4f43428633d6cc33a86fe1678037a5228113eae5d4be879fb; informe paralelo-depurador-140. convertText convierte text.size() de size_t a int sin comprobación explícita local antes de llamar APIs Win32. Puede existir un límite anterior en cada consumidor. **Pendiente:** rastrear todos los consumidores y sus límites, tipos y política de error; solo después preparar validación de entradas grandes. No se ejecutó ni demostró falla. LK-917.

### LKF-564 — Filtro vacío informado como éxito por builder de escenarios

**Componente:** debugger NativeSharp, infraestructura de escenarios profundos. **Estado:** flujo estático observado, sin ejecución. **Evidencia:** A:/nsc-debugger/tests/deep/build_programs.py, SHA-256 c26dc33c591ef00f784795d2d552b0cfb7a7bd3a0cc47e9cd029fd3db4df7e4c; informe paralelo-depurador-145. Si --only no coincide con directorios, la selección queda vacía y el script informa 0/0 y termina con código cero. Además el éxito de cada build se basa en exit code sin verificar identidad PDB/EXE o ejecutar el escenario. **Pendiente:** contrastar consumidores y uso real; registrar número esperado y distinguir selección vacía de escenarios validados. Sin ejecutar script ni cambiar producto. LK-928.

### LKF-565 — Referencia absoluta de Core en proyectos profundos

**Componente:** debugger NativeSharp, corpus de pruebas profundas. **Estado:** ruta fija observada en archivos; resolución y mezcla de versiones no verificadas. **Evidencia:** nueve proyectos A:/nsc-debugger/tests/deep/programs/*/*.nsproj, hashes en informe paralelo-depurador-146, referencian C:/native-sharp/lib/NativeSharp.Core/NativeSharp.Core.nsproj. Seleccionar nsc en build_programs.py no cambia literalmente esas referencias. **Pendiente:** contrastar resolución efectiva de ProjectReference y propiedad del SDK frente al aislamiento 3.0; registrar compilador y Core realmente usados antes de validar. No se ejecutó build ni se afirma mezcla efectiva. LK-931.

### LKF-566 — Comentario de collection spread aparentemente desactualizado

**Componente:** documentación interna del parser NativeSharp. **Estado:** inconsistencia textual local, sin conclusión sobre soporte real. **Evidencia:** A:/nsc-devel/src/Parser/ExpressionParser.cpp:1452–1475, SHA-256 656a85e51e5de6f970454c488d9c1f76c35ef663a5d9570ed597d1875165eb7d. El comentario inicial excluye spread de esta base sintáctica, pero la rama posterior crea SpreadElementExpression. **Pendiente:** contrastar semántica y lowering, aclarar el alcance histórico y no inferir soporte completo; sin ejecutar. LK-941.

### LKF-567 — Formatos y ancho limitados en interpolación dinámica

**Componente:** compilador NativeSharp, parser/lowering sintáctico de interpolación runtime. **Estado:** diagnóstico y ramas observados estáticamente; sin ejecución. **Evidencia:** A:/nsc-devel/src/Parser/ExpressionParser.cpp:1789–1800 y :1844–1880, SHA-256 656a85e51e5de6f970454c488d9c1f76c35ef663a5d9570ed597d1875165eb7d. La ruta acepta formatos D/X con ancho, F con precisión y G sin precisión, con dígitos de ancho/precisión limitados a 64; otros generan diagnóstico y el AST recurre a ToString. La ausencia de formato tiene su propia ruta. **Pendiente:** contrastar otras rutas estáticas, tipos y documentación de versión; no afirmar que todos los formatos de cadena del lenguaje están limitados igual. Sin compilación ni ejecución. LK-952.

**Contraste adicional LKF-049/LKF-365:** TemplateCollector.cpp:206–214 y :254–262, SHA-256 6cefbca6b2ed799f6e3e9670dbc65c43cb8734b2deea23a2fbb19de73b1e5339, emite diagnósticos para flag de tipo genérico partial y método genérico dentro de tipo genérico en esas rutas. Fixture Stage44GenericsMonomorphizationTests.cpp SHA-256 e41e7b35a084bcb2dfc0bf18d62532105f1b33c8e6d5b37497f6667348c480ba espera ambos rechazos. Falta trazar productores de flags y variantes de pipeline; no se ejecutó ni se infiere carencia general de constraints. Informe paralelo-compilador-186, LK-625.

### LKF-568 — Fixture de propiedades acepta diagnóstico distinto del esperado

**Componente:** pruebas de compilador, no compilador de producto. **Estado:** debilidad estática del oráculo; sin ejecución. **Evidencia:** A:/nsc-devel/tests/unit/Stage38PropertiesIndexersOperatorsTests.cpp:88–106, SHA-256 120e232b32442b6c7b665d23ba67bc44305a406c78591c7218ba64174f8f23ce; informe paralelo-compilador-187. expectInvalid acepta el caso si hay cualquier diagnóstico no vacío, aun cuando no coincida expectedText. No demuestra que el compilador emita un diagnóstico incorrecto. **Pendiente:** verificar ID y mensaje específico en validación futura y revisar el resto del harness; sin ejecutar ni modificar la prueba. LK-996.

### LKF-569 — Evidencia débil de asociación entre firmas ABI y funciones

En A:/nsc-devel/tests/unit/Stage40StructValueAbiTests.cpp:270, ciertas aserciones buscan nombres y patrones de firma por separado en el módulo. Dos coincidencias pueden pertenecer a funciones distintas, por lo que esa comprobación no acredita la firma exacta de cada una. Limitación de evidencia del fixture, no defecto confirmado del compilador. Lectura estática, sin ejecución; ver A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-188.md y LK-997.

### LKF-570 — Cobertura de foreach menor a la descrita en el fixture

Stage52_5ForeachSemanticGapsTests.cpp:221–250 presenta como caso de higiene de enumerador un ejemplo que itera un array; otro caso describe verificación LLVM, pero el helper consultado solo devuelve el texto del módulo. El rechazo de const verifica únicamente !ok. Estas observaciones limitan las conclusiones de las pruebas, no demuestran un defecto de NativeSharp. Lectura sin ejecución: A:/nsc-leak-killer/docs/fase-0/paralelo-compilador-190.md; seguimiento LK-1006.

### LKF-571 — Test de reloj verifica ausencia de un símbolo anterior

Stage53_1TimeTests.cpp:169 busca ausencia de __nativesharp_console_writeline; WindowsRuntimeBuilder.cpp identifica ese nombre como anterior y declara __nativesharp_console_write. La aserción no descarta por sí misma una llamada al nombre actual. Es limitación de evidencia de test, sin llamada espuria ni defecto del compilador demostrado; ver paralelo-compilador-191.md y LK-1011.

### LKF-572 — Comentarios contradictorios sobre lanzamiento en test de argumentos

Stage53_5CommandLineArgsTests.cpp:235,332–333 describe fork/execvp sin shell, mientras :154–162 construye un comando para std::system. Rutas concatenadas sin quoting y el fallback de retorno son riesgos dependientes del entorno pendientes de contrastar, no fallos reproducidos. Hallazgo documental de infraestructura, no limitación exclusiva del compilador. Ver paralelo-depurador-184.md y LK-1014.

### LKF-573 — Aserciones de atomicidad no vinculan todos los operandos u órdenes

Stage54_2AtomicsTests.cpp:339–356 usa búsquedas globales que no asocian cada dirección con su llamada; la búsqueda de seq_cst comprueba al menos una presencia, no que se aplique a todas las operaciones descritas. Limitación del fixture, no defecto demostrado en LLVM o NativeSharp. Sin ejecución. Ver paralelo-compilador-195.md y LK-1017.

### LKF-574 — Fixture de posible deadlock carece de límite de tiempo

Stage54_3MutexTests.cpp:146–175 invoca std::system sin deadline; el escenario de regresión por deadlock puede permanecer esperando hasta intervención externa. El fixture también usa algunas búsquedas globales que no vinculan llamada y cuerpo de función. Es limitación del arnés de pruebas, no prueba de un deadlock actual. Revisión sin ejecución: paralelo-compilador-196.md; LK-1022.

### LKF-575 — Límites del fixture de Monitor y descripción de PulseAll

Stage54_4MonitorTests.cpp:268–271 busca una subcadena de llamada, sin comprobar operandos o cantidad; buildExe devuelve una ruta incluso si falla la emisión y el helper de ejecución carece de plazo propio. El comentario sobre única PulseAll omite llamadas anteriores de trabajadores; la llamada es única después de activar Go. Limitaciones de test/documentación, no defecto del compilador o monitor demostrado. Fuente e incertidumbre: paralelo-compilador-197.md; LK-1024.

### LKF-576 — Comentarios históricos contradictorios y cobertura ABI parcial

Stage56C6StructMembersTests.cpp:18–21 describe destructores de struct como rechazados, mientras :401–403 menciona soporte posterior T06-F02. Debe cotejarse el código actual antes de establecer estado vigente. El fixture Clang comprueba dos tamaños y un offset, no toda alineación y ABI descritas en sus comentarios. Limitación documental y de evidencia, no defecto confirmado del compilador; ver paralelo-compilador-201.md y LK-1035.

### LKF-577 — Comentarios históricos desactualizados sobre structs grandes

Stage52GenericCollectionsCompilerFixesTests.cpp:93–108 describe un límite de local struct mayor a 8 bytes, pero :1522–1537 indica que la expectativa cambió a aceptación. No se establece estado actual ni ABI completa a partir de ninguno de los comentarios. Limitación documental, sin prueba ejecutada; ver paralelo-compilador-203.md y LK-1039.

### LKF-578 — Infraestructura de fixtures de formatos pendiente en módulo de test

LinkerFormatsModule.cpp:9–26 declara ausentes constructores independientes COFF, PE32+ y PDB y registra 14 contratos como infraestructura sin usar el registro ejecutable ordinario. Alcance limitado a ese módulo; no acredita defecto del enlazador ni ausencia en otras baterías. Ver paralelo-referencias-137.md y LK-1048.

### LKF-579 — Comentario histórico de arena y posible riesgo del test destructivo

El comentario de TA-AST-014 presenta una fuga como actual, pero AstArena::make contiene destroy_at cuando falla el registro. La limpieza condicional del fixture hace delete de orphan después de salir del ámbito de la arena; si esa rama se alcanzara bajo una regresión, el puntero podría referir a almacenamiento ya liberado y no asignado individualmente. Riesgo estático condicionado, no reproducido; no se afirma fuga vigente del compilador. Ver paralelo-depurador-208.md y LK-1062.

### LKF-580 — Expectativas no fatales seguidas de indexación en pruebas AST

TestContext.h:12–25 acumula el error y continúa. Los casos de AstNodeLifetimeModule y AstArenaModule identificados en informes paralelo-depurador-204/205 acceden a elementos tras expectativas de tamaño no fatales; si una regresión entrega una colección menor, podrían acceder fuera de rango antes de producir un diagnóstico limpio. Riesgo condicionado de infraestructura de pruebas, no fallo observado del compilador. Ver paralelo-referencias-144.md y LK-1069.

- **Ampliación LKF-580:** SsaModelModule.cpp caso 004 también indexa tras una expectativa de tamaño no fatal; el riesgo requiere una regresión que produzca menos elementos y no se ha reproducido. Ver paralelo-depurador-218.md y LK-1069.

- **Ampliación LKF-580:** TargetLayoutValuesModule.cpp casos 014–016 también acceden tras expectativa de tamaño no fatal; riesgo condicionado a una regresión de tamaño, no reproducido. Ver paralelo-depurador-219.md y LK-1069.

- **Ampliación LKF-580:** LanguageIntegrationQualityModule.cpp:117–134 y 284–296 indexa vectores tras expect no fatal; investigar precondiciones y manejo del runner, sin afirmar fallo ejecutado. Ver paralelo-referencias-152.md.


### LKF-581 — Renderer self-hosted de diagnósticos: severidad y escape del código

**Componente:** compilador NativeSharp self-hosted, renderer de diagnósticos; no atribuir al renderer C++. **Estado:** limitaciones observadas por lectura estática; impacto en consumidores y ejecución pendientes de contrastar. **Evidencia:** A:/nsc-devel/selfhost/nsc/Diagnostics/Diagnostics.ns:84–106, SHA-256 178987f61ab461809057b4aac93be7661a44fa176701b9e365de3b8c02a57388; informe A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-224.md y expectativas del lote 223.

La ruta Human forma el prefijo con error y no consulta la severidad almacenada: un warning o note presentado por esa ruta no conserva su severidad. Para código desconocido concatena el argumento sin separador después de unknown diagnostic. Json escapa el argumento, pero concatena Code sin escape entre comillas; el constructor acepta cadenas arbitrarias, de modo que códigos con comillas, barras inversas o controles pueden producir JSON inválido o alterado. El catálogo inspeccionado usa códigos constantes seguros; no se afirma impacto del flujo habitual ni comportamiento del compilador C++.

**Pendiente:** contrastar contrato y productores reales, probar código adversario mediante parser JSON independiente, revisar severidades y mensajes desconocidos y analizar consumidores. Sin compilación ni ejecución. Seguimiento LK-1092.


### LKF-582 — Validación desigual de bytes en cadenas interpoladas

**Componente:** compilador NativeSharp, lexer C++. **Estado:** diferencia observada estáticamente entre rutas; sin ejecución ni conclusión de incumplimiento de especificación. **Evidencia:** A:/nsc-devel/src/Lexer/Lexer.cpp:731–803, :928–990 y :1066–1068, SHA-256 86ead5d1e4e0265340d3db0f3d0e73e887c39267cada1ea9512efd71b645383b; pruebas descritas en paralelo-referencias-153/154.

La ruta de cadenas ordinarias rechaza bytes de control sin escape y secuencias UTF-8 mal formadas. La ruta de cadena interpolada ordinaria copia los demás bytes y puede devolver token válido sin aplicar esas comprobaciones; la variante raw interpolada rechaza ciertos controles pero tampoco muestra validación UTF-8 en la ruta observada. El fixture TL-LEX-091 espera preservar bytes inválidos y controles en la plantilla. No se infiere que el contrato del lenguaje exija el mismo tratamiento ni que todas las rutas posteriores los acepten.

**Pendiente:** contrastar especificación y tratamiento posterior de fragmentos y escapes, cobertura de variantes y consecuencia en diagnósticos, ubicación y reporte; reproducir casos aislados en fase autorizada. Sin compilación ni ejecución. Seguimiento LK-1091.


### LKF-583 — Identificadores limitados a caracteres ASCII en las reglas del lexer

**Componente:** compilador NativeSharp, lexer C++. **Estado:** alcance observado estáticamente; no se establece si es un defecto respecto a la especificación. **Evidencia:** A:/nsc-devel/src/Lexer/Lexer.cpp:18–25, SHA-256 86ead5d1e4e0265340d3db0f3d0e73e887c39267cada1ea9512efd71b645383b; A:/nsc-devel/tests/compiler-suite/host/modules/LexerIdentifierModule.cpp caso 030, SHA-256 3b29b7ca48b661a916714d27a4405cb5213d14b145de61a46a1ae92411345171.

Las funciones observadas aceptan A–Z, a–z y guion bajo para inicio, y además dígitos para continuación. El fixture espera que los bytes UTF-8 de é no formen un identificador. La expectativa no se ha ejecutado y no se extrapola a otras rutas o componentes. **Pendiente:** contrastar especificación y flujo completo, definir soporte deseado y verificar representación de offsets/columnas en pruebas posteriores. Seguimiento LK-1093.

### LKF-584 — Alineación de unión con solo campos de bits

**Componente:** compilador NativeSharp, cálculo de layout. **Estado:** comportamiento observado estáticamente; compatibilidad con ABI externa no demostrada. **Evidencia:** A:/nsc-devel/src/Target/TargetDataLayout.cpp:136–143 y :234–300, SHA-256 3122450527e7f429fbcc78ca3eb53aa2e7b9e8c863895f4b87967e015830a480; informes paralelo-compilador-231 y paralelo-depurador-222.

En una unión formada solo por bitfields de ancho positivo, sin alineación mínima explícita, la ruta observada conserva alineación de agregado 1 aunque registra alineación individual del campo; un miembro ordinario sigue otra ruta y sí puede elevarla. La expectativa del fixture coincide con esa implementación. Esto acota cómo interpretar layout y no demuestra incompatibilidad con clang/MSVC o defecto de ABI.

**Pendiente:** contrastar contrato y ABI vigente, uniones solas, mixtas, arreglos, anidamiento y packing con artefactos de los productores en fase de validación. Sin pruebas ni ejecución. Seguimiento LK-1090.


### LKF-585 — Desfase de catálogos de tokens host y self-host

**Componente:** compilador NativeSharp, definiciones de TokenKind host C++ y self-host. **Estado:** diferencia de fuentes confirmada estáticamente; no se ha demostrado impacto operativo. **Evidencia:** A:/nsc-devel/include/nativesharp/Lexer/TokenKind.h SHA-256 b2c60696f1feadbb6cd90bd4c13dfbc46963e2224dc305c8e9e84fb3592ed749 y A:/nsc-devel/selfhost/nsc/Lexer/TokenKind.ns SHA-256 59e9b55ed8d96e167794e9e037365752e2aaa12088fa5a4f7709beba282a72b6; informe A:/nsc-leak-killer/docs/fase-0/paralelo-depurador-229.md.

El catálogo host contiene 152 entradas y self-host 147; el host añade KwInt128, KwUInt128, QuestionBracket, DotDotEqual y Utf8StringLiteral. Algunas inserciones desplazan ordinales comunes: KwFloat es 24 frente a 22, y DotDot 149 frente a 146. La afirmación del comentario self-host de orden idéntico a nsc0 no coincide con este encabezado host actual. Un intercambio de ordinales crudos sin conversión podría interpretar otro token, pero no se ha constatado que exista tal canal ni fallo de bootstrap.

**Pendiente:** contrastar versiones y consumidores, gramática efectiva, serialización y bootstrap; definir mapa por nombre/versión y validar compatibilidad en fase autorizada. Sin compilación ni ejecución. Seguimiento LK-1094.

### LKF-586 — Escritura de consola del runtime Windows sin comprobar resultado

**Componente:** runtime Windows generado por NativeSharp; no clasificar como limitación exclusiva del compilador. **Evidencia:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp:341–371 y :374–441, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a; lectura parcial paralelo-depurador-231.md.

La ruta generada __nativesharp_console_write trunca la longitud i64 a i32 para WriteFile y no consulta el retorno ni bytes escritos; la escritura del salto de línea tampoco comprueba el resultado. Así, para longitud superior al rango de 32 bits el contador se trunca y esta ruta no informa fallos o escrituras parciales. Alcanzabilidad de tamaños extremos, contratos y manejo en capas superiores no están establecidos. **Pendiente:** contrastar contratos, probar fallos y escrituras parciales en fase autorizada e informar cobertura de stdout/stderr. Sin ejecución.

### LKF-587 — Rutas Windows de entorno y conversión con manejo acotado de fallos

**Componente:** runtime Windows generado por NativeSharp. **Evidencia:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp:1063–1106 y :1139–1189, :1214–1227, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a; lectura parcial paralelo-depurador-231.md. Sin ejecución.

La conversión UTF-8 a UTF-16 trunca longitud i64 a i32 y no verifica los retornos de MultiByteToWideChar antes de construir el terminador. La lectura de variables de entorno genera HeapAlloc seguido de store sin comprobar null y descarta el resultado de la segunda consulta; distingue solo el resultado cero inicial como cadena vacía. El setter descarta el BOOL. Los comentarios señalan como simplificación deliberada colapsar ausencia/error y no exponer fallo del setter; el contrato y la alcanzabilidad deben contrastarse. No se afirma falla reproducida ni impacto habitual.

**Pendiente:** revisar consumidores, errores, longitudes, cambios entre consultas, fallo de reserva y contenido parcial; diseñar pruebas aisladas en fase posterior y registrar cobertura incompleta en LeakKiller. No corresponde al documento exclusivo de limitaciones del compilador.

### LKF-588 — ReadAllText puede declarar bytes no leídos en runtime Windows

**Componente:** runtime Windows generado por NativeSharp. **Evidencia:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp:1531–1582, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a; lectura parcial paralelo-depurador-231.md. Sin ejecución.

La ruta usa el tamaño reportado en la cabecera de string, trunca el conteo a 32 bits para una sola llamada ReadFile y no comprueba resultado ni bytes efectivamente leídos. Si ocurre lectura corta o fallida, o el tamaño excede el conteo representable, la longitud declarada puede superar los bytes inicializados por esa llamada. La ruta tampoco comprueba el resultado de HeapAlloc antes de escribir y convierte fallos de apertura o tamaño a resultado vacío según el código/comentarios. El impacto real, contratos y límites de capas superiores siguen pendientes.

**Pendiente:** contrastar consumidores, fallos, archivo cambiante y tamaños, y diseñar validación aislada posterior de memoria definida y errores. No se atribuye al compilador exclusivamente ni se afirma reproducción.

- **Ampliación LKF-588:** ReadAllBytes en WindowsRuntimeBuilder.cpp:1765–1788 sigue el mismo patrón: conserva tamaño original, trunca a 32 bits y descarta resultado/cantidad de ReadFile. ReadBytes :1832–1848 sí devuelve cantidad, aunque descarta el BOOL; no equiparar sus contratos. Evidencia estática y condiciones en paralelo-depurador-231.md, sin ejecución.

### LKF-589 — Conversión de ruta sin comprobación del tamaño UTF-8

**Componente:** runtime Windows generado por NativeSharp. **Evidencia:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp:2628–2673, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a; lectura parcial paralelo-depurador-231.md. Sin ejecución.

La ruta de GetFullPath descarta el resultado de la segunda consulta de ruta, no comprueba el resultado de WideCharToMultiByte y resta uno del tamaño convertido después de extenderlo a i64. Si la consulta de conversión devuelve cero, esa resta produce el valor máximo de i64; el resultado de HeapAlloc tampoco se comprueba antes del store. Condición y efecto observable requieren contrastar contratos y validación futura, sin asumir que ocurra normalmente.

### LKF-590 — Escritura de texto en archivo con fallos no expuestos

**Componente:** runtime Windows generado por NativeSharp. **Evidencia:** WindowsRuntimeBuilder.cpp:2436–2473 y ruta de append en informe paralelo-depurador-231.md; misma huella y sin ejecución. WriteAllText descarta fallos de apertura según comentario, trunca longitud de contenido a i32, no consulta resultado ni bytes escritos por WriteFile y retorna void. AppendAllText tiene una ruta similar según lectura parcial. Para longitudes grandes o escrituras parciales puede no escribirse todo el contenido sin comunicación por esa ruta; contrastar contratos y callers. No extrapolar a WriteAllBytes, que tiene comprobaciones diferentes.


### LKF-591 — Referencias locales con tipo inferido rechazadas en esta ruta

**Componente:** compilador NativeSharp, parser de sentencias. **Estado:** diagnóstico explícito observado estáticamente, no reproducción ejecutada. **Evidencia:** A:/nsc-devel/src/Parser/StatementParser.cpp:1170–1220, SHA-256 edd7f8fe33868a80a0f0ae1c49ff0035fe05b6de896a05d04b0e2ee459e8fdef.

Cuando esta ruta reconoce una referencia local y después encuentra var, emite InvalidType indicando que requiere tipo de elemento explícito. El comentario también describe owned var/borrowed var como fuera del alcance original, pero esa afirmación histórica requiere rastrear otras rutas y sintaxis antes de convertirla en limitación general.

**Pendiente:** contrastar coordinador alterno, documentación y semántica actual y verificar ejemplos en fase autorizada; no extrapolar a todas las referencias o parámetros. Seguimiento LK-1101.

### LKF-592 — Métricas Windows leídas sin comprobar éxito de la API

**Componente:** runtime Windows generado por NativeSharp. **Evidencia:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp:4838–4852 y :4876–4918, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a; lectura parcial paralelo-depurador-231.md. Sin ejecución.

Las rutas de working set y tiempos asignan buffers en pila, descartan el retorno de K32GetProcessMemoryInfo/GetProcessTimes y leen el campo de salida. Solo cb se inicializa en la primera; los slots de tiempo no se inicializan en la ruta mostrada. Si una API falla sin escribir su salida, la lectura puede utilizar datos no inicializados. El caso de fallo y el impacto en consumidores deben verificarse; no se afirma reproducción ni frecuencia. El informe 231 identifica un patrón análogo para GlobalMemoryStatusEx que queda pendiente de corroboración detallada.

### LKF-593 — Resultados de sincronización Windows no diferenciados en rutas concretas

**Componente:** runtime Windows generado por NativeSharp. **Evidencia:** WindowsRuntimeBuilder.cpp rutas de mutex, semáforo y eventos examinadas en paralelo-depurador-231.md, líneas 4001–4960; misma huella, sin ejecución. Las rutas descritas descartan algunos resultados BOOL o colapsan fallo y timeout; el contrato concreto y efecto en consumidores requieren cotejo detallado. No extrapolar a todas las operaciones ni afirmar deadlock o falla ejecutada.

- **Ampliación LKF-589:** el helper de conversión de nombres de directorio en WindowsRuntimeBuilder.cpp:3241–3265 también extiende el tamaño WideCharToMultiByte y resta 1 sin validar cero, y no comprueba HeapAlloc ni la segunda conversión. Condicionado a fallos de API, sin reproducción; ver paralelo-depurador-231.md.

- **Ampliación LKF-592:** WindowsRuntimeBuilder.cpp:5103–5112 inicializa solo dwLength, descarta BOOL de GlobalMemoryStatusEx y carga total físico; otras rutas del mismo grupo hacen lo análogo. Si la API falla sin escribir, el valor leído no está garantizado. Evidencia estática, sin ejecución.

### LKF-594 — Hook de runtime de debug expone solo cantidad de frames

**Componente:** runtime Windows generado, hook de debug; no extrapolar al depurador externo. **Evidencia:** A:/nsc-devel/src/Runtime/WindowsRuntimeBuilder.cpp:5208–5239, SHA-256 f8e6b4cd6349768fc11ae3ef84808f4059ae24bb314235b6816d06259134a32a; paralelo-depurador-231.md. La ruta captura hasta 62 direcciones en buffer local y retorna solo el conteo; descarta direcciones al volver. Por sí sola no suministra direcciones simbolizables ni grabación de ejecución. Es un alcance deliberado descrito en código, no falta demostrada en el depurador NativeSharp. Sin ejecución.


### LKF-595 — Restricciones explícitas en parser de miembros y funciones

**Componente:** compilador NativeSharp, parser de miembros. **Estado:** diagnósticos observados estáticamente en rutas concretas; no se ejecutaron casos ni se afirma que todas las formas alternativas sean imposibles. **Evidencia:** A:/nsc-devel/src/Parser/MemberParser.cpp:463–469, :2311–2318, :2378–2383, :3153–3163 y :3236–3245, SHA-256 3dd05a060b46875e6271c6b9ae7b3d2bb01ba42de18f3d91257f782fec30aff5; informe paralelo-compilador-237.md.

En las rutas indicadas se diagnostican parámetros genéricos propios de un tipo anidado, propiedades e indexadores sin getter, restricciones genéricas de función global y calificadores owned/borrowed en tipo de error throws. La restricción de función global aporta evidencia actual a la referencia histórica LKF-082; el rechazo de getter también se relaciona con la validación de superficie pública ya documentada. No se extrapola a genéricos no anidados, getters válidos u otros sistemas de excepción.

**Pendiente:** cotejar especificación, productores alternativos, diagnósticos y etapa semántica; preservar tipos y efectos en LeakKiller y verificar ejemplos en fase autorizada. Seguimiento LK-1107.

- **Contraste adicional LKF-077:** MemberParser.cpp:2644–2653, SHA-256 3dd05a060b46875e6271c6b9ae7b3d2bb01ba42de18f3d91257f782fec30aff5, emite diagnóstico para operator ++/-- de usuario en esta ruta; no impide operadores incorporados. :2592–2604 también identifica &&, || y ?? como intrínsecos no sobrecargables en esta ruta. Sin ejecución; cotejar especificación y otros caminos. Informe paralelo-compilador-237.

### LKF-596 — Limpieza identifica artefactos únicamente por extensión

**Componente:** herramienta auxiliar de ejemplos, no compilador. **Evidencia:** A:/nsc-devel/tools/clean_project.py:10–22, SHA-256 ad3aa26362300516face76b24c93b381774c33030cf5599a12927c1e6d769773. En el directorio seleccionado (por defecto el actual), el script elimina archivos inmediatos cuya extensión coincida con su lista, incluidos .h y .pdb, sin comprobar si son generados o propiedad del proyecto. Riesgo condicionado a ejecutarlo en un directorio con archivos relevantes; no se ejecutó y no se afirma pérdida real. **Pendiente:** valorar manifiesto de artefactos, previsualización y fixture aislado; proteger evidencias y símbolos. Seguimiento LK-1114.

### LKF-597 — Alcance acotado del inventario documental NativeSharp 3

**Componente:** herramienta auxiliar de inventario, no compilador. **Evidencia:** A:/nsc-devel/tools/native_sharp_3_source_inventory.py:117–146, :181–219, SHA-256 17bd929e2603064ab2d3ae97030fa3f54ce60b595edee5a4af35a343d7f17d0c. En repositorios Git selecciona Markdown rastreado y filtra por rutas o contenido; omite raíces ausentes y ciertas carpetas. El recorrido alterno aplica si no detecta repositorio. Es un inventario de documentos para su finalidad, no prueba cobertura de archivos no versionados, código, binarios o PDB. No se ejecutó ni se afirma fallo funcional. **Pendiente:** declarar explícitamente cobertura y complementar mecanismos si LeakKiller requiere universo distinto; LK-1119.

### LKF-598 — Fallo condicionado al restaurar inyección de pruebas

**Componente:** soporte de pruebas de escritura, no compilador ni hook de producto. **Evidencia:** A:/nsc-devel/tests/compiler-suite/host/support/WriterSeedFailure.h:73–99, SHA-256 1091bd3d8b32cbba8be9a7a59c8a2372d164266168cba34e2bcaf1f3469fbd4d. El destructor ignora el resultado de restore; si falla la primera llamada VirtualProtect de restauración, devuelve false antes de reponer el puntero IAT y limpiar el estado. Si falla la segunda, ya repuso el puntero pero devuelve false. Riesgo condicionado al fallo Win32 durante ese fixture; no se ejecutó y no se afirma que haya ocurrido. **Pendiente:** prueba aislada futura con fallo inyectado y mecanismo que haga visible la restauración fallida; LK-1120.

### LKF-599 — Conteo impreso de autopruebas de conformidad inconsistente

**Componente:** herramienta auxiliar de conformidad. **Evidencia:** A:/nsc-devel/tools/ns3_conformance.py:256–270, SHA-256 0436df7ca9bb822f41e68aaccc32a9c4d8d658091eef73805e7ceb005ee455f3. La lista contiene siete entradas y el mensaje final afirma ocho checks; puede existir otra interpretación de checks, pero el bucle ejecuta siete casos enumerados. No se ejecutó la herramienta. **Pendiente:** alinear mensaje y conteo y definir unidad medida; LK-1124.

### LKF-600 — Harness de conformidad sin límite de espera de proceso

**Componente:** herramienta auxiliar de conformidad, no compilador. **Evidencia:** A:/nsc-devel/tools/ns3_conformance.py:161–173, :212–231, SHA-256 0436df7ca9bb822f41e68aaccc32a9c4d8d658091eef73805e7ceb005ee455f3. subprocess.run no especifica timeout en esta ruta; un caso cuyo proceso no termina puede mantener el harness esperando indefinidamente salvo intervención externa. No se ejecutó ni se verificaron supervisores externos. **Pendiente:** política de timeout, proceso descendiente y reporte incompleto; LK-1124.

### LKF-601 — Reporte de matriz incorpora validación histórica fija

**Componente:** generador auxiliar de matriz de pruebas. **Evidencia:** A:/nsc-devel/tools/generate_test_destination_matrix.py:304–315 y 428–446, SHA-256 02594d13ee017d034bbc76ee66b6134f2923347ca59a4b34f9d3dbbf8bb8b9a2. El texto generado afirma siempre 684 owners resueltos, cero duplicados y dos generaciones con hash igual, aunque las métricas se calculan en otra sección y el script mostrado no realiza la segunda generación. Una ejecución posterior distinta podría producir un reporte internamente engañoso; no se ejecutó en esta revisión ni se afirma que la ejecución histórica fuera falsa. **Pendiente:** usar resultados comprobados y separar historial; LK-1135.

### LKF-602 — Resolución ambigua de owner en matriz

**Componente:** generador auxiliar de matriz de pruebas. **Evidencia:** A:/nsc-devel/tools/generate_test_destination_matrix.py:95–128, SHA-256 02594d13ee017d034bbc76ee66b6134f2923347ca59a4b34f9d3dbbf8bb8b9a2. Si varios archivos comparten nombre y ningún argumento resuelve uno, escoge el primero ordenado; no expresa ambigüedad en ese retorno. Clasificación de literales y expectativas usa expresiones regulares, como heurística. No se demuestra colisión real ni se ejecutó la herramienta. **Pendiente:** conservar candidatos y evidencia, marcar resolución ambigua y confirmar manualmente; LK-1135.

### LKF-603 — Catálogo omite silenciosamente filas con formato inesperado

**Componente:** herramienta documental auxiliar. **Evidencia:** A:/nsc-devel/tools/native_sharp_3_catalog.py:145–173, :592–598, SHA-256 32fece3776223d45deef0554636e9137db2bac2300bab752c34a245c0e52d7c3. parse_catalogue omite líneas cuya división no tenga seis celdas; si otras filas son válidas, no diagnostica la omitida. Además, --stdout actualiza el resumen del documento maestro antes de imprimir; no asumir que es operación de lectura. Sin ejecución ni caso real observado. **Pendiente:** diagnosticar filas malformadas y documentar efectos; LK-1138.

### LKF-604 — Declaración de dos campos separada por coma en ruta host

**Componente:** compilador NativeSharp, parser host. **Evidencia estática:** A:/nsc-devel/src/Parser/MemberParser.cpp:1538–1575, :1708–1724, :1774–1780, :1805–1864, SHA-256 3dd05a060b46875e6271c6b9ae7b3d2bb01ba42de18f3d91257f782fec30aff5; A:/nsc-devel/src/Parser/ParserContext.cpp:198–211, SHA-256 3d602f546102fdbce3b4f461f45e204a77e3dec95ec11fff3eca950119f207c3; paralelo-referencias-185/186. Para class C { int a, b; }, la ruta analizada consume un nombre y espera punto y coma al encontrar la coma; se infiere diagnóstico ExpectedToken en ella. Evidencia estática, sin ejecución; no extrapolar a locales, otras rutas o declaraciones sin contrastarlas. **Pendiente:** cotejar especificación, recuperación y rutas alternativas; LK-1139.

### LKF-605 — Escape JSON incompleto del controlador de pruebas host

**Componente:** controlador de pruebas host, no compilador. **Evidencia:** A:/nsc-devel/tests/compiler-suite/host/src/main.cpp:90–119, SHA-256 8ca320df2017c0d304ea57a940c4f8b647f228766ee66a241b1e4a9c20d55f02; paralelo-referencias-188. Escapa comillas, barra invertida, LF, CR y tab, pero copia otros controles U+0000–U+001F; si llegan a un campo, el JSON emitido puede ser inválido. No se comprobó alcanzabilidad real ni se ejecutó. **Pendiente:** contrastar productores/consumidores y validar escapes completos; LK-1142.

### LKF-606 — Comentario histórico y fixture discrepan sobre throw escrito

**Componente:** documentación dentro de prueba NativeSharp3T07ErrorTests.cpp, no comportamiento de compilador comprobado. **Evidencia:** A:/nsc-devel/tests/unit/NativeSharp3T07ErrorTests.cpp:708–726, SHA-256 1bec53bd780bfd1a46b964685f7a9fab64210c3d86e3a31d056c9cf5b2c49379. Comentario dice que un throw escrito no se comprueba; el comentario siguiente y el caso UnhandledThrow esperan rechazo cuando no se captura ni declara. No se ejecutó y no se ha contrastado todavía la fuente propietaria. **Pendiente:** verificar semántica vigente y corregir texto histórico en tarea autorizada; LK-1147.

### LKF-607 — Comentario de alineación de NS_PROLOGO requiere corrección

**Componente:** cabecera auxiliar NSASM, no compilador ni linker. **Evidencia estática:** A:/nsasm-devel/include/nsasm.inc:15–40, SHA-256 ac864459ba6b2cb69f0c09bc26b74045b095e35f7cd5a251339d77488f5e6cb1. El comentario de NS_PROLOGO pide reserva múltiplo de 16 más 8; bajo la condición de entrada y CALL indicadas en la propia cabecera, push rbp deja RSP alineado y restar ese valor lo desalinearía antes de otro CALL. Contrastar el contrato exacto, llamadas y directivas unwind antes de modificar; no se ensambló ni ejecutó ningún caso.

### LKF-608 — Lectura parcial no distingue error en nsdis/nsdump

**Componente:** herramientas auxiliares NSASM. **Evidencia:** A:/nsasm-devel/tools/nsdis/main.cpp:15–29, SHA-256 fc358e6801c691f03c2b74da9b5a8ad2c15b4a930772b46250d52384c4327400; A:/nsasm-devel/tools/nsdump/main.cpp:17–31, SHA-256 119973ab6e89e90e60cf52d034761f90d416f6870f9c482ecdbfdf3a271c6b93. Ambas rutas dejan de leer al devolver fread cero, cierran archivo y marcan ok sin comprobar ferror; si ocurre un error de lectura, pueden entregar bytes parciales como lectura exitosa. El parser posterior puede rechazarlos o no; no se ejecutó ni se observó fallo real.

### LKF-609 — Catálogos SDK declaran versiones ABI distintas

**Componente:** catálogos SDK de NSC y NSLink; no se atribuye falla de compilador ni enlace. **Evidencia:** A:/nsc-devel/sdk/win64/nativesharp-runtime.nsimports SHA-256 e77bd24dc0711f8aee60e003abf3f77074ff8ed02f054ab4a9a84bd3ec926a9b y A:/nslink-devel/sdk/win64/nativesharp-runtime.nsimports SHA-256 58e6c5d6b0ec19bed6a8b1d8e621c825fdcc47b488da476e7c0cd7bc570c3d90; informe revision-sdk-nsimports.md. Los comentarios indican ABI 1.0/set 2 y ABI 1.1/set 3 respectivamente aunque las declaraciones coinciden en 78 símbolos; el encabezado de contrato examinado menciona ABI 1.2/set 4 en otra revisión. Son datos documentales de cortes distintos: no demuestran incompatibilidad ni imports efectivos. **Pendiente:** contrastar productor, orden y versiones reales antes de determinar consistencia.

### LKF-610 — Registro opcional de NSLink incluye argumentos sin redacción local

**Componente:** interfaz de NSLink, modo compatible. **Evidencia:** A:/nslink-devel/tools/nslink/Main.cpp:384–425, SHA-256 822403aa293ce092118ac6baf46f41de528d171f68987c935d44ccfbb4d2ee15; lectura-nslink-main-2026-11-10.md. La ruta de log activada por configuración abre con truncamiento y escribe argumentos y rutas sin redacción local. La exposición depende de la configuración, contenido y permisos; no se activó ni se observó filtración. **Pendiente:** contrastar política de datos, activación y permisos, y preservar controles de almacenamiento.

### LKF-611 — Parseo nativo NSLink delega opciones incompletas como rutas

**Componente:** interfaz nativa NSLink. **Evidencia:** A:/nslink-devel/tools/nslink/Main.cpp:571–648, SHA-256 822403aa293ce092118ac6baf46f41de528d171f68987c935d44ccfbb4d2ee15. En la ruta --link, argumento desconocido o una opción con operando ausente al final puede caer en la lista de paths; el comportamiento y diagnóstico posterior dependen del pipeline. No extrapolar al modo compatible ni afirmar que se haya generado un archivo. **Pendiente:** contrastar contrato y diagnósticos por caso, sin ejecución en esta fase.

### LKF-612 — Probes NSASM no ejercitan cinco directivas anunciadas

**Componente:** corpus de probes ml64, infraestructura de pruebas, no soporte de NSASM demostrado. **Evidencia:** A:/nsasm-devel/probes/ml64/p_ALLOCSTACK.asm, p_SETFRAME.asm, p_PUSHFRAME.asm, p_SAVEREG.asm y p_SAVEXMM128.asm; hashes y líneas en auditoria-nsasm-probes-candidatos.md y auditoria-nsasm-probes-huellas.md. Sus comentarios anuncian esas directivas, pero los cuerpos revisados no las invocan; el runner observado llama ml64. Por ello una eventual aceptación de esos cinco archivos no demuestra cobertura de las directivas. No se ejecutaron y podrían existir otros tests válidos. **Pendiente:** buscar cobertura real y diseñar fixtures dirigidos sin inferir soporte o rechazo del ensamblador.


- **Contraste LKF-612:** existen fixtures oracle que usan ALLOCSTACK/SETFRAME y pruebas de API para las cinco operaciones; src/model/Assembler.cpp las despacha. Esto acota el hallazgo a los cinco probes ml64 y no demuestra ejecución o cobertura textual completa de las demás variantes. Ver contraste-lkf-612-unwind.md.

- **Contraste adicional LKF-612:** nsasm-devel/docs/probe_results_2026-08-28.txt SHA-256 219082bae6438fbfd6ae1ed95f5e46d817b651cac2d42dea8637ed14f78dc04b etiqueta cinco directivas como OK en resultado histórico. Las fuentes actuales homónimas no las contienen, pero no se demostró que fueran los insumos históricos; el rótulo por sí solo no acredita probar cada directiva. Ver lectura-probe-results-lkf-612.md.

