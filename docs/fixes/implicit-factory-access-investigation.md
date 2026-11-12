# Accesibilidad de constructor implícito a través de una fábrica binaria

El ejemplo de conteo reentrante de `WeakArc` usa una clase interna del
consumidor sin constructor declarado. Con Core binario, el análisis semántico
lo acepta pero la emisión O0/O2 falla en la plantilla de Core por constructor
implícito inaccesible. Una copia separada que añade constructor público vacío
compila y devuelve 42 en ambos niveles. El débil estático permanece vivo al
consultarlo durante el destructor y no se lee la carga destruida. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/atomic-weak-count-binary-20261112/run-20261112T054625107680Z/results.json`.
No acredita concurrencia, descarga del módulo ni liberación física.

Una matriz independiente sin ejecutar consumidores cruza tipo interno/público,
constructor implícito/público explícito y construcción directa/MakeRc/MakeArc.
Los 12 casos pasan semántica. Solo fallan en emisión las dos fábricas con tipo
interno y constructor implícito; construcción directa, tipo público o
constructor explícito emiten. Fuentes, comandos y hashes estables:
`A:/nsc-fixes-agente-2-evidencia/implicit-factory-access-20261112/run-20261112T054850795200Z/results.json`.

Por lectura, `hasPublicDefaultConstructor` considera satisfecha la restricción
`new()` en una clase que no declara constructores, sin comprobar visibilidad
de la clase; si hay constructores exige uno público sin parámetros. La ruta de
construcción concreta llama a `implicitDefaultAccessible`, que exige tipo
público o mismo proyecto que el callable. El diagnóstico aparece en la plantilla
reconstruida de Core. Esto demuestra una discrepancia entre las comprobaciones;
la hipótesis de que la segunda usa el proyecto de definición de la plantilla
requiere confirmar el contexto real durante la especialización.

No se ha cambiado el contrato. Una solución debe comprobar el sitio y contexto
de instanciación sin permitir acceso a tipos o constructores privados desde
llamadores ajenos. Antes de implementarla hacen falta controles de tipo interno
del consumidor, tipo interno de un tercer módulo, tipos anidados privados y
constructores privado/protegido/implícito, con fábrica local y binaria, así
como solicitudes repetidas desde proyectos distintos. La matriz actual no
resuelve por sí sola cuál es la política de acceso entre módulos.

Una matriz posterior usa una fábrica genérica local con retorno `owned T` y un
productor binario que declara `Hidden` interno, `Open` público, `PrivateCtor`
con constructor privado y `ProtectedCtor` con constructor protegido. La
fábrica local con tipo interno y el `new` directo emiten; las fábricas Core
para ese mismo tipo interno siguen fallando. Los constructores privado y
protegido locales se rechazan por `new()`, así como el protegido precompilado;
el tipo interno ajeno no se resuelve. En cambio, `MakeRc<PrivateCtor>()` con
el tipo público precompilado pasó semántica y emisión. No se ejecutaron
consumidores. Registro:
`A:/nsc-fixes-agente-2-evidencia/implicit-factory-context-owned-20261112/run-20261112T055507558913Z/results.json`.
La primera versión del control local usó un retorno sin ownership y falló por
esa razón previa; se conserva sin contarla como prueba de accesibilidad.

El volcado ABI de ese productor muestra el tipo público `PrivateCtor`, pero no
su constructor privado; sí muestra el constructor protegido de
`ProtectedCtor` y el constructor público sintético de `Open`. Registro:
`A:/nsc-fixes-agente-2-evidencia/implicit-factory-context-20261112/producer-abi.log`.
La ausencia de miembros hace que el chequeo actual de `new()` considere la
clase implícita, coherente con el resultado observado. El ABI no demuestra qué
constructor se ejecutaría; no se ejecutó el programa.

La estructura de metadata de tipo examinada no contiene una bandera específica
de constructor privado y el catálogo público omite esos miembros. El productor
actual agrega un miembro sintético público cuando generó un constructor
implícito. Rechazar de forma conservadora un tipo precompilado sin constructor
público publicado podría evitar asumir que uno privado es implícito, pero los
artefactos históricos podrían carecer también de ese miembro y el consumidor
contiene una ruta de compatibilidad para clases precompiladas sin campos y sin
constructor publicado. Deben compararse esos artefactos antes de modificar la
regla. Sin un contrato adicional, ausencia de entrada significa desconocido,
no prueba de constructor privado ni de constructor implícito accesible.

La reconstrucción solo materializa constructores exportables con símbolo
físico (`ContractMaterialization.cpp:198–205`). La construcción de la metadata
actual agrega el constructor sintético público cuando existe su función MIR;
eso explica el miembro publicado para `Open`. Por otra parte, la ruta de
constructores implícitos permite reconstruir una clase precompilada sin
constructor publicado si no tiene almacenamiento de instancia, precisamente
para compatibilidad con superficies reducidas.

`Stage46NativeLibrariesTests.cpp` contiene un control de productores actuales
con clases públicas implícitas y campos inicializados o cero, y sus
consumidores; genera sus artefactos durante el test. Por lectura no constituye
una prueba de un artefacto histórico cuyo catálogo omita el constructor. El
control dinámico de esa prueba queda fuera del alcance de esta campaña.

Una posible guarda estrecha en la restricción `new()` rechazaría como
desconocida una clase precompilada sin constructor accesible publicado, sin
alterar construcción fuente. Evitaría la suposición observada para
`PrivateCtor`, pero podría rechazar binarios anteriores válidos sin ese miembro
y no corrige por sí sola la construcción directa ni la discrepancia de
accesibilidad de `T` interno en la fábrica Core. No se ha aplicado: falta
contrastar un artefacto histórico real y decidir el comportamiento ante
metadata insuficiente. No se ha demostrado que la ausencia signifique que el
constructor es privado.

Se preparó un candidato acotado en `hasPublicDefaultConstructor`: para clases
precompiladas sin ningún constructor publicado, `new()` deja de suponerse
satisfecho; se mantiene la regla existente de clases fuente y la de structs.
Si existe un constructor público sin parámetros publicado, sigue admitiéndose.
Esto no revela constructores privados ni demuestra que toda metadata antigua
publique implícitos: un binario antiguo válido sin esa entrada también será
rechazado conservadoramente. No cambia el formato de metadata y no pretende
resolver el caso distinto de un tipo interno fuente con constructor implícito
en la plantilla de Core.

La prueba dirigida preparada crea una biblioteca NativeSharp estática actual
con constructores privado, protegido, público e implícito; contrasta los casos
negativos con clase fuente interna, fábrica genérica local, construcción
directa y fábricas Rc/Arc con Core binario. No ejecuta consumidores, exige el
diagnóstico de restricción o visibilidad y verifica los artefactos de productor
y positivos. También distingue Core no configurado de configuración inválida
y comprueba hashes. El candidato y esta prueba aún no se han compilado ni
validado; compatibilidad con artefactos históricos permanece desconocida.

Un sondeo adicional publicó cuatro plantillas genéricas con constructor
implícito, público, privado o protegido. Con biblioteca binaria, las dos
primeras pasaron semántica y emisión; la privada pasó semántica pero la
emisión falló por falta de constructor viable; la protegida fue rechazada por
la restricción en semántica. Con `ProjectReference` las cuatro se detuvieron
antes al resolver visibilidad de la plantilla, así que no comparan la misma
fase. No se ejecutaron consumidores. Registro:
`A:/nsc-fixes-agente-2-evidencia/precompiled-generic-ctor-20261112/run-20261112T060229464664Z/results.json`.
Por lectura, unidades de plantillas precompiladas pueden materializarse con
`isPrecompiled=false`; el candidato limitado a esa marca no certifica la
restricción semántica de especializaciones genéricas. La barrera de emisión
observada en este ejemplo no prueba todos los casos ni depende del candidato.
La guarda candidata consulta `isPrecompiled`; no incluye una exclusión
estructural explícita de especializaciones genéricas que conserven esa marca.

La regresión preparada se contrastó con el compilador previo: su única falla
fue que `PrivateCtor` binario pasó el análisis, donde se esperaba rechazo por
`new()`. Los demás controles de productor, Core y accesibilidad pasaron.
Registro: `A:/nsc-fixes-agente-2-evidencia/implicit-factory-context-20261112/regression-baseline.log`.
Ese resultado es anterior al parche, no valida la corrección.
