# Tracking — Fase 2

| Ola | Alcance | Estado | Previstos | Materializados | Integrados | No materializados | Mat. sin integrar | Pendientes totales | Validados |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | Infraestructura mínima | LISTA | 3 sintéticos | 3 | 3 | 0 | 0 | 0 | 3 |
| 1 | Fundaciones host | EN CURSO | 747 | 747 | 747 | 0 | 0 | 0 | 736 |
| 2 | Sintaxis y semántica frontal | EN CURSO | 1,197 | 1,197 | 1,197 | 0 | 0 | 0 | 333 |
| 3 | IR y optimización | EN CURSO | 1,784 | 1,784 | 1,784 | 0 | 0 | 0 | 0 |
| 4 | Sesión, frontend y method bodies | EN CURSO | 920 | 920 | 920 | 0 | 0 | 0 | 0 |
| 5 | Backend, ABI, runtime y enlace | EN CURSO | 1,600 | 1,600 | 1,600 | 0 | 0 | 0 | 0 |
| 6 | Producto, driver y servicio | EN CURSO | 1,112 | 1,112 | 1,112 | 0 | 0 | 0 | 0 |
| 7 | Compilador self-host | EN CURSO | 1,027 | 1,027 | 1,027 | 0 | 0 | 0 | 0 |
| 8 | Infraestructura y oráculos self-host | EN CURSO | 726 | 726 | 726 | 0 | 0 | 0 | 0 |
| 9 | Build, instalación y paquete de NSC | EN CURSO | 474 | 474 | 474 | 0 | 0 | 0 | 0 |

Estados permitidos: `PENDIENTE`, `EN CURSO`, `LISTA`, `BLOQUEADA`.

## Orquestador NSC Builder 3

| Entrega | Estado | Implementación | Validación ejecutada | Validación diferida |
|---|---|---|---|---|
| Builder 3 monolítico | LISTA | `nscbuilder3.py` | Sintaxis, catálogo, GUI y construcción NSC Release `2/2 PASS` | Infraestructura C++ y batería Fase 2 |

Builder 3 puede configurar y construir NSC con Clang/LLVM/LLD, seleccionar el
binario producido y, en una operación independiente, construir únicamente la
infraestructura nueva de pruebas. Descubre los `9,587` casos canónicos y presenta sus `5,695` filas
ejecutables junto con `3,892` disposiciones no ejecutables, sin convertir estas
últimas en `PASS`. La GUI separa la construcción/selección de NSC, la
configuración/compilación de `nsc_component_tests` y la ejecución por
total, ola, componente, contrato o caso. `Debug`/`Release` y
`O0`/`O1`/`O2`/`O3` son dimensiones independientes. La implementación completa
del orquestador vive únicamente en `nscbuilder3.py`; no se creó un paquete
auxiliar ni se modificó la infraestructura histórica.

La construcción conserva y muestra toda la salida recibida. Además, las
líneas nativas `[n/total]` de Ninja alimentan un contador, una barra de progreso
y la descripción del paso actual en la pestaña `Construcción`; el reporte
condensado no sustituye la consola ni el `full.log`. Esta presentación tiene
validación estática y queda pendiente de comprobación funcional por el usuario.
La pestaña incorpora además un botón de cancelación propio inmediatamente al
lado de `Construir NSC`, habilitado sólo durante configuración o construcción
y conectado al árbol de procesos activo.
La construcción de NSC y la ejecución de pruebas seleccionan `O0` de forma
predeterminada; los niveles superiores siguen disponibles de forma explícita.
Cuando una operación termina correctamente, el indicador de construcción ya no
queda visualmente en el último paso de Ninja: alcanza `total/total`, muestra
`TERMINADO (100%)` y sustituye la descripción activa por un cierre explícito.

La auditoría estática integral del target Debug recorrió `523/523` unidades C++
con `/Zs /W4 /WX`. La primera pasada encontró nueve archivos fallidos; después
de corregir el conjunto completo, la segunda terminó con `0` fallos. El enlace
y la ejecución funcional permanecen pendientes de la corrida del usuario.

La corrida externa posterior compiló `337/337` objetos y aisló el siguiente
gate en el enlace. La auditoría de símbolos completa identificó `49` referencias
con acceso ABI inconsistente: `42` de Sema y `7` de Dominance. Los productores
de prueba ahora siguen el shim ya usado por Parser; no se modificó producto. La
nueva validación de enlace y ejecución queda pendiente de la corrida del usuario.

La corrida siguiente confirmó que la clausura ABI anterior quedó resuelta y
avanzó al enlace final. La primera causa nueva, los `2` símbolos públicos de
identidad consumidos por LanguageService y Driver, ya tiene incorporado el
productor real `Version.cpp` y el `BuildId.h` del NSC seleccionado. No se usaron
stubs ni valores de prueba. La inferencia no se persiste en el cache, así que
cambiar el NSC seleccionado no conserva una identidad obsoleta; Builder 3
elimina también cualquier valor heredado cuando no se configuró un override.
La validación externa permanece pendiente.

La segunda causa del mismo enlace, `24` símbolos transitivos de
`WindowsLibraryBuilder.cpp`, quedó cerrada estáticamente con sus `11`
productores canónicos. La suite usa el `LLVM-C` del `LLVM_DIR` seleccionado y
copia su runtime al árbol de pruebas en `A:`; no usa Visual Studio ni modifica
producto. La auditoría predictiva de la tabla completa del enlace quedó en
`0` símbolos NativeSharp y `0` símbolos LLVM-C sin productor. La validación
externa permanece pendiente. La auditoría adicional tampoco encontró una
implementación de producto duplicada ni dependencias dinámicas de Visual
Studio en `LLVM-C.dll`; los controladores conservan el runtime adyacente.

Builder 3 dejó de enviar `-fuse-ld=lld` directamente a `lld-link` al configurar
la suite. Envía el valor de flags vacío para limpiar también caches existentes;
`CMAKE_LINKER` continúa seleccionando el mismo ejecutable. Con esto quedan
tratadas las tres causas del último log y se espera la siguiente corrida externa.

La auditoría de las demás selecciones opcionales encontró dos valores que un
árbol reutilizado podía conservar: el archivador y el NSC de comparación.
Builder 3 ahora los elimina expresamente del cache cuando sus campos quedan
vacíos. La implementación está cerrada estáticamente; la configuración externa
permanece pendiente.

Los seis casos ejecutables del contrato de grafo raíz BuildSystem ya no pueden
heredar el generador ni una toolchain parcial. Tanto CTest como Builder 3 fijan
Ninja, `Debug`/`Release`, CMake, Clang, LLD, LLVM, `llvm-mt`, `llvm-rc` y el
NSLink seleccionado; el controlador rechaza una invocación incompleta antes de
configurar. La validación externa permanece pendiente.

Los cinco productores de fixtures self-host ya conservan también fallos por
proyecto, modo y optimización. La primera compilación fallida queda como causa
de infraestructura y los consumidores restantes se reportan `BLOCKED` con la
misma evidencia, en lugar de repetir hasta 1,536 compilaciones fallidas. Ningún
caso desaparece del reporte ni se convierte en éxito.

El catálogo de Builder 3 ya no exige en código los totales históricos de `44`
componentes y `9,587` casos. Descubre el denominador vigente y valida su
integridad mediante relaciones entre olas, diseño, asignación y manifiesto:
fuentes no vacías, IDs únicos, componentes cubiertos, paridad exacta de casos y
cero ejecutables ajenos. Los conteos de esta tabla permanecen como evidencia de
la revisión actual, no como condición para ejecutar una revisión posterior.

Los controladores de BuildSystem y Packaging tampoco dependen ya de sus
totales, rangos o listas de IDs de esta revisión. Ambos descubren su denominador
desde diseño, asignación y contratos; después exigen paridad relacional con el
catálogo, las clasificaciones y el manifiesto. Packaging conserva su
contradicción de perfiles sólo mientras la diferencia exista en los inventarios
vivos. Los oráculos de comportamiento permanecen intactos. Los validadores de
inventario pasaron con el corte actual (`456/36/420/2` para BuildSystem y
`18/12/6/4` para Packaging); la ejecución funcional sigue diferida.

El primer grupo self-host (`Bootstrap`, `CodeGen`, `Documentation` y
`Project`) usa ahora un lector común que descubre casos y contratos, rechaza
IDs duplicados, verifica paridad diseño/asignación, conserva la relación
caso-contrato-capa y coteja los módulos materializados contra el inventario
vivo. Sus cuatro gates estáticos pasan. Las contradicciones de Documentation y
Project se conservan por diferencias de perfil observadas, no por exigir un
número fijo de registros. No se compiló ni ejecutó producto.

Siete controladores self-host adicionales (`Driver`, `IR`, `Parser`,
`Projects`, `Sema`, `Symbols` y `Syntax`) derivan ahora sus rangos, capas y
perfiles NIGHTLY desde el inventario vivo. Los siete gates pasan. La auditoría
detectó además que el parser de Projects omitía `CSH-PRJ-005` cuando el oráculo
estaba en la línea siguiente; el patrón ahora admite espacio multilínea y el
contrato se coteja correctamente, sin modificar la prueba materializada.

Los cinco controladores self-host ejecutables o mixtos (`Basic`, `Diagnostics`,
`Lexer`, `Source` y `Types`) ya derivan también su denominador y disposiciones.
El manifiesto se coteja caso por caso con contrato, capa y perfil; las
clasificaciones no ejecutables forman una partición exacta y los defectos
conocidos deben coincidir con sus etiquetas ejecutables. La validación estática
cerró `217` casos canónicos y `192` ejecutables sin compilar fixtures.

Los dos controladores de infraestructura self-host
(`Bootstrap.Infrastructure` y `Contracts`) derivan ya también contratos, casos,
rangos, fuentes y contradicciones desde el inventario vivo. Los catálogos deben
cubrir exactamente los contratos y casos canónicos, sin duplicados ni fuentes
ausentes; los módulos materializados deben representar la misma partición. Sus
gates estáticos pasan sobre `726` casos canónicos, todos conservados como
`InfrastructurePending`. No se compiló ni ejecutó producto.

El esquema ejecutable carga ya las `5,695` filas reales del manifiesto central.
Se unificaron las clases de timeout entre esquema y ejecutores, se reconocieron
las capas, clases y aislamientos ya usados por los casos canónicos y se separó
el grafo de dependencias entre IDs de caso y productores externos. Las `14`
pruebas unitarias dirigidas del esquema pasan. No se alteró ningún caso ni
oráculo.

Los quince gates host por componente dejaron de copiar denominadores de una
revisión. Ahora reutilizan la selección productiva sobre el manifiesto y los
inventarios canónicos, y validan identidad, orden y estado terminal de cada
fila seleccionada. La auditoría estática cubre `19` componentes y `4,774`
selecciones ejecutables en Debug; las `25` pruebas Python dirigidas del esquema,
selector y helper pasan. `Host.nsc` conserva sus `160` casos como
`InfrastructurePending` derivados del inventario, sin promoverlos al manifiesto.
No se compiló ni ejecutó producto.

Los `27` gates dirigidos foundation de Host.Basic, Host.Diagnostics y
Host.Source conservan su ejecución por contrato, pero ya no mantienen una
segunda lista de IDs ni totales esperados. La selección no vacía, la cobertura
exacta de resultados y los estados permitidos quedan en el selector, runner,
ledger y reporte compartidos; sus `20/20` pruebas Python dirigidas pasan. El
gate especial de ownership de Host.Source permanece separado para la siguiente
auditoría porque también verifica el inventario completo del componente. No se
compiló ni ejecutó producto.

Los `21` gates dirigidos de Host.Language conservan contratos, perfiles y
ejecución, y delegan ya el cierre completo al selector, runner, ledger y reporte
compartidos. Se retiraron exclusivamente sus rangos y conteos duplicados; los
casos `KNOWN_FAILURE` continúan gobernados por `expected_outcome` en el
manifiesto. Los `21/21` módulos pasan validación sintáctica y las `20/20`
pruebas Python del cierre central pasan. No se compiló ni ejecutó producto.

Los `30` gates dirigidos de Host.Target conservan cada contrato, perfil y
ejecución de Target, y delegan ya la clausura exacta al controlador central. Se
retiraron únicamente rangos y conteos duplicados; los oráculos de identidad,
ABI, layout, alineación, bitfields, CPU, LLVM y registros permanecen en los
casos ejecutados. Los `30/30` módulos pasan validación sintáctica y las `20/20`
pruebas Python del cierre central pasan. No se compiló ni ejecutó producto.

La clausura del selector distingue ya dependencias de caso de productores
externos. El caso real `T12B-00001`, que declara `CMake`, `LLVM 23` y `NSLink`,
se selecciona sin fabricar IDs ni perder esos requisitos; los IDs de caso
ausentes siguen rechazándose. Las `9/9` pruebas del selector pasan y la
selección completa Debug recorre `5,695/5,695` filas. No se compiló ni ejecutó
producto.

La ejecución externa `20261009-225838-013b3717`, sobre `c9a1c52ce`, validó
la ruta de construcción del compilador: configuración Release y target `nsc`
pasaron `2/2` con Clang 23.1.0, LLVM 23.1.0, LLD, runtime estático VS2026,
enlace host `STATIC` y optimización host `O0`. El producto quedó en
`A:/nsc-devel/build-clang/bin/nsc.exe`. Este resultado no valida todavía
`nsc_component_tests` ni casos de la matriz.

Los conteos separan implementación de verificación: un caso rojo por defecto
conocido cuenta como implementado, se reporta `KNOWN_FAILURE` y no aumenta la
columna `Verificados`. No se elimina ni se convierte en `PASS`; tampoco bloquea el
flujo ordinario. Un caso que requiere infraestructura todavía inexistente cuenta
como materializado únicamente cuando existe en un catálogo explícito
`InfrastructurePending`; permanece fuera del manifiesto ejecutable y no aumenta
la columna `Validados`.

| Corte principal | Previstos | Materializados | Integrados | No materializados | Mat. sin integrar | Pendientes totales | Validados |
|---|---:|---:|---:|---:|---:|---:|---:|
| **Total global, olas 1–9** | **9,587** | **9,587** | **9,587** | **0** | **0** | **0** | **333** |
| Ola 2 | 1,197 | 1,197 | 1,197 | 0 | 0 | 0 | 333 |
| Ola 3 | 1,784 | 1,784 | 1,784 | 0 | 0 | 0 | 0 |
| Host.Compilation | 416 | 416 | 416 | 0 | 0 | 0 | 0 |
| Host.Frontend | 504 | 504 | 504 | 0 | 0 | 0 | 0 |
| Host.LLVM | 512 | 512 | 512 | 0 | 0 | 0 | 0 |
| Host.Interop | 384 | 384 | 384 | 0 | 0 | 0 | 0 |
| Host.Linker | 320 | 320 | 320 | 0 | 0 | 0 | 0 |
| Host.Runtime | 384 | 384 | 384 | 0 | 0 | 0 | 0 |
| Host.Driver | 616 | 616 | 616 | 0 | 0 | 0 | 0 |
| Host.nsc | 160 | 160 | 160 | 0 | 0 | 0 | 0 |
| Host.LanguageService | 336 | 336 | 336 | 0 | 0 | 0 | 0 |
| **Total confirmado, olas 2–6** | **6,613** | **6,613** | **6,613** | **0** | **0** | **0** | **333** |
| SelfHost.Basic | 28 | 28 | 28 | 0 | 0 | 0 | 0 |
| SelfHost.Bootstrap | 23 | 23 | 23 | 0 | 0 | 0 | 0 |
| SelfHost.CodeGen | 136 | 136 | 136 | 0 | 0 | 0 | 0 |
| SelfHost.Diagnostics | 28 | 28 | 28 | 0 | 0 | 0 | 0 |
| SelfHost.Documentation | 12 | 12 | 12 | 0 | 0 | 0 | 0 |
| SelfHost.Driver | 79 | 79 | 79 | 0 | 0 | 0 | 0 |
| SelfHost.IR | 92 | 92 | 92 | 0 | 0 | 0 | 0 |
| SelfHost.Lexer | 53 | 53 | 53 | 0 | 0 | 0 | 0 |
| SelfHost.Parser | 127 | 127 | 127 | 0 | 0 | 0 | 0 |
| SelfHost.Project | 22 | 22 | 22 | 0 | 0 | 0 | 0 |
| SelfHost.Projects | 95 | 95 | 95 | 0 | 0 | 0 | 0 |
| SelfHost.Sema | 153 | 153 | 153 | 0 | 0 | 0 | 0 |
| SelfHost.Source | 61 | 61 | 61 | 0 | 0 | 0 | 0 |
| SelfHost.Symbols | 35 | 35 | 35 | 0 | 0 | 0 | 0 |
| SelfHost.Syntax | 36 | 36 | 36 | 0 | 0 | 0 | 0 |
| SelfHost.Types | 47 | 47 | 47 | 0 | 0 | 0 | 0 |
| **Total confirmado, olas 2–7** | **7,640** | **7,640** | **7,640** | **0** | **0** | **0** | **333** |
| SelfHost.Bootstrap.Infrastructure | 438 | 438 | 438 | 0 | 0 | 0 | 0 |
| SelfHost.Contracts | 288 | 288 | 288 | 0 | 0 | 0 | 0 |
| **Total confirmado, olas 2–8** | **8,366** | **8,366** | **8,366** | **0** | **0** | **0** | **333** |
| BuildSystem | 456 | 456 | 456 | 0 | 0 | 0 | 0 |
| Packaging | 18 | 18 | 18 | 0 | 0 | 0 | 0 |
| **Total confirmado, olas 2–9** | **8,840** | **8,840** | **8,840** | **0** | **0** | **0** | **333** |

Desglose canónico de la ola 7:

| Componente | Rango canónico | Contratos | Previstos | Materializados | Integrados | Estado |
|---|---|---:|---:|---:|---:|---|
| SelfHost.Basic | `TSH-00001..00028` | 4 | 28 | 28 | 28 | EN CURSO |
| SelfHost.Bootstrap | `TSH-00029..00051` | 3 | 23 | 23 | 23 | EN CURSO |
| SelfHost.CodeGen | `TSH-00052..00187` | 19 | 136 | 136 | 136 | EN CURSO |
| SelfHost.Diagnostics | `TSH-00188..00215` | 4 | 28 | 28 | 28 | EN CURSO |
| SelfHost.Documentation | `TSH-00216..00227` | 2 | 12 | 12 | 12 | EN CURSO |
| SelfHost.Driver | `TSH-00228..00306` | 10 | 79 | 79 | 79 | EN CURSO |
| SelfHost.IR | `TSH-00307..00398` | 12 | 92 | 92 | 92 | EN CURSO |
| SelfHost.Lexer | `TSH-00399..00451` | 8 | 53 | 53 | 53 | EN CURSO |
| SelfHost.Parser | `TSH-00452..00578` | 16 | 127 | 127 | 127 | EN CURSO |
| SelfHost.Project | `TSH-00579..00600` | 3 | 22 | 22 | 22 | EN CURSO |
| SelfHost.Projects | `TSH-00601..00695` | 12 | 95 | 95 | 95 | EN CURSO |
| SelfHost.Sema | `TSH-00696..00848` | 20 | 153 | 153 | 153 | EN CURSO |
| SelfHost.Source | `TSH-00849..00909` | 8 | 61 | 61 | 61 | EN CURSO |
| SelfHost.Symbols | `TSH-00910..00944` | 5 | 35 | 35 | 35 | EN CURSO |
| SelfHost.Syntax | `TSH-00945..00980` | 5 | 36 | 36 | 36 | EN CURSO |
| SelfHost.Types | `TSH-00981..01027` | 7 | 47 | 47 | 47 | EN CURSO |
| **Total ola 7** | `TSH-00001..01027` | **138** | **1,027** | **1,027** | **1,027** | **EN CURSO** |

Desglose canónico de la ola 8:

| Componente | Rango canónico | Contratos | Previstos | Materializados | Integrados | Estado |
|---|---|---:|---:|---:|---:|---|
| SelfHost.Bootstrap.Infrastructure | `T12B-01081..01518` | 73 | 438 | 438 | 438 | EN CURSO |
| SelfHost.Contracts | `T12B-01519..01806` | 48 | 288 | 288 | 288 | EN CURSO |
| **Total ola 8** | `T12B-01081..01806` | **121** | **726** | **726** | **726** | **EN CURSO** |

Desglose canónico de la ola 9:

| Componente | IDs canónicos | Contratos | Previstos | Materializados | Integrados | Estado |
|---|---|---:|---:|---:|---:|---|
| BuildSystem | `T12B-00001..00456` | 76 | 456 | 456 | 456 | EN CURSO |
| Packaging | `T12B-00457..00468`, `T12B-01069..01074` | 3 | 18 | 18 | 18 | EN CURSO |
| **Total ola 9** | conjunto canónico anterior | **79** | **474** | **474** | **474** | **EN CURSO** |

`Host.Compilation` ya tiene sus `416` casos, seis registros, manifiesto, CMake,
gate y evidencia integrados; su validación permanece diferida.
`Host.Frontend` ya tiene registro, manifiesto, CMake, gate y evidencia integrados;
su validación permanece diferida.
`Host.LLVM` está integrado con `72` ejecutables en el manifiesto ordinario y
`440` `InfrastructurePending` visibles sólo en el registro. `Host.Interop` está
integrado con `160` ejecutables en el manifiesto ordinario y `224`
`InfrastructurePending` visibles sólo en el registro.
`Host.Driver` está integrado con `264` ejecutables en el manifiesto ordinario y
`352` `InfrastructurePending` visibles sólo en el registro. Sus seis módulos,
gate y evidencia están conectados; la validación permanece diferida.
`Host.Runtime` está integrado con `80` ejecutables en el manifiesto ordinario y
`304` `InfrastructurePending` visibles en su catálogo materializado. Sus cuatro
módulos, gate y evidencia están conectados; la validación permanece diferida.
`Host.nsc` está integrado con sus `160` casos como `InfrastructurePending`:
`80` unitarios bloqueados por seams privados y `80` de proceso bloqueados por
el ejecutor NSC general ausente. No agrega filas al manifiesto ordinario; sus
dos módulos, gate de clasificación y evidencia están conectados sin ejecutar.
`Host.LanguageService` tiene sus `336/336` casos materializados e integrados:
`255` `Required`, `1` `KnownProductDefect` ejecutable y `80`
`InfrastructurePending`. `TF-LANGUAGE-SERVICE-304` conserva el oráculo de
rechazo atómico de un nombre reservado, está en el manifiesto como
`KNOWN_FAILURE` y permanece rojo hasta corregir el producto. Su
integración central está materializada; su validación permanece diferida y no
aumenta `Validados`.

`SelfHost.Bootstrap` tiene `23/23` casos integrados como `InfrastructurePending`.
`SelfHost.Basic` tiene `28/28` casos integrados: `27` ejecutables y `1`
`PlanContradiction`. `SelfHost.CodeGen` tiene `136/136` casos integrados como
`InfrastructurePending`. `SelfHost.Diagnostics` reemplaza su materialización
anterior por `28/28` casos ejecutables integrados. `SelfHost.Documentation`,
`SelfHost.Driver`, `SelfHost.IR`, `SelfHost.Parser` y `SelfHost.Project` tienen
respectivamente `12`, `79`, `92`, `127` y `22` casos integrados como
`InfrastructurePending`. `SelfHost.Project` conserva abierta la contradicción
entre los perfiles del plan y la asignación. `SelfHost.Projects` y
`SelfHost.Sema` agregan respectivamente `95` y `153` casos integrados como
`InfrastructurePending`. `SelfHost.Types` integra `47/47`: `36` ejecutables,
`1` `InfrastructurePending` y `10` `PlanContradiction`; dos de sus ejecutables
conservan `KnownProductDefect`. `SelfHost.Lexer` integra `53/53` casos
ejecutables: `52` ordinarios y `1` `KnownProductDefect`. `SelfHost.Source`
integra `61/61`: `48` ejecutables (`47` ordinarios y `1`
`KnownProductDefect`), `2` `InfrastructurePending` y `11`
`PlanContradiction`. `SelfHost.Symbols` y `SelfHost.Syntax` integran sus `35`
y `36` casos como `InfrastructurePending`. Ninguno de estos cierres aumenta
`Validados` porque no se ejecutaron builds, pruebas ni gates.

`SelfHost.Bootstrap.Infrastructure` integra sus `438/438` casos como
`InfrastructurePending`: `16` productores de datos normativos y `57`
verificadores dependen de la clausura real `nsc0/nsc1/nsc2`, NSLink, LLVM,
Python y contratos self-host. `SelfHost.Contracts` integra `288/288` casos
como `InfrastructurePending` porque requiere esa infraestructura, fixtures
self-host y productos SH41. Ambos componentes conservan abierta la
contradicción entre los perfiles PR/NIGHTLY del plan y la asignación totalmente
NIGHTLY del inventario. No incorporan filas al manifiesto ordinario y no
aumentan `Validados`.

`BuildSystem` materializa e integra `456/456` casos: `34` Required y `2`
`KnownProductDefect` forman las `36` filas del controlador nuevo; `420`
permanecen `HistoricalDelegated` a los gates congelados asignados por la matriz.
La delegación conserva cada ID y dueño sin copiar ni ejecutar la suite histórica.
`Packaging` materializa e integra `18/18`: `8` Required y `4`
`KnownProductDefect` forman las `12` filas del controlador de proceso; `6`
casos de `deploy.py` permanecen `InfrastructurePending` por requerir
automatización GUI modal. Los defectos conocidos de `deploy.bat` conservan
visible que el wrapper ignora build/prefix y fija rutas A:/C:. La contradicción
de perfiles `15 PR + 3 NIGHTLY` frente a `18 NIGHTLY` permanece documentada y
la matriz de asignación gobierna la ejecución. Ningún caso de la Ola 9 aumenta
`Validados` en este corte.

Desglose de la ola 1: `736 PASS`, `7 KNOWN_FAILURE` implementados y ejecutables
de forma individual, y `4` casos materializados como `InfrastructurePending`.
Los siete defectos permanecen rojos sin interferir con el flujo ordinario.

La ola 1 conserva cuatro limitaciones de infraestructura (`TS-SRC-008`,
`TS-SRC-040`, `TS-SRC-041`, `TS-SRC-049`). Ya existen en el registro central
con disposición `InfrastructurePending`, permanecen fuera del manifiesto
ejecutable y no aumentan `Validados`. La ola 1 no se declara `LISTA` porque la
validación de esos cuatro oráculos sigue requiriendo memoria extrema,
coordinación determinista de E/S o ASan con aislamiento de proceso.

La auditoría estática global contra los inventarios canónicos cerró `44/44`
componentes, `1,208/1,208` contratos y `9,587/9,587` casos materializados e
integrados. El manifiesto ordinario conserva únicamente casos ejecutables; las
disposiciones no ejecutables permanecen en registros o catálogos explícitos.
Este cierre es de implementación: no ejecutó builds, pruebas ni gates y por eso
el total conservador de `Validados` permanece en `333`.

Desglose certificado de `Host.Lexer`: `110/110` casos implementados, `109 PASS`
y `1 KNOWN_FAILURE` (`TL-LEX-075`) en Debug y Release. El target consolidado y
el gate controlador pasaron en ambos modos; no se ejecutó la suite histórica.

Desglose de `Host.AST`: `230/230` casos materializados e integrados. Los `224`
casos ejecutables conservan su certificación `PASS` en Debug y Release; los seis
restantes están materializados como `InfrastructurePending`, fuera del manifiesto
ordinario y sin validación. No se ejecutó la suite histórica.

Estado actual de `Host.Parser`: `536/536` casos están materializados en módulos
nuevos y conservan visibles los defectos conocidos y las limitaciones de
infraestructura. El registro central, el manifiesto y el gate controlador ya
están integrados; por eso estos casos aumentan `Implementados`, pero todavía no
aumentan `Verificados`. La auditoría independiente, el build y la ejecución de
los `536` IDs quedan diferidos al cierre offline, después de completar toda la
implementación de la Fase 2.

Estado actual de `Host.Sema`: `321/321` casos están materializados en veinte
módulos nuevos y tres soportes aislados. Los defectos conocidos y los casos que
requieren sanitizer, seam privado o aislamiento de proceso permanecen visibles
en sus registros; no se ejecutaron ni se presentaron como verificados. La
integración central de registro, manifiesto, CMake y controlador está
materializada y su validación queda diferida.

La primera construcción Debug offline alcanzó `144/524` y descubrió una
omisión del soporte común: siete módulos consumían tres helpers no publicados.
`SemaTestSupport.h` ahora proporciona `addSemaCase`, `AnalyzerFixture` y
`diagnosticCount`; las dos inicializaciones ambiguas de `EffectContext` también
quedaron desambiguadas. El cierre es estático y conserva `321/321`
materializados; la nueva construcción del usuario determinará la verificación.

Estado actual de `Host.HIR`: `232/232` casos están materializados para los
`29/29` contratos `CF-HIR-001..029` en cinco archivos nuevos. Su registro,
target, manifiesto y controlador de ola ya están integrados; la validación
permanece pendiente.

Estado actual de `Host.SSA`: `336/336` casos están materializados para los
`42/42` contratos `CF-SSA-001..042` en cinco archivos nuevos. Su registro,
target, manifiesto y controlador de ola ya están integrados; la validación
permanece pendiente.

Estado actual de `Host.CFG`: `208/208` casos están materializados para los
`26/26` contratos `CF-CFG-001..026` en cinco archivos nuevos. Su registro,
target, manifiesto y controlador de ola ya están integrados; la validación
permanece pendiente.

Estado actual de `Host.Dominance`: `256/256` casos están materializados para
los `32/32` contratos `CF-DOM-001..032` en cinco archivos nuevos. Los casos de
defecto real o infraestructura especial permanecen visibles en los registros;
la integración central está materializada y la validación permanece pendiente.

Estado actual de `Host.MIR`: `304/304` casos están materializados para los
`38/38` contratos `CF-MIR-001..038` en cinco archivos nuevos. Los `38` casos
`NIGHTLY` conservan su perfil canónico; la integración central y la validación
está materializada y la validación permanece pendiente.

Estado actual de `Host.Passes`: `448/448` casos están materializados para los
`56/56` contratos `CF-PAS-001..056` en ocho archivos nuevos. Conserva `392`
casos `PR`, `56` casos `NIGHTLY` y seis limitaciones explícitas que requieren
instrumentación de lifetime o expansión exacta de corpus. La integración
central está materializada y la validación permanece pendiente.

La integración de la Ola 3 incorpora `1,767` filas ejecutables al manifiesto:
`1,765` ordinarias y `2` `KNOWN_FAILURE`. Los `17` casos restantes permanecen
materializados y visibles como infraestructura especial; no forman parte del
flujo ordinario hasta que exista su aislamiento, sanitizer, seam o corpus.

Trabajo activo en paralelo, exclusivamente de implementación:

| Componente | Rango | Estado de materialización |
|---|---|---|
| Host.Dominance | `TF-DOM-001..256` | MATERIALIZADO E INTEGRADO 256/256; validación pendiente |
| Host.MIR | `TF-MIR-001..304` | MATERIALIZADO E INTEGRADO 304/304; validación pendiente |
| Host.Passes | `TF-PAS-001..448` | MATERIALIZADO E INTEGRADO 448/448; validación pendiente |
| Host.Compilation | `TF-COMPILATION-001..416` | MATERIALIZADO E INTEGRADO 416/416; validación pendiente |
| Host.Frontend | `TF-FRONT-001..504` | MATERIALIZADO E INTEGRADO 504/504; validación pendiente |
| Host.LLVM | `TF-LLVM-001..512` | MATERIALIZADO E INTEGRADO 512/512; 72 ejecutables y 440 InfrastructurePending; validación pendiente |
| Host.Interop | `TF-INTEROP-001..384` | MATERIALIZADO E INTEGRADO 384/384; 160 ejecutables y 224 InfrastructurePending; validación pendiente |
| Host.Linker | `TF-LINKER-001..320` | MATERIALIZADO E INTEGRADO 320/320; 13 ejecutables y 307 InfrastructurePending; validación pendiente |
| Host.nsc | `TF-NSC-001..160` | MATERIALIZADO E INTEGRADO 160/160; 160 InfrastructurePending; validación pendiente |

Estado actual de `Host.Compilation`: `416/416` casos y `52/52` contratos están
materializados en siete archivos nuevos. Los `416` casos son autocontenidos:
`328` unitarios, `88` de integración, `364` `PR` y `52` `NIGHTLY`. Sus seis
registros, manifiesto, CMake y controlador están integrados; la validación
permanece pendiente.

Estado actual de `Host.Frontend`: `504/504` casos y `63/63` contratos están
materializados en siete archivos nuevos. Conserva `441` casos `PR`, `63`
`NIGHTLY`, `20` contratos unitarios, `42` de componente y `1` de integración.
Su registro, manifiesto, CMake y controlador están integrados; la validación
permanece pendiente.

Estado actual de `Host.Interop`: `384/384` casos y `48/48` contratos están
materializados en siete archivos nuevos. `160` casos tienen oráculo directo y
`224` requieren infraestructura especial; estos últimos permanecerán fuera del
flujo ordinario y con disposición `InfrastructurePending`. La validación sigue
pendiente. Los seis registros, las cuatro fuentes de producto, las `160` filas
ejecutables y el controlador están integrados.

Estado actual de `Host.LLVM`: `512/512` casos y `64/64` contratos están
materializados en nueve archivos nuevos. `72` casos son autocontenidos y `440`
requieren infraestructura LLVM, comparación IR/COFF, child process,
filesystem/concurrencia determinista, runtime/SEH/OOP o corpus físico. Esa
dependencia se registra como infraestructura especial, no como defecto de
producto. Los ocho registros, sus fuentes autocontenidas, las `72` filas
ejecutables y el controlador están integrados; la validación permanece pendiente.

Estado actual de `Host.Linker`: `320/320` casos y `40/40` contratos están
materializados. `13` casos tienen oráculo local ejecutable y están integrados
en el manifiesto ordinario; `307` requieren builders COFF/PE/PDB, NSLink,
procesos controlados, productos enlazados, cache o publicación atómica. Esos
casos conservan disposición `InfrastructurePending` fuera del manifiesto
ordinario. No hay casos `KnownProductDefect`. Los tres registros, las dos
fuentes de producto estrictamente ejercidas y el controlador están integrados;
la validación permanece pendiente.

Secuencia vigente desde el 9 de octubre de 2026: primero se implementa toda la
infraestructura y todos los casos de la Fase 2; después se ejecuta la validación
offline. Mientras dure la implementación no se consumirá tiempo en builds,
ejecuciones ni gates parciales. Los conteos verificados permanecerán congelados
hasta comenzar ese cierre.

Validación 21 cerró la segunda mitad del contrato de dependencias: el ledger y
el runner ya distinguen dependencias entre casos de requisitos externos. Los
`213` casos que declaran alguno de `11` productores externos conservan esa
evidencia sin quedar esperando un resultado interno imposible. Las `11/11`
pruebas dirigidas de ledger y runner pasan; no se compiló ni ejecutó producto.

Validación 22 reemplazó la partición privada del gate especial `Host.Source`
por una clasificación JSONL canónica. El inventario vivo demuestra `58 = 54`
ejecutables `+ 4` `InfrastructurePending`; el gate ya no contiene rangos,
conteos ni listas privadas de IDs y conserva la validación integral del
registro. La comprobación fue estática; no se ejecutó producto.

Validación 23 retiró del gate sintético las expectativas rígidas de `3`
resultados y `8` eventos. El gate carga y selecciona el fixture con la API
productiva, compara identidades y estados con los casos vivos, y valida el
protocolo completo de eventos con cardinalidad derivada. Sus `2/2` pruebas
Python pasan; no se ejecutó el host compilado.

Validación 24 completó el barrido estático de los `118` controladores de
integración. No quedan listas ni conteos privados de casos dependientes de una
revisión fuera de los helpers dinámicos. Se conservaron los valores que sí son
oráculos del producto: forma de registros, variantes de contratos, continuidad
de inventarios y un archivo de paquete por publicación. La nueva prueba del
manifiesto vivo cerró `176` selecciones causales y la suite Python completa
pasó `71/71`; no se compiló ni ejecutó producto.

Validación 25 cerró estáticamente el grafo C++ y CTest: `307` archivos de
módulo corresponden uno a uno con `307` definiciones, declaraciones y llamadas
de registro; las `535` fuentes del target existen y no se repiten; los `115`
controladores `run_*.py` tienen exactamente una referencia CTest y los `117`
nombres CTest son únicos. La prueba deriva todos los conjuntos desde el árbol
vivo y no congela estos totales.

Validación 26 agregó el gate global del registro. Su expectativa viva contiene
`9,116` filas (`7,360` Host, `1,753` SelfHost y `3` Infrastructure), derivadas
de los inventarios y fixture sin listas privadas. El gate rechaza formato,
duplicados, desorden, omisiones, inesperados y cambios de contrato/componente.
Sus pruebas Python y la conexión CTest pasan estáticamente; la ejecución contra
el host actualizado corresponde a la siguiente construcción offline.

Validación 27 agregó un cotejo AST/CMake para los `116` controladores. Cada
bloque CTest proporciona todas las opciones `required=True` de su parser, no
pasa opciones desconocidas y referencia exactamente un script; todos los
scripts quedan conectados. La suite Python completa pasa `76/76` sin ejecutar
ni compilar producto.

Validación 28 fijó el contrato puro de comandos de NSC Builder 3. Sus cuatro
pruebas derivan y cotejan los árboles separados Debug/Release, rutas CMake
normalizadas, NSC/NSLink/LLVM seleccionados, opciones de cache ausentes y
presentes, optimización host `O0`, enlace estático y los únicos targets
autorizados (`nsc` y `nsc_component_tests`). Las pruebas dirigidas pasan `4/4`
y la suite Python completa pasa `80/80`; no se descubrieron herramientas
reales, no se abrió la GUI y no se configuró, compiló ni ejecutó producto.

Validación 29 demuestra que Builder 3 conserva la frontera exclusiva de NSC:
sus `44` componentes coinciden exactamente con las olas de implementación y
pertenecen sólo a `Host.*`, `SelfHost.*`, `BuildSystem` o `Packaging`. Los `13`
componentes documentales fuera de esas olas —incluidas las bibliotecas y el
SDK— no aparecen en ninguna fila del manifiesto ejecutable. Las pruebas
dirigidas pasan `2/2` y la suite Python completa `82/82`; no se eliminó ni
modificó ningún inventario externo.

Validación 30 cerró la matriz lógica de Builder 3 contra el catálogo vivo. Las
`12,542` variantes esperadas son únicas y corresponden exactamente a modos y
optimizaciones declarados por los `5,695` casos ejecutables. Los filtros
acumulativos de perfil y las particiones por ola, componente y contrato
coinciden con sus campos canónicos; las `3,892` disposiciones no ejecutables
generan cero variantes. Las pruebas dirigidas pasan `4/4` y la suite Python
completa `86/86`, sin ejecutar ninguna celda.

Validación 31 cerró el despacho de ejecutores de Builder 3. La prueba negativa
demostró que `historical-weekly` caía antes en el controlador de identidad y
podía devolver `PASS`; ahora cualquier ejecutor distinto de los dos realmente
implementados termina como `INFRASTRUCTURE_ERROR` sin invocar otra ruta. Las
`5,695` filas ejecutables vivas alcanzan exactamente uno de los cinco
controladores previstos. Las pruebas dirigidas pasan `2/2` y la suite Python
completa `88/88`.

Validación 32 alineó los timeouts de Builder 3 con el esquema central. Antes,
`49` casos `short` y `398` casos `medium` se degradaban silenciosamente a
`normal`, alterando `15 -> 30 s` y `60 -> 30 s`. Builder 3 reconoce ahora las
siete clases con sus duraciones exactas y rechaza cualquier clase desconocida
antes de crear un proceso. Las pruebas dirigidas pasan `3/3` y la suite Python
completa `91/91`.

Validación 33 cerró las precondiciones de las rutas especializadas. Los `192`
casos self-host contienen proyecto local existente, argumento, sandbox y
oráculo tipados; las cuatro operaciones de identidad coinciden con el ejecutor
real; las `36` filas BuildSystem y `12` Packaging alcanzan exactamente una
rama válida de sus gates mediante dobles, sin herramientas externas. Las
pruebas dirigidas pasan `4/4` y la suite Python completa `95/95`.

Validación 34 eliminó dos falsos cierres de evidencia: un conjunto vacío y un
estado desconocido devolvían `PASSED`. El estado global exige ahora al menos un
resultado y acepta sólo los siete estados del protocolo; cualquier otro valor
impide publicar el cierre. La cancelación conserva una salida `CANCELLED` por
cada variante no iniciada. Las pruebas dirigidas pasan `4/4` y la suite Python
completa `99/99`.

Validación 35 cerró la cardinalidad terminal. Si `metadata.json` declara
`variant_count`, Builder 3 exige ahora exactamente ese número de claves únicas
antes de escribir `results.json`, `summary.md` o `RUN_FINISHED`. Las pruebas
reproducen y rechazan una variante omitida, una clave duplicada y una
cardinalidad mal tipada; el flujo correcto conserva pares de eventos, resultados
y log progresivo. Las pruebas dirigidas pasan `4/4` y la suite Python completa
`103/103`.

Validación 36 hizo determinista la reejecución. La proyección conserva el orden
del plan visible, incluye sólo estados terminales y rechaza claves duplicadas o
ajenas en lugar de descartarlas silenciosamente. `PASS` y `KNOWN_FAILURE` no
entran al retry. Las pruebas dirigidas pasan `4/4` y la suite Python completa
`107/107`.

Validación 37 cerró la frontera de rutas y recibos de Builder 3. Las tres
raíces escribibles configurables rechazan `C:` y el almacenamiento propio
permanece en `A:`. Los recibos conservan exactamente ruta, tamaño, fecha y
SHA-256 del NSC y NSLink seleccionados. Las pruebas dirigidas pasan `4/4` y la
suite Python completa `111/111`; no se descubrieron ni ejecutaron herramientas
reales.

Validación 38 eliminó la sustitución silenciosa de herramientas al comenzar una
operación. La GUI valida ahora los valores visibles tal como fueron elegidos;
`with_discovery` sólo permanece en la autodetección explícita y en la carga
inicial. La reproducción falló antes del cambio y las pruebas dirigidas pasan
`5/5`; la suite Python completa pasa `112/112`.

Validación 39 cerró la integridad de lectura del historial. Una fila terminal
exige ahora metadata, resultados, conteos, cardinalidad y evento final
coherentes; las corridas abiertas quedan `INCOMPLETE` y las contradictorias
`MALFORMED`, nunca `PASSED`. Las pruebas dirigidas pasan `4/4`, la suite Python
completa `116/116` y las `17` corridas locales vigentes conservaron sus estados
terminales al ser auditadas en sólo lectura.

Validación 40 cerró la evidencia de cancelación de operaciones de
infraestructura. El comando activo y todos los comandos no iniciados reciben
ahora filas `CANCELLED` en el orden del plan; una cancelación anterior al primer
comando ya no produce evidencia vacía. Las pruebas dirigidas pasan `3/3` y la
suite Python completa `119/119`, sin crear procesos reales.

Validación 41 cerró la terminación confirmada de procesos. Builder 3 comprueba
`taskkill`, espera la salida y aplica `kill` al hijo conocido cuando el comando
falla o la espera vence; un proceso ya terminado permanece intacto. Las pruebas
con dobles pasan `4/4` y la suite Python completa `123/123`, sin crear ni
terminar procesos reales.

Validación 42 cerró los diagnósticos progresivos de procesos. Los mensajes de
cancelación previa, cancelación activa y timeout pasan ahora por el callback que
alimenta `full.log`, además de conservarse en el resultado de `run`. Las pruebas
dirigidas pasan `6/6` y la suite Python completa `125/125`, usando únicamente
dobles.

Validación 43 demostró el aislamiento de artefactos por modo sin cambio
funcional. Debug y Release usan árboles distintos tanto para NSC como para
`nsc_component_tests`; la ausencia del host conserva una ruta de preflight en
el modo pedido y no activa fallback cruzado. Las pruebas dirigidas pasan `4/4`
y la suite Python completa `129/129` sin ejecutar binarios.

Validación 44 cerró la envolvente del host consolidado. Builder 3 exige ahora
un solo objeto JSON con identidad completa, duración y fallos tipados, estado
`PASS`/`FAIL` y exit code coherente. Cualquier violación queda como error de
infraestructura antes de evaluar defectos conocidos. Las pruebas dirigidas
pasan `5/5` y la suite Python completa `134/134` sin ejecutar el host.

Validación 45 cerró la envolvente del controlador de identidad. Builder 3
exige ahora el esquema canónico, identidad exacta de corrida y caso, evidencia
y conteos tipados, y concordancia entre estado y exit code. Un reporte ajeno,
truncado o contradictorio queda como error de infraestructura con su archivo
como evidencia. Las pruebas dirigidas pasan `5/5` y la suite Python completa
`139/139` sin ejecutar el controlador ni producto.

Validación 46 cerró la envolvente de los gates Python de BuildSystem y
Packaging. Sólo una tupla exacta `(bool, str)` puede convertirse en resultado
de producto; ya no se coercionan cadenas, números o contenedores. Una forma
inválida o una excepción conserva el directorio del caso y termina como error de
infraestructura. Las pruebas dirigidas pasan `5/5` y la suite Python completa
`144/144` sin ejecutar gates reales.

Validación 47 cerró el oráculo self-host antes de preparar cualquier fixture.
Los `192` casos vivos conservan su esquema; exit code, descripción y JSON se
validan sin conversiones ni defaults, y se rechazan campos desconocidos. Las
pruebas dirigidas pasan `5/5` y la suite Python completa `149/149` sin preparar
o ejecutar fixtures.

Validación 48 cerró las entradas self-host antes de preparar productores. El
proyecto debe ser un `.nsproj` local existente, el argumento texto no vacío y
el sandbox el literal declarado; no quedan conversiones por `str` o veracidad.
Los cinco productores vivos no colisionan por nombre base. Las pruebas
dirigidas pasan `5/5` y la suite Python completa `154/154` sin preparar o
ejecutar fixtures.

Validación 49 separó la causa de control del exit code. Una cancelación domina
cualquier resultado parcial como `CANCELLED` y un timeout queda como
`INFRASTRUCTURE_ERROR`; ninguno puede convertirse en falla o defecto conocido
del compilador. Las pruebas dirigidas pasan `8/8` y la suite Python completa
`156/156` usando sólo dobles de proceso.

Validación 50 cerró los tipos de ruta configurados. Los ejecutables y archivos
opcionales ya no aceptan directorios; `LLVM_DIR` y las raíces escribibles no
aceptan archivos, y las raíces conservan su frontera en `A:`. Las pruebas
dirigidas pasan `8/8` y la suite Python completa `159/159` sin descubrir o
ejecutar herramientas reales.

Validación 51 cerró el tipo desde la autodetección. Los ejecutables y el
archivador se seleccionan sólo con `is_file()` y `LLVM_DIR` sólo con
`is_dir()`, conservando candidatos y prioridades. Las pruebas dirigidas pasan
`10/10` y la suite Python completa `161/161` sin descubrir herramientas reales.

Validación 52 cerró el esquema de `config.json` en su lectura. Campos
desconocidos y tipos incorrectos ya no se ignoran ni alcanzan la GUI o los
comandos; los campos omitidos conservan defaults y la autodetección posterior.
Las pruebas dirigidas pasan `4/4` y la suite Python completa `165/165` sin leer
la configuración real.

Validación 53 unificó la autoridad del manifiesto ejecutable. Builder 3 somete
ahora `cases.jsonl` al mismo cargador estricto que el controlador antes de
proyectarlo y ya no convierte silenciosamente identificadores, ejecutores,
resultados esperados u optimizaciones. Las pruebas dirigidas pasan `3/3` y la
suite Python completa `168/168` sin modificar inventarios ni ejecutar producto.

Validación 54 cerró el esquema mínimo de los tres CSV del catálogo. Encabezados
requeridos ausentes o duplicados, filas desalineadas, valores obligatorios
vacíos y olas no canónicas fallan ahora en su origen; columnas futuras con
nombre siguen permitidas. Las pruebas dirigidas pasan `5/5` y la suite Python
completa `173/173` sin modificar inventarios ni ejecutar producto.

Validación 55 cerró los dominios de asignación del compilador. Perfiles, capas,
ejecutores, aislamientos, modos u optimizaciones desconocidos ya no pueden
desaparecer silenciosamente de la selección, y los centinelas no se mezclan con
valores concretos. Las pruebas dirigidas pasan `4/4` y la suite Python completa
`177/177` sin modificar los CSV ni ejecutar producto.

Validación 56 hizo tipada la proyección ejecutable. Los `5695` casos toman ahora
sus seis campos operativos del `CaseManifest` validado; quedaron eliminadas `86`
divergencias de capa causadas por mezclar autoridades. Las pruebas dirigidas
pasan `4/4` y la suite Python completa `178/178` sin cambiar la matriz ni
ejecutar producto.

Validación 57 cerró los catálogos de disposición. Las `669` filas vivas deben
declarar una identidad canónica o pertenecer a uno de cuatro archivos de
metadatos explícitos; rangos vacíos, identidades ambiguas, disposiciones
desconocidas e IDs ajenos ya no se ignoran. Las pruebas dirigidas pasan `5/5` y
la suite Python completa `183/183` sin modificar catálogos ni ejecutar producto.

Validación 58 puso los procesos de los `48` gates Python bajo el controlador
cancelable. Stdout y stderr siguen separados para los oráculos, mientras
cancelación, timeout, propiedad del árbol y consola progresiva quedan
centralizados. Las pruebas dirigidas pasan `6/6` y la suite Python completa
`189/189` usando sólo dobles de proceso.

Validación 59 cerró la persistencia de configuración. `jobs` se valida en
`1..256` antes de la GUI y `config.json` se publica mediante reemplazo atómico,
preservando la versión anterior ante fallos y limpiando temporales. Las pruebas
dirigidas pasan `8/8` y la suite Python completa `193/193` sin tocar la
configuración real.

Validación 60 hizo atómica la publicación terminal. Metadatos, resultados y
resumen ya no aparecen parcialmente; `RUN_FINISHED` sólo se agrega después de
publicarlos y un fallo conserva la corrida como incompleta. Las pruebas
dirigidas pasan `7/7` y la suite Python completa `196/196` sin tocar el historial
real.

Validación 61 conservó las causas de productores self-host. Timeout,
cancelación, exit code no cero y ausencia de ejecutable ya no colapsan al mismo
mensaje; los consumidores reciben `BLOQUEADO_POR` con la causa exacta. Las
pruebas dirigidas pasan `5/5` y la suite Python completa `199/199` usando sólo
dobles.

Validación 62 cerró la auditoría estática de Builder 3 sin promoverla a éxito
funcional. Catálogo `9587/9587`, matriz `12542/12542`, GUI smoke y `199/199`
pruebas Python pasan; no quedan defectos estáticos demostrados en las fronteras
auditadas. La corrida funcional externa sigue pendiente y la tarea permanece
activa hasta cerrar sus errores en cero.

Validacion 63 cerro los enteros de ruta de los gates de integracion. BuildSystem
y Packaging ya no convierten cadenas, flotantes o booleanos a contrato o
variante; exigen un `int` exacto antes de despachar. Las pruebas dirigidas pasan
`10/10` y la suite Python completa `201/201`, sin configurar, compilar o ejecutar
producto. La validacion funcional externa sigue pendiente y la tarea permanece
activa hasta cero errores inesperados.

Validacion 64 cerro las identidades y conteos de resultados. Los catorce gates
host y el gate de infraestructura comparten ahora una autoridad que exige
`case_id` y estados de cadena exactos, ademas de conteos enteros exactos no
negativos. Las pruebas dirigidas pasan `9/9` y la suite Python completa
`203/203`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 65 cerro el protocolo sintetico de eventos. Secuencias,
identidades, payloads y cardinalidades terminales exigen ahora tipos exactos;
booleanos y flotantes ya no pueden acreditar numeros enteros. Las pruebas
dirigidas pasan `4/4` y la suite Python completa `204/204`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 66 cerro el exit code esperado de los cinco gates self-host
ejecutables. Basic, Diagnostics, Lexer, Source y Types comparten ahora una
autoridad que exige un entero exacto y rechaza cadenas, flotantes y booleanos.
Las pruebas dirigidas pasan `3/3` y la suite Python completa `207/207`, sin
ejecutar producto. La tarea funcional permanece activa.

Validacion 67 cerro los tres conteos declarados por contrato en Packaging.
`required`, `known_product_defect` e `infrastructure_pending` deben ser enteros
exactos no negativos antes de compararse con los conjuntos derivados. Las
pruebas dirigidas pasan `7/7` y la suite Python completa `208/208`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 68 cerro la igualdad JSON de SelfHost.Diagnostics. Objetos y arreglos
se comparan ahora por estructura, tipo y valor mediante una lista de trabajo
iterativa; `true` ya no acredita `1` ni un entero acredita un flotante. Las
pruebas dirigidas pasan `4/4` y la suite Python completa `209/209`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 69 cerro el contrato de `CaseResult`. Identidad, enum de estado,
duracion, evidencia y bloqueadores exigen ahora tipos e invariantes completos;
no entran al ledger estados textuales, booleanos numericos, NaN ni colecciones
ambiguas. Las pruebas dirigidas pasan `7/7` y la suite Python completa
`210/210`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 70 cerro la evidencia vacia del adaptador host. Una lista de fallos
con cadenas vacias se clasifica ahora como `INFRASTRUCTURE_ERROR` antes de
construir `CaseResult`, sin propagar excepciones. Las pruebas dirigidas pasan
`9/9` y la suite Python completa `211/211`, sin ejecutar NativeSharp. La tarea
funcional permanece activa.

Validacion 71 cerro el tipo de retorno del executor. `None`, cadenas u otros
objetos se materializan ahora como `INFRASTRUCTURE_ERROR`; el ledger y los
eventos llegan a cierre terminal en vez de truncarse por `AttributeError`. Las
pruebas dirigidas pasan `6/6` y la suite Python completa `212/212`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 72 cerro `run_id` antes de cualquier escritura. Workspace, eventos y
reportes comparten una raiz validada; segmentos vacios, ocultos, anidados o con
traversal fallan sin crear estado. Las pruebas dirigidas pasan `5/5` y la suite
Python completa `213/213`, sin ejecutar producto. La tarea funcional permanece
activa.

Validacion 73 cerro el tipo de la razon de cancelacion. Vacio, espacios,
enteros, booleanos y `None` fallan antes de activar el token; la primera razon
textual valida conserva autoridad. Las pruebas dirigidas pasan `11/11` y la
suite Python completa `213/213`, sin ejecutar producto. La tarea funcional
permanece activa.

Validacion 74 cerro el esquema previo de `EventWriter`. Tipo de evento,
identidad y payload se validan antes de serializar; entradas invalidas no
escriben ni avanzan la secuencia. Las pruebas dirigidas pasan `6/6` y la suite
Python completa `214/214`, sin ejecutar producto. La tarea funcional permanece
activa.

Validacion 75 ordeno evidencia y cierre terminal. Los tres reportes se publican
antes de `RUN_FINISHED`; si la publicacion falla, el stream progresivo se
conserva pero no aparece un cierre falso. Las pruebas dirigidas pasan `11/11` y
la suite Python completa `215/215`, sin ejecutar producto. La tarea funcional
permanece activa.

Validacion 76 cerro la identidad terminal. Metadata y herramientas validan
tipos, dominios, unicidad y SHA-256 completo antes de abrir evidencia. Las
pruebas dirigidas pasan `5/5` y la suite Python completa `216/216`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 77 cerro el protocolo minimo de identidad del compilador. Toda
respuesta `NSCOMPILER 1` exige ahora sus cinco campos no vacios antes de
comparar; la ausencia de `build` o `compiler_version` ya no puede acreditar una
identidad distinta. Las pruebas dirigidas pasan `5/5` y la suite Python completa
`217/217`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 78 cerro las omisiones parciales del selector. Caso, contrato,
componente y capa se validan completos contra el universo antes de filtrar; una
identidad inexistente ya no desaparece cuando comparte solicitud con otra
valida. Las pruebas dirigidas pasan `10/10` y la suite Python completa
`218/218`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 79 preservo la evidencia parcial de timeouts host. Las salidas en
bytes de `TimeoutExpired` se normalizan antes de escribir logs; el diagnostico
causal ya no puede degradarse a un `TypeError`. Las pruebas dirigidas pasan
`10/10` y la suite Python completa `219/219`, sin ejecutar producto. La tarea
funcional permanece activa.

Validacion 80 preservo tambien la evidencia parcial de timeouts del protocolo
de identidad. `_invoke` publica ambos logs antes de propagar el timeout; el
recibo y la salida parcial sobreviven juntos. Las pruebas dirigidas pasan `6/6`
y la suite Python completa `220/220`, sin ejecutar producto. La tarea funcional
permanece activa.

Validacion 81 cerro la contradiccion entre filas y conteos de resultados. Cada
fila exige estado exacto y se coteja contra el outcome declarado; los agregados
se derivan de esas mismas filas. Las pruebas dirigidas pasan `12/12` y la suite
Python completa `221/221`, sin ejecutar producto. La tarea funcional permanece
activa.

Validacion 82 cerro la contradiccion del stream progresivo por caso. Cada
`CASE_FINISHED` exige payload y estado exactos contra el outcome declarado; la
cobertura de IDs ya no basta para acreditar eventos incompatibles. Las pruebas
dirigidas pasan `5/5` y la suite Python completa `222/222`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 83 cerro la coercion del oraculo JSON self-host en Builder 3. La
comparacion conserva estructura, tipo y valor mediante una lista de trabajo
iterativa; `true`, `1` y `1.0` ya no son intercambiables. Las pruebas dirigidas
pasan `6/6` y la suite Python completa `223/223`, sin ejecutar producto. La
tarea funcional permanece activa.

Validacion 84 cerro las duraciones no finitas del reporte enfocado. `NaN` e
infinitos ya no pueden acreditar un resultado terminal. Las pruebas dirigidas
pasan `5/5` y la suite Python completa `223/223`, sin ejecutar producto. La
tarea funcional permanece activa.

Validacion 85 cerro la omision de ruido en stdout del host. Builder 3 exige
ahora exactamente una linea no vacia, igual que el productor y el adaptador
canonico; un diagnostico adicional ya no se descarta para fabricar un sobre
valido. Las pruebas dirigidas pasan `5/5` y la suite Python completa `223/223`,
sin ejecutar producto. La tarea funcional permanece activa.

Validacion 86 cerro la contradiccion entre estado y evidencia del host. `PASS`
no admite fallas, `FAIL` exige evidencia y cada mensaje debe ser no vacio. Las
pruebas dirigidas pasan `6/6` y la suite Python completa `224/224`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 87 cerro la omision de recibos de herramientas en el reporte
enfocado. Nombre, ruta resuelta y SHA-256 de NSC y de los auxiliares
seleccionados se cotejan ahora exactamente. Las pruebas dirigidas pasan `6/6`
y la suite Python completa `225/225`, sin ejecutar producto. La tarea funcional
permanece activa.

Validacion 88 cerro las fallas de gates Python sin causa. Una respuesta falsa
exige ahora detalle no vacio antes de proyectarse a un estado terminal. Las
pruebas dirigidas pasan `6/6` y la suite Python completa `226/226`, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 89 cerro la evidencia vacia del reporte enfocado. Las entradas de
`failures` y `blocked_by` deben ser cadenas no vacias, igual que en
`CaseResult`. Las pruebas dirigidas pasan `6/6` y la suite Python completa
`226/226`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 90 cerro la coercion booleano-entero en el reporte enfocado.
`schema` y `run.selected` exigen ahora tipo `int` exacto ademas del valor `1`.
Las pruebas dirigidas pasan `6/6` y la suite Python completa `226/226`, sin
ejecutar producto. La tarea funcional permanece activa.

Validacion 91 cerro la sustitucion de identidad self-host. `case_argument`
debe coincidir exactamente con el caso seleccionado; los `192/192` casos vivos
cumplen la relacion. Las pruebas dirigidas pasan `5/5` y la suite Python
completa `226/226`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 92 revalido la infraestructura estatica completa: `226/226` pruebas
Python, `9,587` casos, `1,208` contratos, `44` componentes, `5,695` filas
ejecutables, `0` sin clasificar y GUI smoke PASS. No se ejecuto producto; la
corrida funcional integral permanece pendiente y la tarea sigue activa.

Validacion 93 cerro las claves JSON duplicadas del manifiesto canonico. La
unicidad se exige en todos los objetos, incluidos `inputs` y `oracle`. Las
pruebas dirigidas pasan `15/15`, la suite Python `227/227` y el catalogo vivo
conserva `9,587` casos y `0` sin clasificar. La tarea funcional permanece activa.

Validacion 94 cerro las claves JSON duplicadas en catalogos de disposicion. La
regla alcanza objetos anidados y conserva ubicacion causal. Las pruebas
dirigidas pasan `6/6`, la suite Python `228/228` y el catalogo vivo conserva
`9,587` casos y `0` sin clasificar. La tarea funcional permanece activa.

Validacion 95 cerro las claves JSON duplicadas del sobre host consolidado. Una
segunda definicion ya no puede sustituir el estado ni la identidad antes de la
validacion. Las pruebas dirigidas pasan `7/7` y la suite Python `229/229`, sin
ejecutar producto. La tarea funcional permanece activa.

Validacion 96 cerro las claves JSON duplicadas del reporte enfocado. Metadata,
recibos, conteos y resultados exigen ahora definiciones unicas. Las pruebas
dirigidas pasan `7/7` y la suite Python `230/230`, sin ejecutar producto. La
tarea funcional permanece activa.

Validacion 97 cerro las claves JSON duplicadas en la salida de fixtures
self-host. La unicidad se comprueba antes de comparar contra el oraculo. Las
pruebas dirigidas pasan `7/7` y la suite Python `231/231`, sin ejecutar producto.
La tarea funcional permanece activa.

Validacion 98 cerro las claves JSON duplicadas al reabrir el historial.
Metadata o resultados ambiguos quedan `MALFORMED` y el stream de eventos ya usa
el lector estricto. Las pruebas dirigidas pasan `5/5` y la suite Python
`232/232`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 99 cerro las claves JSON duplicadas de `config.json`. Las rutas y el
paralelismo no pueden sustituirse por last-wins antes del descubrimiento. Las
pruebas dirigidas pasan `5/5` y la suite Python `233/233`, sin ejecutar producto.
La tarea funcional permanece activa.

Validacion 100 cerro las claves JSON duplicadas del adaptador host canonico.
Identidad, estado y evidencia exigen definiciones unicas antes de validar el
resultado terminal. Las pruebas dirigidas pasan `11/11` y la suite Python
`234/234`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 101 cerro la duracion ignorada del adaptador host canonico.
`duration_us` exige ahora entero exacto no negativo; ausencia y coerciones no
pueden acreditar un resultado terminal. Las pruebas dirigidas pasan `12/12` y
la suite Python `235/235`, sin ejecutar producto. La tarea funcional permanece
activa.

Validacion 102 cerro las claves JSON duplicadas del lector compartido por los
18 controladores self-host. La unicidad alcanza objetos anidados y conserva
archivo, linea y clave causal. Las pruebas dirigidas pasan `5/5` y la suite
Python `236/236`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 103 cerro las claves JSON duplicadas de los gates BuildSystem y
Packaging mediante un lector comun estricto. La unicidad alcanza objetos
anidados y conserva archivo, linea y clave causal. Las pruebas dirigidas pasan
`9/9` y la suite Python `237/237`, sin ejecutar producto. La tarea funcional
permanece activa.

Validacion 104 cerro las claves JSON duplicadas al reabrir evidencia terminal
en 14 gates host y el gate Infrastructure. `results.json` y `events.jsonl`
usan ahora lectores comunes estrictos. Las pruebas dirigidas pasan `15/15`, la
suite Python `238/238` y la sintaxis de los gates pasa, sin ejecutar producto.
La tarea funcional permanece activa.

Validacion 105 cerro las claves JSON duplicadas del catalogo
`SelfHost.Bootstrap.Infrastructure`. Archivo, linea y clave causal se conservan
antes de validar linaje. La prueba dirigida pasa `1/1` y la suite Python
`239/239`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 106 cerro las claves JSON duplicadas en la salida evaluada por
`SelfHost.Diagnostics`. Una salida contradictoria ya no puede coincidir por
last-wins con el oraculo exacto. Las pruebas dirigidas pasan `6/6` y la suite
Python `240/240`, sin ejecutar producto. La tarea funcional permanece activa.

Validacion 107 cerro la estructura CSV ambigua del inventario canonico.
Encabezados duplicados, campos requeridos ausentes, filas truncadas y celdas
extra ya no atraviesan `InventoryIndex`; columnas adicionales con nombre se
conservan. Las pruebas dirigidas pasan `16/16`, la suite Python `241/241` y el
inventario vivo carga `57` componentes y `1,309` contratos, sin ejecutar
producto. La tarea funcional permanece activa.

Validacion 108 cerro los tres lectores CSV permisivos de integracion y su
consumidor indirecto. Diez consumos declaran ahora campos requeridos sobre un
lector comun estricto. Las pruebas dirigidas pasan `19/19`, la suite Python
`242/242`, la sintaxis pasa y los inventarios vivos conservan `10,193` filas de
diseno, `10,193` asignaciones y `1,309` contratos, sin ejecutar producto. La
tarea funcional permanece activa.

Validacion 109 cerro el alias CSV permisivo de Builder 3 usado por las
auditorias de alcance. Ahora delega en el lector estructural estricto sin
imponer campos ni cerrar columnas extensibles. Las pruebas dirigidas pasan
`8/8` y la suite Python `243/243`, sin ejecutar producto. La tarea funcional
permanece activa.

Validacion 110 cerro la coercion de identidades en BuildSystem y Packaging.
Los indices exigen ahora `case_id` y `contract_id` como cadenas no vacias. Las
pruebas dirigidas pasan `12/12`, la suite Python `244/244`, la sintaxis pasa y
los dos linajes vivos conservan sus denominadores, sin ejecutar producto. La
tarea funcional permanece activa.

Validacion 111 revalido el conjunto estatico posterior al endurecimiento:
`244/244` pruebas Python, `9,587` casos, `1,208` contratos, `44` componentes,
`5,695` ejecutables, `0` sin clasificar y GUI smoke PASS. La infraestructura
estatica reporta cero errores detectados; la corrida funcional integral sigue
pendiente y la tarea permanece activa.

Validacion 112 cerro la aceptacion de `tags` escalares en los manifiestos
crudos de BuildSystem, Packaging y self-host. Los nueve defectos conocidos de
los cuatro componentes auditados conservan ahora arreglos exactos; texto,
elementos no string, vacios y duplicados se rechazan antes de clasificar. Las
pruebas dirigidas pasan `14/14`, la suite Python `246/246` y los `129` casos
vivos auditados pasan la frontera, sin compilar ni ejecutar producto. La tarea
funcional permanece activa.

Validacion 113 unifico la autoridad de esquema de los siete consumidores raw
del manifiesto central. Todos validan primero las `5,695` filas mediante el
parser canonico y cotejan despues identidad y orden antes de conservar objetos
raw. Una matriz de `24` mutaciones queda rechazada, las pruebas dirigidas pasan
`16/16`, la suite Python `248/248` y los `240` casos vivos de esos componentes
validan, sin compilar ni ejecutar producto. La tarea funcional permanece activa.

Validacion 114 elimino las coerciones del catalogo
`SelfHost.Bootstrap.Infrastructure`. Sus ocho campos son ahora exactos y
`case_ids` exige un arreglo no vacio de cadenas unicas. Las pruebas dirigidas
pasan `5/5`, la suite Python `252/252` y el catalogo vivo conserva `73`
contratos, `438` casos y cero productores ausentes, sin ejecutar producto. La
tarea funcional permanece activa.

Validacion 115 rectifico el denominador de consumidores raw de Validacion 113:
eran diez, no siete. `Host.Source`, `Host.nsc` y la validacion generica de
componentes pendientes pasan ahora por el parser canonico. Las pruebas
dirigidas pasan `17/17`, la suite Python `252/252`, los componentes concretos
proyectan `294` filas y no queda lectura directa fuera de la frontera comun,
sin ejecutar producto. La tarea funcional permanece activa.

Validacion 116 elimino las coerciones de identidad del inventario compartido
self-host. Manifiestos y clasificaciones exigen ahora cadenas JSON no vacias
antes de comparar linaje o disposicion. Las pruebas dirigidas pasan `7/7`, la
suite Python `256/256` y los cuatro catalogos vivos validan sus `32` filas, sin
ejecutar producto. La tarea funcional permanece activa.

Validacion 117 elimino las coerciones JSON de los catalogos BuildSystem y
Packaging. Rangos, arreglos de casos, disposiciones, contratos y claves de
contradiccion exigen ahora tipos exactos. Las pruebas dirigidas pasan `17/17`,
la suite Python `260/260` y los cinco catalogos vivos validan sus `512` filas,
sin ejecutar producto. La tarea funcional permanece activa.

Validacion 118 elimino las siete coerciones restantes de identidades y listas
en los gates Project, Bootstrap Infrastructure, Contracts y Documentation.
Diecinueve mutaciones quedan rechazadas, las pruebas dirigidas pasan `24/24`,
la suite Python `266/266` y los cinco documentos vivos validan sus `52` filas,
sin compilar ni ejecutar producto. La tarea funcional permanece activa.

Validacion 119 cerro la coercion de identidades en modulos self-host
materializados. `int`, `bool`, vacio y `None` ya no adquieren identidad por
representacion; los tres gates vivos conservan `171` casos. Las pruebas
dirigidas pasan `6/6` y la suite Python `268/268`, sin compilar ni ejecutar
producto. La tarea funcional permanece activa.

Validacion 120 impidio que los catalogos Contracts y Bootstrap Infrastructure
acrediten productores fuera del checkout mediante rutas absolutas o `..`.
Cuatro escapes quedan rechazados, los `121` contratos vivos conservan `726`
casos, las pruebas dirigidas pasan `13/13` y la suite Python `270/270`, sin
compilar ni ejecutar producto. La tarea funcional permanece activa.

Validacion 121 concilio los cinco reportes externos recientes: cuatro hashes
unicos y un duplicado exacto. Cada causa posee correccion estatica en el arbol
actual; la auditoria Builder 3 conserva `9,587` casos, `1,208` contratos, `44`
componentes y `0` sin clasificar. No se afirma recompilacion posterior: la
corrida funcional externa sigue pendiente y la tarea permanece activa.

Validacion 122 cerro el productor del argumento runtime de la suite. Builder 3
transmite la biblioteca de `install_prefix` en `A:` para Debug y Release y su
prueba contractual rechaza una caida a `Program Files`. La prueba dirigida
pasa `4/4`, la suite Python `270/270`, el catalogo conserva `9,587` casos y
`0` sin clasificar. La auditoria posterior demostro que el consumidor CMake
sombreaba ese argumento; ese defecto se separo en Validacion 123.

Validacion 123 cerro el consumo efectivo del argumento runtime. La declaracion
CMake es ahora `CACHE FILEPATH` sin `FORCE`, por lo que conserva la autoridad
explicita emitida por Builder 3. La prueba dirigida pasa `5/5` y la suite
Python `271/271`, sin configurar, compilar o ejecutar producto. La corrida
funcional externa sigue pendiente y la tarea permanece activa.

Validacion 124 elimino la cache obsoleta de los dos artefactos LLVM C
derivados. `LLVM-C.lib` y `LLVM-C.dll` se recalculan ahora exclusivamente desde
el `LLVM_DIR` vigente en cada configuracion. La prueba dirigida pasa `6/6` y
la suite Python `272/272`, sin configurar, compilar o ejecutar producto. La
corrida funcional externa sigue pendiente y la tarea permanece activa.

Validacion 125 fijo el interprete Python de los 118 controladores CTest sin
hardcodear una instalacion. Builder 3 descubre `sys.executable` y lo transmite
como `Python3_EXECUTABLE` para Debug y Release. Las pruebas dirigidas pasan
`17/17` y la suite Python `273/273`, sin configurar, compilar o ejecutar
producto. La corrida funcional externa sigue pendiente y la tarea permanece
activa.

Validacion 126 rectifico la autoridad runtime establecida en Validacion 122.
Builder 3 ya no deriva `NativeSharp.Runtime.Win32.lib` de `install_prefix`:
usa el mismo directorio `generated` propietario del `nsc.exe` seleccionado o
del `BuildId.h` explicito. Las pruebas dirigidas pasan `6/6`, la suite Python
`273/273` y el catalogo conserva `9,587` casos, `1,208` contratos, `44`
componentes y `0` sin clasificar, sin configurar, compilar o ejecutar producto.
La corrida funcional externa sigue pendiente y la tarea permanece activa.

Validacion 127 cerro la perdida del diagnostico causal de comandos de
infraestructura. `CommandWorker` ya no descarta la salida capturada: los fallos
conservan en `CaseOutcome.detail` el primer error de CMake, compilador, linker o
Ninja y mantienen `full.log` integro. Las pruebas dirigidas pasan `5/5`, la
suite Python `275/275` y el catalogo conserva `9,587` casos, `1,208` contratos,
`44` componentes y `0` sin clasificar, sin configurar, compilar o ejecutar
producto. La corrida funcional externa sigue pendiente y la tarea permanece
activa.

Validacion 128 concilio los 17 recibos locales de Builder 3: siete exitosos y
diez fallidos. Los diez fallos contienen ocho logs unicos y seis causas; las
seis poseen cierre estatico trazable en el arbol actual, no quedan recibos
malformados, omitidos o sin clasificar. Ninguna de esas revisiones constituye
una corrida integral posterior a los cierres 122--127, por lo que la validacion
funcional externa sigue pendiente y la tarea permanece activa.

Validacion 129 cerro la perdida grafica del diagnostico de infraestructura.
La causa ya persistida en `CaseOutcome`, `results.json` y `summary.md` alimenta
ahora el mismo reporte condensado que la ejecucion de casos; ante fallos o
excepciones del controlador, Builder 3 abre automaticamente `Resultados >
Errores condensados`. Las pruebas dirigidas pasan `7/7`, la suite Python
`277/277`, el catalogo conserva `9,587` casos y `0` sin clasificar y el smoke
de GUI pasa, sin configurar, compilar o ejecutar producto. La corrida
funcional externa sigue pendiente y la tarea permanece activa.

Validacion 130 cerro la cardinalidad terminal del plan de infraestructura. Un
fallo conserva su propia fila `FAIL` y materializa todos los comandos
posteriores no iniciados como `BLOCKED` por esa precondicion; la metadata
declara ahora `variant_count`, por lo que un recibo incompleto no puede
publicarse como terminal. Las pruebas dirigidas pasan `9/9`, la suite Python
`279/279`, el catalogo conserva `9,587` casos y `0` sin clasificar y el smoke
de GUI pasa, sin configurar, compilar o ejecutar producto. La corrida
funcional externa sigue pendiente y la tarea permanece activa.

Validacion 131 corrigio el productor comun del registro. La auditoria de los
14 helpers iterativos encontro una sola perdida de propiedad: `Host.Passes`
movia `layer` en la primera de ocho variantes. El target incremental construyo
`8/8` y `nsc_component_tests --list` enumero `8,054` filas con salida `0`.
Los seis archivos protegidos conservan sus hashes. La ejecucion dirigida
descubrio a continuacion un timeout distinto causado por el ordenamiento
cuadratico del registro; queda como siguiente hipotesis. La tarea permanece
activa y no se ejecuto baseline ni se toco `C:`.

Validacion 132 elimino el ordenamiento completo ejecutado tras cada alta del
registro. `TestRegistry::add` localiza una sola posicion con `lower_bound` y
conserva orden, busqueda, duplicados y filtros. La reconstruccion termino
`314/314`, `INFRA-REGISTRY-001` pasa y `--list` conserva `8,054` filas en
`9,631 ms`, dentro del limite de 60 segundos. El gate ya alcanza su comparacion
y revela una diferencia independiente de membresia, que queda como Validacion
133. La tarea permanece activa y no se ejecuto baseline ni se toco `C:`.

Validacion 133 corrigio la identidad de los trece casos Sema 087--099. El
productor emitia `TS-SEMA-87..99` donde el inventario canónico declara
`TS-SEMA-087..099`; la reconstruccion incremental termino `2/2`, `--list`
conserva `8,054` filas y la conciliacion queda en `0` inesperados y `0`
cambios de contrato/componente. El gate dirigido alcanza la comparacion en
`9.17 s` y falla ahora exclusivamente por `1,062` IDs canónicos restantes,
agrupados en diez componentes para la siguiente tanda. La tarea permanece
activa y no se ejecuto baseline ni se toco `C:`.

Validacion 134 hizo observable, sin volverla ejecutable, la colección pendiente
que el gate global confundía con `TestRegistry`. `--list-pending` enumera 841
filas: AST 6, Interop 224, Linker 307 y Runtime 304. La reconstrucción del
target termina correctamente; las 841 identidades conservan contrato y
componente canónicos, no contienen duplicados y no se solapan con las 8,054
filas ordinarias. La siguiente tanda corregirá el gate para exigir la
partición completa, incluidos los 221 productores externos restantes. La tarea
permanece activa y no se ejecutó baseline ni se tocó `C:`.

Validacion 135 corrigio la frontera del gate global sin reducir el denominador.
Las 9,116 identidades canónicas quedan cubiertas exactamente por 8,054 filas
ordinarias, 841 pendientes no ejecutables y 221 productores externos. La unión
tiene 0 solapamientos, 0 omisiones, 0 inesperados y 0 cambios de contrato o
componente. Las pruebas dirigidas pasan `4/4` y
`nsc.compiler-suite.registry-inventory` pasa `1/1` en `18.62 s`. La tarea
permanece activa y continúa con el barrido de los 116 gates Debug; no se
ejecutó baseline ni se tocó `C:`.

Validacion 136 retiro el costo cuadratico residual del registro. Las 8,054
altas ahora se anexan y el orden observable se materializa una sola vez; la
reconstruccion Debug termino `315/315`, `INFRA-REGISTRY-001` pasa y el arranque
`--list` bajo de `9,631 ms` a `645.626 ms`. El recorrido dirigido
`nsc.compiler-suite.host-source-position-controller` pasa `1/1` en `1.05 s`,
por debajo del limite `tiny` de 5 segundos. La tarea permanece activa y sigue
con el barrido de los 116 gates Debug; no se ejecuto baseline ni se toco `C:`.

Validacion 137 completo la superficie operativa de Builder 3 sin ejecutar
producto. Construccion ofrece ahora `Ejecutar pruebas`; Resultados incorpora
`Fallos exactos` con una fila por variante `caso|build|optimizacion`, permite
desactivar o reactivar filas solo para el reintento y reejecuta exclusivamente
los fallos activos. La sintaxis Python pasa, las pruebas dirigidas pasan `7/7`
y el smoke valida botones y pestana. La corrida normal conserva siempre el
denominador completo. La tarea permanece activa; no se compilo ni ejecuto NSC,
no se ejecuto baseline y no se toco `C:`.

Validacion 138 cerro estaticamente la causa de los cuadros interactivos del
runtime de Microsoft. El punto de entrada Windows de `nsc_component_tests`
desactiva fault dialogs, dirige reportes CRT Debug a `stderr` y evita el
reporting interactivo de `abort`, sin ocultar el fallo ni convertirlo en PASS.
El contrato estatico pasa `3/3`, `git diff --check` pasa y los seis archivos
protegidos conservan sus hashes. La compilacion y corrida funcional quedan
pendientes del usuario. La tarea permanece activa; no se compilo ni ejecuto
producto, no se ejecuto baseline y no se toco `C:`.

Validacion 139 concentra la operacion en `Pruebas` y hace observable cada
variante exacta, incluidos los casos unitarios: progreso, estado, duracion,
ultima actividad y accion por fila. `Ejecutar` o `Volver a correr`, tanto en la
columna visible como en el menu contextual, lanza solamente
`caso|build|optimizacion`; los pendientes y filtrados no ofrecen ejecucion. Los
contratos dirigidos pasan `24/24`, el smoke de GUI pasa y `git diff --check`
pasa. La validacion funcional queda pendiente del usuario; no se compilo ni
ejecuto producto, no se ejecuto baseline y no se toco `C:`.

Validacion 140 agrega a la tabla principal de `Pruebas` filtros visuales por
estado exacto y por grupos `Fallos`, `En curso` y `Pendientes/Listos`, con un
contador de filas visibles. Filtrar no cambia el plan ni el denominador de la
corrida. Los contratos dirigidos pasan `25/25`, el smoke de GUI pasa y
`git diff --check` pasa. No se compilo ni ejecuto producto, no se ejecuto
baseline y no se toco `C:`.

Validacion 141 incorpora carga de ejecuciones terminales anteriores en
`Pruebas` y en `Resultados > Historial`. La GUI reconstruye tabla, estados,
duraciones, log y fallos exactos reejecutables sin modificar la evidencia
original; todo reintento crea un `run_id` nuevo. Las ejecuciones incompletas,
malformadas, ajenas a la suite o incompatibles se rechazan. Los contratos
dirigidos pasan `33/33`, el smoke de GUI pasa y `git diff --check` pasa. No se
compilo ni ejecuto producto, no se interfirio con la corrida activa, no se
ejecuto baseline y no se toco `C:`.

Validacion 142 corrige la restauracion grafica parcial descubierta con la
corrida real `20261010-090333-82a4a13f`. Sus `5,695` filas sí estaban completas
en disco (`5,140 PASS`, `247 FAIL`, `88 INFRASTRUCTURE_ERROR`, `187 BLOCKED` y
`33 KNOWN_FAILURE`), pero Builder 3 las sustituia desde `LISTO` con filtro y
ordenamiento activos. La carga ahora selecciona todos los estados, suspende la
reordenacion durante la proyeccion y coteja exactamente los conteos antes de
mostrar la tabla. El cargador leyó la evidencia real con los cinco conteos
exactos, la sintaxis Python, el smoke de GUI y `git diff --check` pasan. La
comprobacion grafica queda pendiente de reabrir Builder 3; no se compilo ni
ejecuto producto, no se ejecuto baseline y no se toco `C:`.

Validacion 143 completa la restauracion de la seleccion historica, no solo de
sus resultados terminales. La corrida `20261010-090333-82a4a13f` vuelve a
materializar `9,587` filas: `5,695` variantes ejecutadas y `3,892` casos no
ejecutables visibles. Se restauran los controles `Total/Todos/Debug/O0`, los
cinco grupos terminales, pendientes, log, reporte condensado y fallos exactos
sin ejecutar ni compilar nada. El cargador acepta el orden real de finalizacion
pero exige el conjunto exacto sin faltantes, sobrantes o duplicados. Las diez
pruebas dirigidas, la sintaxis, la lectura real y el smoke de GUI pasan. La
tarea permanece activa y vuelve al analisis de las siete causas de
infraestructura; no se ejecuto baseline ni se toco `C:`.

Validacion 144 retira la proyeccion grafica intermedia que todavia dejaba la
carga visible incompleta para el usuario. Builder 3 crea ahora las `5,695`
filas terminales directamente desde `results.json`, incluyendo estado,
duracion, detalle y evidencia, y agrega despues las `3,892` filas pendientes.
No reejecuta, recompila ni transforma resultados. La sintaxis Python y el
diferencial pasan. Seleccionar una ejecucion guardada la carga inmediatamente
y la cabecera confirma `Ejecucion CARGADA`; la comprobacion visual corresponde
al usuario.

Validacion 145 separa el historial del flujo vivo. `Pruebas` queda limpio para
una corrida nueva y puede reiniciarse sin borrar evidencia. Cada seleccion de
`Resultados > Historial` abre un tab independiente con las `9,587` filas de su
seleccion, estados terminales y pendientes, filtros por estado, componente,
momento, duracion y texto, mas log, resumen, eventos, resultados y metadatos.
La carga de la corrida real no modifico tabla, conteos, log ni reporte vivos en
el smoke. No se compilo ni ejecuto producto, no se ejecuto baseline y no se
toco `C:`.

Validacion 146 corrige la usabilidad del historial sin cambiar el motor de
ejecucion. La lista identifica cada corrida por fecha, estado, conteos,
configuracion y directorio, y ofrece vista previa de resumen, log y reporte.
Cada corrida se abre con dos secciones principales, `Pruebas` y `Reporte`; el
reporte agrupa sus seis artefactos y las pruebas exponen acciones visibles para
reejecutar la fila seleccionada o la corrida completa. La sintaxis y el smoke
de GUI pasan; no se compilo ni ejecuto producto, no se ejecuto baseline y no se
toco `C:`.

Validacion 147 separa `Historial` de `Resultados` en la jerarquia principal de
Builder 3. `Resultados` queda reservado al log, errores y fallos de la corrida
actual. `Historial` muestra la lista de corridas y abre cada una como tab dentro
de su propio panel; cada corrida conserva internamente `Pruebas` y `Reporte`.
La sintaxis y el smoke de GUI pasan; no se compilo ni ejecuto producto, no se
ejecuto baseline y no se toco `C:`.

Validacion 148 cierra el analisis integral de la corrida funcional
`20261010-090333-82a4a13f` sin reejecutarla. Los `555` resultados no exitosos
quedan contabilizados: `247 FAIL` distribuidos en `95` contratos, `88
INFRASTRUCTURE_ERROR` reducidos a siete fronteras causales, `187 BLOCKED`
derivados de cinco productores self-host y `33 KNOWN_FAILURE` inventariados.
El informe distingue causa confirmada, inferencia y falta de evidencia; en
particular, `65` fallos de compilacion de proyectos pierden el error de carga o
los diagnosticos observados y por ello no se promueven artificialmente a
defectos del producto. No se modifico codigo, oraculos, manifiestos ni
expectativas y no se toco `C:`. Evidencia:
`ANALISIS-ERRORES-CORRIDA-20261010-090333-82a4a13f.md`.

Validacion 149 corrige las dos fronteras confirmadas de Builder 3 sin tocar el
compilador. La carga por ruta absoluta publica ahora una sola vez el directorio
de integracion para que los gates puedan importar `component_gate_support`; el
validador de identidad reconoce `SKIPPED: 0` como contador canonico, pero no lo
acepta como resultado ejecutable ni con cardinalidad positiva. Las `15` pruebas
unitarias dirigidas pasan. Quedan pendientes de una proxima corrida funcional
los `36` casos BuildSystem y los `4` casos de identidad afectados. Evidencia:
`TANDA-VALIDACION-149-CIERRE-BUILDER3-GATES-IDENTIDAD.md`.

Validacion 150 elimina los `18` abortos confirmados de oraculos C++ sin cambiar
expectativas ni producto. Frontend deja de usar `front()` tras una precondicion
fallida; HIR conserva vivo el `FrontendCompilation` propietario de sus nodos; y
MIR no itera bloques si falta la funcion o su secuencia. La construccion
dirigida de `nsc_component_tests` termina `6/6`; los `18/18` casos publican JSON,
con `8 PASS`, `10 FAIL` contractuales, `0 INFRASTRUCTURE_ERROR` y `0` abortos.
No se ejecuto la bateria completa ni se toco `C:`. Evidencia:
`TANDA-VALIDACION-150-CIERRE-ABORTOS-ORACULOS-CPP.md`.

Validacion 151 implementa una frontera contenida para los cinco productores
self-host que usaban `Compile Include` con segmentos padre. Cada gate
materializa dentro de la evidencia de su corrida una copia exacta de las
fuentes, genera un proyecto sin `..`, valida referencias y rechaza escapes,
comodines, duplicados y destinos reutilizados. Los cinco proyectos canónicos y
el producto permanecen intactos. La prueba dirigida cubre los cinco proyectos
vivos y la identidad byte a byte. Por instruccion expresa la ejecución queda
diferida hasta completar las validaciones 152 a 155 y efectuar una sola corrida
integral `Debug/O0/Todos`. Evidencia:
`TANDA-VALIDACION-151-FIXTURES-SELFHOST-PRODUCTORES.md`.

Validacion 152 implementa descubrimiento portable del archivador de Packaging.
Builder 3 conserva una ruta explicita valida, despues consulta `PATH` y por
ultimo las raices `ProgramFiles`, `ProgramW6432` y `ProgramFiles(x86)` que
publica Windows. No codifica una unidad ni modifica el entorno. Dos pruebas
dirigidas cubren la instalacion estandar y la precedencia del override. La
ejecucion queda diferida hasta la corrida integral final. Evidencia:
`TANDA-VALIDACION-152-DESCUBRIMIENTO-ARCHIVADOR-PACKAGING.md`.

Validacion 153 asigna la clase temporal canonica `extended` a los once casos
NIGHTLY de Dominance que ejercitan profundidad, amplitud y escala. No reduce el
trabajo ni toca el producto; un agotamiento de 180 segundos seguira siendo
error de infraestructura. Una prueba dirigida fija los once IDs y su perfil.
La ejecucion queda diferida hasta la corrida integral final. Evidencia:
`TANDA-VALIDACION-153-PRESUPUESTO-NIGHTLY-DOMINANCE.md`.

Validacion 154 codifica la terminacion anormal exacta de `TL-LANG-099` y
`TL-LANG-250`, dos defectos de enum externo ya inventariados. El manifiesto
exige `KNOWN_FAILURE`, etiquetas de defecto/frontera, codigo no cero y la
asercion exacta de indice. Builder 3 no acepta crashes arbitrarios, codigo cero,
texto distinto ni timeouts. Los casos siguen visibles como defecto conocido,
nunca como PASS. La ejecucion queda diferida hasta la corrida integral final.
Evidencia:
`TANDA-VALIDACION-154-AISLAMIENTO-DEFECTOS-CONOCIDOS-ENUM.md`.

Validacion 155 audita las `247/247` filas `FAIL` de la corrida funcional y
confirma que sus `95/95` contratos, manifiestos, documentos y detalles existen
sin reclasificaciones ni omisiones. Los `65` fallos compartidos de
`ProjectCompilation` conservan ahora el error de carga, los contadores y todos
los diagnosticos renderizados; las expectativas permanecen intactas. Los
temporales de esta frontera y del materializador self-host quedan dentro del
workspace de `A:`. Por instruccion expresa no se ejecuto ninguna validacion
parcial; el siguiente paso es una sola construccion y corrida integral
`Debug/O0/Todos`. Evidencia:
`TANDA-VALIDACION-155-EVIDENCIA-ORACULOS-Y-AUDITORIA.md`.

Validacion 156 parte de la corrida integral
`20261010-122235-304f2829` (`5,695/5,695`) y corrige tres residuos de
infraestructura demostrados por sus artefactos. Builder 3 compila ahora el
proyecto self-host materializado en su ruta activa; descubre el `BuildId.h` y
un NSC de comparacion distinto ya existentes; y clasifica los once timeouts
Dominance marcados explicitamente como `FAIL` contractuales, nunca como error
de infraestructura ni como PASS. No se cambio producto ni se redujo cobertura.
Evidencia:
`TANDA-VALIDACION-156-CIERRE-RESIDUOS-CORRIDA-INTEGRAL.md`.

Validacion 157 corrige las dos causas de control detectadas al ejecutar los
`314` casos del controlador despues del commit de la validacion 156: registro
temprano de modulos cargados por ruta para permitir introspeccion de
`dataclasses`, y lectura segura del manifiesto opcional en dobles sinteticos.
No modifica producto ni expectativas. Evidencia:
`TANDA-VALIDACION-157-CIERRE-CONTROLADOR.md`.

Validacion 158 parte de la corrida dirigida
`20261010-142040-70b38c9c`: las dos identidades Host.Basic y SelfHost.Basic
pasaron, mientras cuatro productores revelaron dos residuos de fixture. La
construccion self-host recibe ahora la raiz de bibliotecas del prefijo local y
el fixture Types deja de usar palabras reservadas como identificadores. No se
modifica producto. Evidencia:
`TANDA-VALIDACION-158-DEPENDENCIAS-SELFHOST.md`.

Validacion 159 corrige una precondicion del cargador visible al ejecutar
aisladamente el constructor self-host: los gates requieren tanto la raiz
`controller` como `controller/integration`. Ambas se publican una sola vez; no
se modifica producto ni oraculos. Evidencia:
`TANDA-VALIDACION-159-IMPORTS-GATES-AISLADOS.md`.

Validacion 160 corrige la dependencia declarativa ausente del productor
`SelfHost.Types`: `TypeSystem.ns` usa `List<T>`, por lo que su proyecto de
fixture debe referenciar `NativeSharp.Core.Collections.Generic`. No se modifica
producto ni oraculos. La revalidacion se limito exclusivamente a `TSH-00981` y
termino `1/1 PASS` en `20261010-142944-tsh00981`. Evidencia:
`TANDA-VALIDACION-160-DEPENDENCIA-COLLECTIONS-TYPES.md`.

Validacion 161 cierra la clasificacion causal de la corrida integral
`20261010-142113-a514d35c` (`Debug/O0/Todos`). La autoridad contiene `274 FAIL`,
`4 INFRASTRUCTURE_ERROR`, `161 BLOCKED` y `41 KNOWN_FAILURE`. Los `274 FAIL`
quedan separados sin omisiones en `215` defectos de prueba/oraculo, `4`
defectos de infraestructura, `5` defectos reales confirmados del compilador y
`50` casos que requieren evidencia observada antes de decidir. Los cuatro
productores self-host que originaron los `161 BLOCKED` ya tienen evidencia
dirigida posterior en `PASS`, pero sus consumidores no se promueven a `PASS`
hasta reejecutarlos. Se fijaron siete tandas causales: cada correccion tendra
prueba puntual y al cierre de cada tanda se reejecutaran las `439` variantes
correctivas. La bateria completa de `5,695` variantes queda reservada para el
cierre de todas las tandas. Evidencia:
`TANDA-VALIDACION-161-CLASIFICACION-Y-PLAN-CORRECTIVO.md`.

Validacion 162 corrige la raiz entregada al gate `C12B-BLD-005`. Builder 3
deja de derivar una supuesta instalacion desde el `nsc.exe` seleccionado y usa
el `install_prefix` explicito de su configuracion. Esto conserva la verificacion
real del layout instalado y elimina la falsa copia de `build-clang`. Los cuatro
casos afectados `T12B-00025`, `00026`, `00029` y `00030` terminaron `4/4 PASS`
en `Debug/NA` despues del commit funcional `f013e83bd`, en las corridas
`20261010-164623-9c9cda3a`, `20261010-164630-bb9048cd`,
`20261010-164635-3fb17649` y `20261010-164639-bfd3d751`. Esta evidencia es
dirigida y no sustituye la reejecucion correctiva ni la corrida integral.
Evidencia:
`TANDA-VALIDACION-162-PREFIJO-INSTALADO-BUILDSYSTEM.md`.

Validacion 164 corrige exclusivamente el desfase de indice del oraculo
`PunctuationMatrix()` para `TSH-00445`: `Question` ocupa la posicion `8`, no la
`9` reservada a `Colon`. La correccion de prueba/oraculo se commiteo en
`0d57b8592`; la reejecucion canonica `TSH-00445|Debug|O0` termino `1/1 PASS`
en `20261010-164845-5ee9450d`. No se modifico el lexer ni producto, y esta
prueba dirigida no promueve otros casos. Evidencia:
`TANDA-VALIDACION-164-ORACULO-LEXER-PUNTUACION.md`.

Validacion 166 cierra la Tanda 1 con la accion historica `Reejecutar fallos`
sobre la autoridad `20261010-142113-a514d35c`. La corrida terminal
`20261010-170051-1caf1c2f`, fijada en
`ca1ef7f04f6bd924405ea40f0fc37b4efb19c5fc`, ejecuto exactamente `439/439`
claves en `Debug/O0`: `212 PASS`, `6 KNOWN_FAILURE`, `221 FAIL`,
`0 BLOCKED` y `0 INFRASTRUCTURE_ERROR`. Las transiciones fueron
`221 FAIL -> FAIL`, `53 FAIL -> PASS`, `4 INFRASTRUCTURE_ERROR -> PASS`,
`155 BLOCKED -> PASS` y `6 BLOCKED -> KNOWN_FAILURE`; no hubo claves faltantes,
sobrantes ni duplicadas. Los cuatro productores self-host pasaron y ya no
existe bloqueo productor. La misma revisión incorpora dos grupos tempranos de
Tanda 2: ocho macros de `CF-FRONT-052` y cuarenta fixtures `for` de
`CF-FRONT-056..063`, todos confirmados en `PASS`. No se ejecuto la bateria
integral de `5,695`. Evidencia:
`TANDA-VALIDACION-166-CIERRE-TANDA-1-REEJECUCION-439.md`.

Validacion 168 reconcilia exhaustivamente los `221 FAIL` de la corrida
`20261010-170051-1caf1c2f`: `191` defectos de prueba/oraculo, `22` defectos
reales de producto, `2` pendientes de evidencia y `6` consumidores MIR
derivados de `TF-HIR-120`. La union contiene `221` IDs unicos, sin faltantes,
sobrantes, duplicados ni intersecciones. Los dos pendientes exactos son
`TS-SEMA-111` y `TF-SSA-273`; no deben modificarse antes de instrumentarlos.
No se compilo ni se ejecutaron pruebas para esta actualizacion documental.
Evidencia: `TANDA-VALIDACION-168-CLASIFICACION-221.md`.

Validacion 167 corrigio las fixtures Frontend de `move(...)`, flujo de
excepcion tipada y `trait IValue` en el commit `5ad58bf9e`. La corrida dirigida
`20261010-tanda167-frontend-fixtures-5ad58bf9e` termino `25/25 PASS` en
`Debug/NA`, sin IDs faltantes ni adicionales. Evidencia:
`TANDA-VALIDACION-167-FIXTURES-FRONTEND-MOVE-EXCEPCIONES-OPAQUE.md`.

Validacion 169 preservo el boundary vacio de `CF-FRONT-006` y corrigio su
oraculo en el commit `e3fbe5e7b`. La corrida dirigida
`20261010-tanda169-frontend-empty-namespace-e3fbe5e7b` termino `1/1 PASS` en
`Debug/NA`. Evidencia:
`TANDA-VALIDACION-169-ORACULO-FRONTEND-NAMESPACE-VACIO.md`.

Validacion 170 agrego oraculos explicitos de ausencia para `34` boundaries
Frontend en el commit `de719a9fe`. El build exclusivo de
`nsc_component_tests` Debug fue correcto y la corrida dirigida
`20261010-tanda170-frontend-absence-de719a9fe` termino `34/34 PASS` en
`Debug/NA`, con `0` faltantes, extras o duplicados. Evidencia:
`TANDA-VALIDACION-170-ORACULOS-AUSENCIA-FRONTEND.md`.

Validacion 171 corrigio el oraculo de identidad `int` y separo los contratos
nominal, matriz valida, ausencia, limites, determinismo y escala de
`CF-FRONT-021` en el commit `8558b5c8c`. El build exclusivo de
`nsc_component_tests` Debug fue correcto y la corrida dirigida
`20261010-tanda171-frontend-generic-inference-8558b5c8c` termino `6/6 PASS`
en `Debug/NA`, con `0` faltantes, extras o duplicados. Con este cierre, Tanda 2
queda lista para su gate correctivo; el gate y la corrida integral aun no se
han ejecutado. Evidencia:
`TANDA-VALIDACION-171-INFERENCIA-GENERICA-FRONTEND.md`.

El primer gate correctivo de Tanda 2,
`20261010-181736-433daee9`, reejecuto exactamente los `221` FAIL del corte
anterior: `65 PASS`, `156 FAIL`, cero faltantes, extras o duplicados. El unico
residuo Frontend fue `TF-FRONT-192|Debug|NA`.

Validacion 172 corrigio la identidad de tupla inventada por el oraculo de
escala `CF-FRONT-024` en el commit `022e3aaab`. El build exclusivo de
`nsc_component_tests` Debug fue correcto y la corrida dirigida
`20261010-tanda172-tf-front-192-022e3aaab` termino `1/1 PASS` en `Debug/NA`,
con cero faltantes, extras o duplicados. Tanda 2 queda nuevamente lista para
su gate correctivo; la corrida integral sigue pendiente. Evidencia:
`TANDA-VALIDACION-172-TF-FRONT-192.md`.

Validaciones 223 a 225 corrigen tres defectos de oráculo Dominance sin tocar
producto: `TF-DOM-030` ahora corrompe realmente la metadata de children,
`TF-DOM-072` acota la cadena NIGHTLY a una consulta extremo a extremo y
`TF-DOM-080` sustituye la matriz cartesiana NIGHTLY por consultas
representativas. Evidencia: `TANDA-VALIDACION-223-DOMINANCE-METADATA-CORRUPTA.md`,
`TANDA-VALIDACION-224-DOMINANCE-CHAIN-ESCALA.md` y
`TANDA-VALIDACION-225-DOMINANCE-NCD-ESCALA.md`. No se ejecutaron pruebas por
orden del usuario.

Validacion 226 corrige el defecto productor compartido del verificador de
Dominance que afectaba `TF-DOM-024`, `032`, `040`, `104`, `128`, `216`, `224`,
`232` y `256`: elimina los sets por nodo y las matrices exhaustivas a escala,
preserva exhaustividad en grafos pequenos y calcula idoms, children y fronteras
con memoria acotada. Evidencia:
`TANDA-VALIDACION-226-DOMINANCE-VERIFIER-ACOTADO.md`. No se compilo ni ejecuto;
la validacion queda delegada al usuario.

Validacion 227 implementa el productor HIR del operador condicional para
`TF-HIR-120`: Sema selecciona el tipo comun y registra conversiones, HIR crea
un nodo `Conditional` tipado y su verifier exige condicion `bool` y dos ramas
del tipo de resultado. Los consumidores `TF-MIR-152`, `183`, `195`, `198`,
`200` y `270` permanecen `BLOQUEADO_POR TF-HIR-120` hasta que el usuario
valide el productor; no se parchearon consumidores ni se ejecutaron pruebas.
Evidencia: `TANDA-VALIDACION-227-HIR-CONDICIONAL.md`.

Validacion 205 corrige `TF-PAS-273..280`: el fixture de `CF-PAS-035` coloca
ahora el PHI constante dentro del prefijo que procesa el pase y comprueba una
sola sustitucion con identidad y valor exactos. No se modifico producto ni se
ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-205-PASSES-PHI-CONSTANTE.md`.

Validacion 206 corrige el oraculo ejecutable de `CF-PAS-050`: el dump se
compara contra el opcode MIR canonico `const`, no contra el nombre inventado
`constant`. No se modifico producto ni se ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-206-PASSES-DUMP-CONST.md`.

Validacion 207 corrige `TF-SSA-134` y `TF-SSA-198`: los oraculos distinguen
stores de simbolos promovidos del storage legitimo de un local cuya direccion
fue tomada. No se modifico producto ni se ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-207-SSA-STORES-PROMOVIDOS.md`.

Validacion 208 corrige `TF-SSA-268`: la variante de orden de funciones usa
dos definiciones, de modo que invertir la secuencia produce una corrupcion
observable por el verificador. No se modifico producto ni se ejecutaron
pruebas. Evidencia: `TANDA-VALIDACION-208-SSA-ORDEN-FUNCIONES.md`.

Validacion 209 corrige `TF-SSA-294`: la mutacion ya no intenta sobrescribir
una clave existente mediante insercion; publica un bloque de salida nulo que
el verificador puede observar. No se modifico producto ni se ejecutaron
pruebas. Evidencia: `TANDA-VALIDACION-209-SSA-MAPEO-BLOQUES.md`.

Validacion 210 resuelve la evidencia pendiente de `TF-SSA-273`: el `else`
final del oraculo duplicaba un local tambien en la variante nominal `0`. La
mutacion queda limitada a la variante `7`; no se modifico producto ni se
ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-210-SSA-PROVENIENCIA-NOMINAL.md`.

Validacion 211 corrige `TSH-00019`: los vectores FNV-1a de `é` y `ñ` usan los
bytes UTF-8 observables por `string`, no valores Latin-1. No se modifico el
algoritmo self-host ni se ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-211-SELFHOST-FNV-UTF8.md`.

Validacion 226 completa los oraculos de `TS-SEMA-118`, `125`, `127` y `145`
con la nota ordenada `PreviousDeclaration` que emite el producto. No se
modifico producto ni se ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-226-SEMA-PREVIOUS-DECLARATION.md`.

Validacion 227 alinea `TS-SEMA-167` y `193` con la frontera real del frontend:
un error lexico corta antes del parser y por contrato no publica AST. No se
modifico producto ni se ejecutaron pruebas. Evidencia:
`TANDA-VALIDACION-227-SEMA-FRONTERA-LEXICA.md`.

Validacion 228 actualiza los fixtures de `TS-SEMA-128`, `135`, `157` y `158`
a las formas vigentes de bloques extern C e inline asm, con la secuencia
diagnostica completa. No se modifico producto ni se ejecutaron pruebas.
Evidencia: `TANDA-VALIDACION-228-SEMA-FIXTURES-EXTERN-ASM.md`.

Validacion 229 corrige el productor de `TP-PAR-020`: el lookahead de tipos
consume ahora la anotacion completa `borrowed[lifetime]` con la misma frontera
que `parseType`. No se compilo ni ejecuto; la validacion queda delegada al
usuario. Evidencia: `TANDA-VALIDACION-229-PARSER-LIFETIME-LOOKAHEAD.md`.

Validacion 230 corrige el productor de `TP-PAR-024` y `TP-PAR-098`: el parser
descompone `>>=` en dos cierres genericos y un `=` logico con rango estable,
sin alterar el flujo fisico del lexer. El oraculo 024 observa ahora el parser
productivo, no un token fisico imposible. No se compilo ni ejecuto; la
validacion queda delegada al usuario. Evidencia:
`TANDA-VALIDACION-230-PARSER-GENERIC-SHIFT-ASSIGN.md`.

Validacion 231 corrige `TP-PAR-160`: una colision entre campos promovidos se
clasifica como `DuplicateSymbol`, no como declaracion esperada. Se conserva la
recuperacion existente. No se compilo ni ejecuto; la validacion queda delegada
al usuario. Evidencia:
`TANDA-VALIDACION-231-PARSER-PROMOTED-DUPLICATE.md`.

Validacion 232 corrige `TP-PAR-170`: una alternativa repetida dentro de un
`variant` se clasifica como `DuplicateSymbol`, preservando rango y
recuperacion. No se compilo ni ejecuto; la validacion queda delegada al
usuario. Evidencia:
`TANDA-VALIDACION-232-PARSER-VARIANT-DUPLICATE.md`.

Validacion 233 corrige el productor de `TS-SEMA-100`: `record` publica ahora
en `SemanticModel` el simbolo ya resuelto que contiene `ExpressionInfo`. No se
compilo ni ejecuto; la validacion queda delegada al usuario. Evidencia:
`TANDA-VALIDACION-233-SEMA-SYMBOL-BINDING.md`.

Validacion 234 corrige `TS-SEMA-116`: un inicializador enum que ya produjo
tipo `Error` conserva solamente el diagnostico causal y no agrega
`InvalidEnumInitializer`. No se compilo ni ejecuto; la validacion queda
delegada al usuario. Evidencia:
`TANDA-VALIDACION-234-SEMA-ENUM-CASCADE.md`.

Validacion 235 corrige `TS-SEMA-140`: una global `const` no constante emite
solo `ConstRequiresConstant`; la global ordinaria conserva
`GlobalRequiresConstant`. No se compilo ni ejecuto; la validacion queda
delegada al usuario. Evidencia:
`TANDA-VALIDACION-235-SEMA-CONST-GLOBAL.md`.
