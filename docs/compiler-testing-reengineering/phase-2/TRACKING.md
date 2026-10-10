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
