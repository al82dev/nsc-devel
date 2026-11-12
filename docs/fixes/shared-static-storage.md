# Almacenamiento estático de Rc y WeakRc

El usuario eligió prohibir el almacenamiento estático compartido de `Rc` y
`WeakRc`, incluso dentro de otros tipos. Una reproducción anterior guardaba
`WeakRc` en un campo estático y lo usaba desde un método iniciado por `Thread`;
el análisis la aceptó (`public-local-thread-negative.log`). No se ejecutó esa
variante con contadores no atómicos.

El modelo de miembros examina el tipo resuelto y sigue campos, arreglos,
tuplas, referencias tipadas, clases base y derivados conocidos. La búsqueda
comprueba la plantilla y la identidad de especialización al reconocer las
familias locales. Un índice inverso de herencia evita volver a descubrir todas
las relaciones al procesar cada campo. El análisis de miembros rechaza campos
estáticos afectados, el de namespace rechaza estado global y el de cuerpos
rechaza locales estáticos. Los tipos atómicos siguen admitidos cuando su
contenido no incluye una referencia local.

Las reproducciones anteriores aceptaban fuertes y débiles, contenedores,
arreglos, locales estáticos y estado de namespace. También aceptaban una base
o interfaz estática capaz de recibir una clase derivada con `WeakRc`; las
salidas anteriores están en `public-tuple-borrow-<caso>-Rc.log` bajo
`A:/nsc-fixes-evidencia`. La prueba autocontenida
`nsc.compiler.shared-static-semantics` pasó sus 19 casos: verifica esos
destinos, tipos genéricos, arreglos fijos y punteros, y conserva controles de
`Arc`, uso local, ciclo, clase hermana y `Rc` de otro namespace
(`shared-static-ctest.log`). El fixture declara tipos de prueba con los mismos
nombres de Core; no demuestra por sí solo el SDK público.

Con las fuentes públicas en curso se rechazaron once reproducciones y pasaron
en O0/O2 los controles de `Rc` local, `WeakArc` estático, tipo ajeno y conteo
reentrante. Para este último la prueba usa un observador `unsafe` estrictamente
local al test, pues la referencia débil real ya no puede ser un campo estático.
Una prueba con productor y consumidor NativeSharp distintos rechazó los cinco
casos afectados y aceptó los controles local y atómico
(`shared-static-project-reference.log`). El análisis semántico del proyecto
Core aislado pasó (`shared-static-core-check.log`) y la prueba de contienda
`Arc`/`WeakArc` desde fuentes pasó en O0/O2 (`public-atomic-race-ctest.log`).
La regresión entre proyectos se amplió con contenedor genérico, `Arc` que
contiene `WeakRc`, herencia directa, campo de tipo base e interfaz con derivado
conocido. Las cinco formas fueron rechazadas y los controles anteriores siguieron
pasando (`shared-static-cross-project-expanded.log`). Los dos proyectos se
analizaron desde fuentes mediante `ProjectReference`; no se probó metadata de
bibliotecas precompiladas ni ejecución de esos casos.
La prueba quedó registrada como `nsc.sdk.shared-static-project-reference`; se
actualizó la configuración incremental y esa selección de CTest pasó.

La selección entre proyectos se amplió con un arreglo fijo de `Rc`, un puntero
a `WeakRc` y un campo estático genérico especializado con `WeakRc`. Las tres
formas se rechazaron y los controles local y atómico siguieron aceptándose
(`shared-static-cross-extra.log`). Usa las fuentes públicas de la copia mediante
`ProjectReference`; no demuestra la misma regla con metadata precompilada.

Una prueba posterior usa una biblioteca NativeSharp mínima precompilada y
`LibraryReference`, con declaraciones de prueba de las cuatro familias en el
namespace de Core. El productor se emitió con `--emit-project-oop-lib -O0` y
el consumidor se comprobó con `--check-project-method-bodies`; los siete casos
prohibidos (Rc y WeakRc directos, contenedores con campo público y privado,
derivado, base con derivado conocido y arreglo) se rechazaron con el diagnóstico
esperado. `WeakArc` estático y un `Rc` de otro namespace se aceptaron.
Comandos y diagnósticos: `A:/nsc-fixes-evidencia/shared-static-binary-current.log`;
fuente reproducible: `tests/integration/shared_static_binary.py`. La generación
de biblioteca produjo automáticamente un encabezado, que no se utilizó. La
prueba examina solo semántica y metadata del fixture, no el SDK público binario,
la ejecución ni derivados cargados posteriormente. El compilador informó hash
`bc4d89b727068dcb` y los temporales estuvieron en A.
La prueba quedó registrada como `nsc.sdk.shared-static-binary-reference`; tras
actualizar la configuración existente, esa selección de CTest pasó 1/1
(`A:/nsc-fixes-evidencia/shared-static-binary-ctest.log`).

Una prueba adicional utiliza el SDK público binario aislado
`A:/nsc-fixes-evidencia/generic-owner-sdk-current/NativeSharpCore.lib` y su
metadata, junto con `A:/nsc-fixes-evidencia/build/bin/nsc.exe`. El análisis
rechazó `Rc` y `WeakRc` estáticos, un contenedor de `Rc`, un arreglo de
`WeakRc` y un `Arc` estático que contiene `WeakRc`; aceptó un `Rc` local y
los controles estáticos `Arc`/`WeakArc`. Los siete casos pasaron en
`shared-static-real-sdk-regression.log`, con el comando reproducible
`A:/nsc-fixes-evidencia/shared-static-real-sdk-regression.cmd`. La prueba
`tests/integration/shared_static_real_sdk.py` se ejecutó directamente y aún
no está registrada en CMake, que contiene cambios ajenos. El compilador usado
se construyó con cambios ajenos sin registrar; esta verificación es semántica,
no ejecuta carreras ni acredita clases cargadas dinámicamente o la revisión
integrada con la laptop.

La selección se amplió a 16 casos: 14 rechazos y dos controles aceptados. La
biblioteca productora añade contenedores genéricos y anidados; una segunda
biblioteca precompilada referencia a la primera y contiene un campo privado
que alcanza `WeakRc`. El consumidor rechazó ese campo transitivo, un wrapper
local sobre el tipo binario, arreglos fijos, punteros y una especialización de
campo estático genérico. Ambas bibliotecas se generaron y los 16 controles
pasaron; la selección CTest volvió a pasar 1/1. El archivo
`shared-static-binary-current.log` conserva los comandos y diagnósticos de esta
versión ampliada, en `A:/nsc-fixes-evidencia`. Se sigue usando un fixture de
tipos mínimos y no se demuestra el consumo del SDK público binario ni la
visibilidad de derivados cargados después de compilar el consumidor.

La regla es conservadora: puede rechazar un estático que solo se usa en un
hilo y una base o interfaz por tener un derivado afectado conocido. Recorre
solo los tipos, miembros y relaciones presentes en la compilación; no se ha
acreditado información suficiente sobre campos ocultos de bibliotecas
precompiladas ni clases cargadas posteriormente. La identidad de Core se
contrasta con la plantilla resuelta y su namespace; no existe una verificación
criptográfica del proveedor. El código `unsafe` que convierte un puntero a
entero puede ocultar la procedencia al análisis y no queda cubierto. Por tanto,
estas pruebas no certifican la restricción para todas las cargas binarias ni
el contrato completo de concurrencia de las cuatro familias.

La prueba pública adicional rechazó un campo estático de tupla que contiene
`WeakRc` y otro con `Rc` anidado en un wrapper, como exige la decisión. El
control de tupla estática con `WeakArc` pasó el análisis semántico pero falló
al emitir LLVM en O0: `Stage 40 cannot map named value type to LLVM carrier`.
Los controles con `(int, long)` y con una estructura ordinaria mostraron el
mismo diagnóstico en O0, por lo que el problema observado no es específico de
`WeakArc`. Evidencia: `static-tuple-atomic-before.log`,
`static-tuple-value-before.log` y `static-tuple-scalar-before.log` en
`A:/nsc-fixes-evidencia`. El diagnóstico proviene de `LlvmTypeMapping.cpp` cuando
no se encuentra el layout; aún no se ha demostrado la causa de esa ausencia,
comparado con la base, ni verificado O2. Las regresiones de emisión conservan
su expectativa de éxito y permanecen fallidas; no se modificó el backend.

El control `static_tuple_scalar_with_local` añade a una función un local del
mismo tipo `(int, long)` y ejecuta 42 en O0/O2
(`static-tuple-use-control.log`). En el código, `importTupleLayouts` descubre
actualmente tipos de retorno, parámetros, capturas y nodos de funciones HIR;
no visita las declaraciones de campos estáticos que `buildStaticFields` añade
después. La hipótesis causal es que esos campos no incorporan su layout cuando
no hay un uso adicional en una función. El ajuste candidato se limitaría a
`importTupleLayouts` en `src/Frontend/MIR/MirLayoutImporter.cpp`: recoger los
tipos de tupla de campos estáticos con layout calculable y reutilizar la ruta
existente de importación. No se cambiarían el mapeo LLVM, metadata, ABI ni
interoperabilidad. Deben contrastarse tuplas anidadas, genéricos abiertos,
controles escalares y el campo atómico antes de considerarlo terminado.

El ensayo incorpora los tipos de tupla de campos estáticos para los que el
modelo ya calcula layout, y deja diferidos los que aún no lo tienen. Tras la
compilación incremental, los casos escalar, estructura, `WeakArc`, tupla
anidada y genérico abierto ejecutaron 42 en O0/O2; también lo hizo la escritura
y lectura de una tupla estática escalar. Las tuplas estáticas que contienen
`Rc` y `WeakRc` siguieron rechazándose semánticamente. Evidencia:
`A:/nsc-fixes-evidencia/static-tuple-layout-directed.log`. El ensayo está sin
commit y no verifica estado de namespace, bibliotecas precompiladas, un valor
atómico no vacío en la tupla ni su destrucción al terminar el módulo. Tampoco
se ha comparado todavía el defecto con la revisión base.

Los controles posteriores `static_tuple_atomic_replace` y
`static_tuple_atomic_replace_other` guardan un `WeakArc` no vacío en el campo
estático, lo sustituyen por un débil vacío o por uno de otro bloque y consultan
los contadores de los fuertes vivos. Ambos ejecutaron 42 en O0/O2:
`A:/nsc-fixes-evidencia/static-tuple-weak-replace.log`. Los casos prueban la
sustitución explícita durante `Main`, no la destrucción automática al descargar
el módulo ni la liberación directa de los bloques de control. No se modificó el
compilador en esta tanda.

La sintaxis directa `(int, long) value` en namespace no alcanza el análisis:
el parser informa `expected a global declaration`
(`namespace-tuple-syntax.log`). Un alias `using Pair = (int, long)` sí llega
a resolución de tipos, pero su uso como estado de namespace se rechaza porque
`$tuple` no identifica una plantilla visible (`namespace-tuple-alias-before.log`).
El mismo alias falla en un campo estático de clase
(`class-tuple-alias-before.log`), por lo que no demuestra un fallo exclusivo de
namespace. No se alteró el parser ni la resolución de alias; sus pruebas siguen
con expectativa de éxito y pendientes de investigación. La emisión de tuplas
como estado de namespace permanece sin verificar. Logs en
`A:/nsc-fixes-evidencia`.

El análisis del rechazo del alias localiza una pérdida de identidad en
`ProjectGenericModel::applyExpandedAliasSyntax`: el clon del destino conserva
la marca `isTuple`, pero al aplicar el alias al sitio de uso se copian el nombre
interno y argumentos sin copiar esa marca ni los nombres de elementos. Después,
`processNestedTypeSyntax` trata `$tuple` como una plantilla nominal. La
corrección candidata se limita a preservar la marca y nombres de tupla en esa
función; no altera otros alias ni las reglas de genéricos. Debe comprobarse con
alias de tuplas simples, anidadas y con elementos nombrados, y con los controles
existentes de aliases nominales antes de considerarla terminada.

Con el ensayo aplicado y compilación incremental, los alias directos de tupla
en local o campo, incluidos anidamiento y nombres, ejecutaron 42 en O0/O2
(`tuple-alias-direct-after.log`). Un alias encadenado `Other = Pair` sigue
rechazado por `unknown type name '$tuple'` (`tuple-alias-chain-after.log`).
El alias directo en namespace ahora supera análisis semántico, pero falla en
emisión por falta del layout de tupla (`tuple-alias-namespace-layout.log`).
El modelo de namespace obtiene la clave con `memberModel.canonicalTypeKey` de
la sintaxis de su declaración; el importador de layouts no visita esas
declaraciones. Se investigará si incorporar sus tuplas calculables resuelve el
caso, conservando la ruta independiente de alias encadenados y sin modificar
metadata ni interoperabilidad. Los tres logs están en `A:/nsc-fixes-evidencia`.

**Comprobación posterior:** con el compilador aislado actual, el alias
encadenado de tupla anidada ya ejecuta 42 en O0/O2; el rechazo anterior es
histórico. La selección dirigida también comprobó alias encadenados con
`WeakRc`: campos estáticos, tuplas anidadas y estado de namespace se rechazan;
el control equivalente con `WeakArc` ejecuta 42 en ambos niveles. Comando y
salida: `A:/nsc-fixes-evidencia/tuple-alias-chain-pointer-check.cmd` y `.log`.
La corrección de alias figura en `39db316af`; esta repetición usa el árbol con
cambios locales adicionales.

Con el Core binario preexistente de `pointers-sdk-refresh`, el consumidor
reprodujo esos tres rechazos de `WeakRc` y aceptó `WeakArc`; este último
compiló y ejecutó 42 en O0/O2 (`tuple-alias-chain-binary-check.cmd` y `.log`).
El campo atómico permanece en su valor predeterminado en esta prueba; no
demuestra destrucción al descargar el módulo ni invalida los demás pendientes.
No se regeneró Core para esta ejecución: su identidad anterior y límites están
en `pointers-sdk-refresh-scope.md`.

Una comprobación posterior del alias escalar encadenado como estado de
namespace (`Pair = (int, long); Other = Pair; Other value`) compiló y ejecutó
42 en O0 y O2 con el compilador aislado SHA-256
`f8d6020ee7a69d327a21c7cc339eef43306001d8a67e2f25f542269a5afe8580`.
Se añadió `namespace_chained_tuple_alias_control` a la prueba dirigida;
comando y resultado en `namespace-chained-tuple-directed-20261111.cmd` y
`.log` bajo `A:/nsc-fixes-evidencia`. La prueba declara el estado, pero no lo
lee ni escribe, y utiliza fuentes del SDK: no acredita acceso al valor ni
consumo de estado de namespace precompilado.

Dos controles posteriores sí acceden al estado desde el mismo namespace. Con
el alias escalar encadenado, el programa escribe `(40, 2l)`, desestructura la
tupla y devuelve 42. Con un alias encadenado de `(int, WeakArc<Payload>)`,
guarda un débil no vacío, comprueba que el contador del fuerte pasa a uno,
reemplaza la tupla por un débil vacío y comprueba que vuelve a cero. Ambos
ejecutaron 42 en O0 y O2 con el mismo compilador y fuentes del SDK. Los casos
son `namespace_chained_tuple_alias_access` y
`namespace_chained_tuple_weak_arc_replace`; comandos y salidas en
`namespace-chained-tuple-access-20261111.cmd`/`.log` y
`namespace-chained-weak-arc-20261111.cmd`/`.log`, en `A:/nsc-fixes-evidencia`.
La comprobación cubre sustitución explícita, no descarga del módulo ni
consumo precompilado.

Se añadió además la regresión `nsc.sdk.namespace-tuple-binary`: un productor
NativeSharp de biblioteca estática escribe y lee el estado de namespace con un
alias escalar encadenado; un consumidor independiente llama a la función
pública del productor y verifica 42. Un productor con estado escalar sencillo
sirve de control. Ambos pasaron con productor y consumidor en O0, y con ambos
en O2. La configuración incremental y el resultado 1/1 están en
`A:/nsc-fixes-evidencia/namespace-tuple-binary-20261111/registered.cmd` y
`registered.log`; se empleó el mismo compilador aislado del apartado anterior.
La llamada no expone directamente el estado al consumidor y no prueba cruces
O0/O2, DLL, `WeakArc`, descarga de módulo ni Core binario regenerado.

Al probar acceso directo desde otro módulo se reprodujo un defecto específico:
el control de estado escalar público ejecutaba 42 en O0/O2, pero la biblioteca
que exportaba el estado con alias de tupla fallaba al verificar la interfaz
generada `public (int, long) value;`. El despacho global del parser no admitía
un paréntesis al comienzo, aunque `parseType` ya admite tuplas. En
`Parser::dispatchGlobalFallback` se incorporó esa forma únicamente si
`canStartTypeAt` reconoce el tipo; no se cambió la metadata. Tras la
compilación incremental, productor y consumidor separados escribieron y
leyeron tanto el escalar como la tupla en O0/O2, con salida 42. La regresión
`nsc.sdk.namespace-tuple-binary` pasó 1/1 e incluye el control sintáctico
inválido y la ruta por función: `namespace-tuple-binary-20261111/registered.log`.
El fallo previo se conserva en `direct-before.log`, y el compilador empleado
tiene SHA-256 `25b5946bd906f16875dd02357b819ce352bf6ba2c5be581b55e02709463395c1`.

La prueba existente `nsc.parser` no pasó: falla una de 23 comprobaciones que
espera que se rechace el operador ternario. La misma prueba, compilada
incrementalmente en la base aislada, falla con la misma comprobación y conteo
(`parser-test.log` y `baseline-parser-test.log` en la misma carpeta). La
comparación no convierte el test fallido en éxito. Por error se invocó una
vez la regresión sin el entorno aislado: creó un temporal bajo C:/Windows/Temp
y buscó un runtime no disponible en C, por lo que falló antes de las pruebas;
el directorio temporal ya no existe. Se repitió con las variables de A y se
obtuvo el resultado indicado arriba. No se modificó ni desplegó la instalación
funcional.

Se reprodujo otro límite al exportar una tupla pública con `WeakArc` real y
Core binario: el control con `WeakArc` directo ejecutó 42 en O0/O2, pero la
tupla no permitía producir la biblioteca porque el reconstructor de tipos
rechazaba ese elemento (`namespace-weakarc-binary-20261111/probe-before.log`).
El mapeo provisional acepta ahora la especialización exacta de `WeakArc` de
`NativeSharpCore` cuando su identidad completa figura en la metadata y usa el
reconstructor genérico existente. No habilita por nombre corto otros tipos.

La regresión `nsc.sdk.namespace-weakarc-binary` pasó 1/1 sin omisión: productor
y consumidor separados asignaron y reemplazaron un débil no vacío y comprobaron
que el contador volvía a cero, con y sin tupla, en O0 y O2. Un tipo homónimo
de otro módulo siguió rechazado por el reconstructor. La prueba de tuplas con
vistas conservó sus rechazos preventivos y la regresión escalar de namespace
pasó; resultados en `namespace-weakarc-binary-20261111/registered.log`,
`views.log` y `tuples.log` bajo `A:/nsc-fixes-evidencia`. El compilador aislado
tiene SHA-256 `2991ceade7afb0d4e5d8bc757195bb88821b07b0500a547b8ff870a29921f97d`.
El Core usado es anterior al compilador; no se regeneró. La prueba requiere
`NSC_TEST_CORE_BINARY` y se omite cuando no está configurado. Esto no demuestra
seguridad para otras especializaciones, bibliotecas históricas, tuplas de
vistas ni descarga del módulo. El cambio comparte el mapeo aún sin commit de
`SharedView` y permanece provisional.

La misma prueba reprodujo que el estado público `Arc` directo pasaba en O0/O2,
pero una tupla pública con esa especialización fallaba en el reconstructor
(`namespace-weakarc-binary-20261111/strong-before.log`). El ensayo reconoce
también la identidad completa de `Arc` real, sin cambiar el formato de
metadata. Tras compilar incrementalmente, los cuatro recorridos de estado
directo y tupla para `Arc` y `WeakArc` pasaron en O0/O2 y verificaron que los
contadores regresan a su valor anterior al reemplazo. Los homónimos de ambos
tipos en otro módulo se rechazaron. La regresión registrada pasó 1/1 sin
omisión (`registered.log`), y las pruebas dirigidas de vistas y tuplas escalares
volvieron a pasar (`views.log`, `tuples.log`). Compilador SHA-256
`e134cc95ef59a9302196d0576b7a08db0e301dd13e96200abe7879fd0419d1ea`;
Core binario anterior, sin regenerar. No se acreditan DLL, descarga de módulo,
especializaciones anidadas arbitrarias ni metadata histórica. Los cambios en
el mapeo y las pruebas de las familias atómicas siguen sin commit.

El ensayo posterior del importador recoge también las declaraciones de estado
de namespace con sintaxis y layout de tupla calculable. Con compilación
incremental, el alias directo de tupla escalar y otro que contiene `WeakArc`
ejecutaron 42 en O0/O2; el equivalente que contiene `WeakRc` se rechazó por la
regla de almacenamiento compartido. También pasaron los controles de alias en
local y campo, tupla anidada y elementos nombrados. Evidencia:
`A:/nsc-fixes-evidencia/tuple-alias-namespace-directed.log`. No se ha probado
resolución de nombres de elementos ni namespace precompilado. El fallo histórico
del alias encadenado es anterior a las comprobaciones posteriores descritas
arriba. Los dos ajustes siguen sin commit.

Las compilaciones fueron incrementales y usaron temporales en A. El cambio de
la cabecera del modelo de miembros obligó a recompilar dependencias del propio
compilador, incluidos objetos de fuentes de interoperabilidad en la carpeta de
build aislada; no se modificaron esas fuentes ni las carpetas de build de
interoperabilidad y no se ejecutaron sus pruebas.
