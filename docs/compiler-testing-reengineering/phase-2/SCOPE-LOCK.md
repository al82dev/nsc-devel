# SCOPE LOCK — Fase 2

## Objetivo

Implementar la suite nueva especificada por la Fase 1, siguiendo las olas y dependencias publicadas. La unidad de
trabajo es el contrato observable y cada caso conserva trazabilidad `case_id -> contract_id -> component`.

## Invariantes

- Las 684 pruebas históricas y su infraestructura permanecen intactas y disponibles como referencia semanal.
- Los seis cambios preexistentes registrados por la Fase 1 no se modifican ni se incluyen en commits.
- No se cambia el comportamiento del compilador para satisfacer una prueba.
- No se toca `C:`, no se despliega y no se usan remotos.
- No se implementa cobertura instrumental en esta fase.
- Un defecto observable conserva una prueba real con oráculo correcto aunque permanezca roja hasta corregir el
  producto; queda prohibido convertirlo en `SKIP`, `XFAIL`, éxito esperado u oráculo vacío.
- Una expectativa de prueba que no corresponde a una capacidad vigente del compilador es un defecto del test
  plan: se elimina o se reescribe contra el contrato real. No se conserva como feature pendiente dentro de la
  suite activa ni autoriza ampliar el producto.
- Una prueba válida que requiere infraestructura especial se registra como pendiente explícito. No se declara
  verificada, pero tampoco bloquea contratos u olas causalmente independientes.
- Avanzar trabajo independiente no reduce denominadores, no oculta pendientes y no autoriza declarar completa
  una ola que aún contenga casos sin verificar.
- Un productor fallido bloquea a sus consumidores; no se usan artefactos viejos como fallback.

## Orden

1. Ola 0: infraestructura mínima y fixtures sintéticos del framework.
2. Olas 1 a 6: compilador host por dependencias.
3. Olas 7 y 8: self-host y bootstrap.
4. Ola 9: construcción y paquete del compilador.

Dentro de la Fase 2 se aplica una separación adicional autorizada el 9 de octubre de 2026:

1. materializar e integrar primero todos los casos, manifiestos y superficies de ejecución de las olas;
2. conservar durante esa implementación todos los defectos y limitaciones con estado visible;
3. no gastar CPU en compilaciones, ejecuciones, gates ni ciclos de validación parciales;
4. ejecutar la validación offline sólo después de completar la infraestructura prevista.

Esta separación cambia el momento de la validación, no reduce la implementación ni permite presentar como
verificado aquello que todavía no se ejecutó.

La Fase 2 es exclusivamente la suite de NSC. Las bibliotecas `Library.*`, el SDK
y los componentes de otros repositorios no se implementan, registran ni ejecutan
en esta infraestructura. Sus inventarios de Fase 1 conservan valor documental,
pero cualquier suite futura les pertenece a sus repositorios respectivos.

## Enmienda: NSC Builder 3 monolítico

El usuario autorizó una interfaz nueva para operar la suite de Fase 2 y ordenó que toda su implementación viva en
un único archivo Python. Por tanto `nscbuilder3.py` contiene GUI, catálogo, selección, procesos, estados, historial y
reportes. No se crean paquetes `_nscbuilder3`, módulos auxiliares, archivos `.pyw` ni scripts nuevos para repartir
esa funcionalidad. Esta excepción monolítica se limita al orquestador y no modifica la suite ni el producto.

Builder 3 permite configurar y construir NSC de forma explícita con Clang/LLVM/LLD, y selecciona el binario
resultante para las pruebas. La construcción del compilador y la infraestructura de pruebas son operaciones
separadas: la segunda sólo construye `nsc_component_tests` y nunca reconstruye NSC implícitamente. Builder 3 no
ejecuta baseline, no escribe ni despliega en `C:`; sólo ejecuta la toolchain instalada allí.

Cada cambio incluye implementación, pruebas nuevas enfocadas, evidencia Markdown y un commit pequeño.
El mismo commit que cierra una tanda actualiza `TRACKING.md` y `CURSOR.json`; queda prohibido diferir el estado
persistente a una tanda posterior.

### Cambio cerrado: visibilidad íntegra de la construcción

El cambio autorizado se limita a `nscbuilder3.py` y a la documentación de la
Tanda 10. Su objetivo es presentar de forma explícita el progreso que Ninja ya
emite durante la construcción de NSC, sin ocultar, resumir ni sustituir la
salida original.

Presupuesto del cambio:

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | NSC Builder 3 |
| Archivos funcionales | `nscbuilder3.py` |
| Símbolos | `CommandWorker` y la pestaña `Construcción` de `MainWindow` |
| Comportamiento | Contador `n/total`, barra y descripción del paso Ninja actual |
| Persistencia | `full.log` conserva literalmente toda la salida recibida |
| Validación | Revisión estática; la construcción y ejecución quedan en manos del usuario |

Permanecen protegidos el compilador, la suite nueva, la suite histórica, los
seis cambios preexistentes, `C:`, despliegues y remotos. El reporte condensado
continúa siendo una vista adicional y nunca reemplaza el log completo.

### Cambio cerrado: cancelación visible de construcción

El controlador ya puede cancelar el árbol de procesos que posee, pero la
pestaña `Construcción` no expone ese control. El cambio autorizado conecta un
botón de cancelación visible al mecanismo existente, sin alterar los comandos,
la compilación ni el cierre del proceso.

Presupuesto del cambio:

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | NSC Builder 3 |
| Archivos funcionales | `nscbuilder3.py` |
| Símbolos | Pestaña `Construcción`, `_set_busy` y `_cancel_active` |
| Comportamiento | Cancelar configuración o construcción activas desde su propia pestaña |
| Validación | Revisión estática; la cancelación real queda para una ejecución del usuario |

El botón de ejecución conserva su responsabilidad sobre la batería de pruebas;
el botón nuevo sólo se habilita para operaciones de construcción.

### Cambio cerrado: ubicación del control de cancelación

Por instrucción explícita del usuario, el botón `Cancelar construcción` debe
aparecer inmediatamente al lado de `Construir NSC`. Sólo se autoriza mover el
widget; su señal, estado habilitado y mecanismo de cancelación permanecen
idénticos.

### Cambio cerrado: `O0` predeterminado

El selector de optimización host de la construcción de NSC debe iniciar en
`O0`. El único cambio funcional autorizado es ordenar sus opciones como
`O0`, `O1`, `O2`, `O3`; no se modifica la selección de optimización de la suite
ni los comandos resultantes para una opción elegida explícitamente.

### Cambio cerrado: cierre inequívoco del progreso de construcción

La finalización correcta de una operación debe sustituir el último porcentaje
observado por un estado terminal inequívoco. El cambio se limita a la pestaña
`Construcción`: cuando el proceso termina correctamente, la barra alcanza su
máximo y muestra `TERMINADO — 100%`; si existía un total de Ninja, conserva
además el contador `total/total`. La etiqueta inferior deja de presentar el
último paso como si continuara activo.

Permanecen idénticos los comandos, la detección de progreso, la salida completa,
los artefactos generados y los estados de error o cancelación. No se infiere el
éxito por el porcentaje: únicamente el resultado terminal del proceso puede
activar este estado.

### Cambio cerrado: cierre de compilación del soporte Host.Sema

La primera construcción Debug de `nsc_component_tests` demostró dos defectos
locales de la suite nueva: siete módulos consumen `addSemaCase`, dos consumen
además `AnalyzerFixture` y `diagnosticCount`, pero `SemaTestSupport.h` no publica
esos tres contratos; `SemaEffectsExpressionModule.cpp` contiene además dos
inicializaciones interpretadas por C++ como declaraciones de función.

El presupuesto queda limitado a esos dos archivos de pruebas y a la evidencia
de la Tanda 02.07. Se restauran los contratos ya consumidos y se desambiguan las
dos inicializaciones con llaves. El compilador, CMake, manifiestos, oráculos,
IDs, disposiciones y demás módulos permanecen protegidos. La nueva construcción
queda en manos del usuario.

### Cambio cerrado: auditoría estática integral de `nsc_component_tests`

La solicitud vigente exige revisar el conjunto completo y no avanzar mediante
un error por corrida. El inventario canónico del target contiene `523` unidades
de traducción C++ entre la suite y las fuentes de producto enlazadas por ella.
Se auditan con las mismas opciones estrictas del build Debug, pero en modo de
sólo sintaxis: no se enlaza, no se genera el compilador, no se ejecutan casos y
no se modifican artefactos de producto.

Cada defecto demostrado se corrige en la fuente de prueba mínima. Permanecen
protegidos NSC, CMake, manifiestos, oráculos, IDs, disposiciones, la suite
histórica, los seis archivos preexistentes, `C:`, despliegues y remotos. El gate
termina cuando todas las unidades C++ del target aceptan la revisión de sintaxis
con `/W4 /WX` o quedan bloqueadas con evidencia explícita.

### Enmienda: cierre ABI de accesos privados en la suite

La corrida externa llegó a `337/337` objetos y demostró un fallo exclusivo de
enlace: las pruebas que incluyen cabeceras con `private` expuesto solicitan
símbolos `public`, mientras los cuerpos de producto compilados directamente
publican símbolos `private` bajo el ABI MSVC. El límite del linker mostró 20,
pero el cruce completo de referencias y definiciones detectó 49: 42 de
`SemanticAnalyzer`, 4 de `DominanceBuilder` y 3 de `DominanceDumper`.

Se autorizan únicamente `tests/compiler-suite/CMakeLists.txt`, cuatro shims de
producto de prueba —incluido el shim Parser ya existente como precedente— y la
documentación de Fase 2. Los shims compilan los cuerpos vigentes con la misma
declaración de acceso que sus pruebas. Permanecen protegidos todos los fuentes y
headers de producto, contratos, oráculos, IDs, disposiciones, manifiestos, la
suite histórica, los seis archivos preexistentes, `C:`, despliegues y remotos.
La validación de enlace y ejecución corresponde a la siguiente corrida externa.

### Enmienda: clausura completa del enlace Debug

La corrida externa posterior eliminó las `49` discrepancias ABI y llegó al
enlace final. El inventario de símbolos del archivo de respuesta real encontró
`26` símbolos NativeSharp sin productor. Se dividen en dos causas y se corrigen
en commits independientes:

1. `kCompilerVersion` y `kCompilerBuildId` son consumidos por
   `LanguageService.cpp` y `CompilerConsole.cpp`, pero faltaba el productor real
   `Version.cpp` y su `BuildId.h` generado.
2. Los otros `24` símbolos son la clausura transitiva de
   `WindowsLibraryBuilder.cpp`, incorporado por los casos ejecutables de
   `Host.Linker` sin sus productores LLVM, runtime y linker.

Presupuesto autorizado:

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | Infraestructura nueva `nsc_component_tests` y Builder 3 |
| Archivos funcionales | `tests/compiler-suite/CMakeLists.txt`, `nscbuilder3.py` |
| Comportamiento | Cerrar productores reales del enlace y retirar el argumento inválido enviado directamente a `lld-link` |
| Evidencia | Tabla de símbolos del `rsp` real y dos logs externos equivalentes |
| Validación | Cierre estático; nueva configuración, enlace y ejecución a cargo del usuario |

Permanecen protegidos el producto, las pruebas y oráculos, IDs, disposiciones,
manifiestos, suite histórica, seis archivos preexistentes, `C:`, despliegues y
remotos. Quedan prohibidos stubs de identidad, valores inventados y omitir casos
para conseguir que el ejecutable enlace.

### Enmienda: selecciones opcionales sin estado heredado

Builder 3 reutiliza los árboles de CMake de la suite. Cuando el usuario deja
vacíos el archivador o el NSC de comparación, la configuración vigente debe
retirar también cualquier selección almacenada por una corrida anterior.

El presupuesto queda limitado a `configure_commands` en `nscbuilder3.py` y a
la evidencia de Validación 06. Se permite emitir únicamente los dos `-U`
correspondientes a esas variables opcionales. Permanecen protegidos CMake, los
controladores, las pruebas, el producto, las demás opciones, los seis archivos
preexistentes, `C:`, despliegues y remotos. La configuración funcional queda
pendiente de la siguiente corrida externa.

### Enmienda: toolchain completa del grafo raíz

Los seis casos ejecutables `C12B-BLD-076` configuran árboles hijos del
repositorio principal. Deben fijar Ninja, modo, CMake, Clang, LLD, LLVM,
herramientas de recursos y el NSLink seleccionado; queda prohibido permitir que
el CMake hijo elija Visual Studio o una herramienta residual del entorno.

El presupuesto queda limitado a `run_buildsystem_gate.py`, la inscripción
`buildsystem-controller` de `tests/compiler-suite/CMakeLists.txt`,
`ExecutionEngine._run_buildsystem` en `nscbuilder3.py` y la evidencia de
Validación 07. Los seis casos, sus oráculos, el producto, los demás
controladores y los archivos protegidos permanecen intactos. No se configura ni
compila localmente; la validación corresponde a la corrida externa.

### Enmienda: bloqueo causal de consumidores self-host

Los cinco proyectos de fixture self-host son productores compartidos por 192
casos lógicos y 1,536 variantes concretas. Una compilación fallida debe
registrarse una sola vez por proyecto, modo y optimización; los consumidores
restantes conservan su identidad con estado `BLOCKED` y evidencia
`BLOQUEADO_POR`, sin reintentar el mismo productor ni convertirlos en éxito.

El presupuesto queda limitado a `ExecutionEngine._run_selfhost`, sus caches de
productor, el conjunto de estados terminales y el color de `BLOCKED` en
`nscbuilder3.py`, más la evidencia de Validación 08. Permanecen intactos los
fixtures, manifiestos, casos, oráculos, compilador, suite histórica y archivos
protegidos. No se compila ni ejecuta localmente.

### Enmienda: catálogo canónico descubierto dinámicamente

Builder 3 debe descubrir el denominador de NSC desde `implementation-waves.csv`,
`test-case-design.csv`, `suite-case-allocation.csv` y `cases.jsonl`. Los totales
vigentes son evidencia de una revisión, no precondiciones escritas en código.
Agregar o retirar legítimamente un caso o componente no debe exigir editar un
número esperado en el orquestador.

El presupuesto queda limitado a `CanonicalCatalog.load` y
`CanonicalCatalog._validate_sources` en `nscbuilder3.py`, más la evidencia de
Validación 09. Se sustituyen sólo los totales rígidos por invariantes
relacionales: fuentes no vacías, IDs únicos, componentes con diseño, paridad
entre diseño y asignación y ausencia de casos ejecutables ajenos al catálogo.
Permanecen intactos casos, contratos, oráculos, disposiciones, manifiestos,
compilador, suite histórica, archivos protegidos y totales documentales de la
revisión actual. No se compila ni ejecuta localmente.

### Enmienda: denominadores vivos de BuildSystem y Packaging

Los controladores de BuildSystem y Packaging deben validar el conjunto vigente
de contratos y casos por relaciones entre las fuentes canónicas. Queda
prohibido rechazar una revisión porque cambió un total, un rango ordinal, una
partición de perfiles o una lista de IDs escrita en el controlador.

El presupuesto queda limitado a
`run_buildsystem_gate.py`, `run_packaging_gate.py` y la evidencia de Validación
10. Los controladores deben exigir IDs únicos, referencias válidas, paridad de
contrato por caso, cobertura completa del catálogo, particiones sin traslape y
correspondencia exacta entre catálogo, clasificaciones y manifiesto. Las
contradicciones de perfil se derivan de los inventarios vivos. Permanecen
intactos los casos, contratos, oráculos, disposiciones, manifiestos, producto,
suite histórica y archivos protegidos. Los oráculos de cardinalidad funcional,
como producir exactamente un archivo de paquete, no forman parte de esta
enmienda y se conservan.

### Enmienda: inventario self-host común, primer grupo

Los gates de `SelfHost.Bootstrap`, `SelfHost.CodeGen`,
`SelfHost.Documentation` y `SelfHost.Project` deben derivar su denominador,
contratos, capas, perfiles y rangos desde los tres inventarios canónicos. No
pueden duplicar rangos ordinales, repartos por contrato ni cantidades de esta
revisión.

El presupuesto queda limitado a esos cuatro gates, un helper común de lectura
y validación relacional y la evidencia de Validación 11. Se conservan los
módulos C++, disposiciones, contradicciones, casos, oráculos, manifiestos y
producto. Los conflictos de perfil se mantienen sólo cuando se derivan de una
diferencia viva entre diseño y asignación. No se compila ni se ejecuta producto.

### Enmienda: inventario self-host común, controladores por rango

Los gates de `SelfHost.Driver`, `SelfHost.IR`, `SelfHost.Parser`,
`SelfHost.Projects`, `SelfHost.Sema`, `SelfHost.Symbols` y `SelfHost.Syntax`
deben derivar rangos, capas y perfiles desde el inventario común. Se permite
corregir únicamente el parser de descriptores cuando omita una fila realmente
materializada por diferencias de espacio o salto de línea.

El presupuesto queda limitado a esos siete gates, las funciones compartidas
de perfil/rango y la evidencia de Validación 12. Módulos, headers de soporte,
casos, contratos, oráculos, disposiciones, manifiestos y producto permanecen
intactos. No se configura ni compila producto.

### Enmienda: inventario self-host ejecutable y mixto

Los gates de `SelfHost.Basic`, `SelfHost.Diagnostics`, `SelfHost.Lexer`,
`SelfHost.Source` y `SelfHost.Types` deben derivar el denominador ejecutable,
las contradicciones, los pendientes de infraestructura y los defectos conocidos
desde inventarios, clasificaciones y manifiesto vivos. Se conservan sin cambios
la compilación de fixtures y todos los oráculos de ejecución.

El presupuesto queda limitado a esos cinco gates, validadores compartidos de
manifiesto/clasificación y la evidencia de Validación 13. No se ejecutan los
fixtures porque requieren la corrida externa autorizada; sólo se valida su
grafo estático completo.

### Enmienda: inventario de infraestructura self-host

Los gates de `SelfHost.Bootstrap.Infrastructure` y `SelfHost.Contracts` deben
derivar contratos, casos, rangos, fuentes y contradicciones de perfil desde los
inventarios y catálogos vivos. Queda prohibido condicionar una revisión a
cantidades, extremos ordinales o listas de rangos copiadas en los controladores.

El presupuesto queda limitado a ambos gates, el cargador del catálogo de
Bootstrap Infrastructure, validadores compartidos ya existentes y la evidencia
de Validación 14. Los módulos C++, contratos de producto, disposiciones,
oráculos, casos, manifiestos y producto permanecen intactos. No se configura ni
compila producto.

### Enmienda: clausura del esquema ejecutable de Fase 2

El cargador debe aceptar todos los valores canónicos que ya existen en el
manifiesto central y que pertenecen al vocabulario de Fase 2. Los timeouts deben
tener una única tabla compartida por esquema y ejecutores. Las dependencias de
caso participan en el grafo sólo cuando tienen forma de ID de caso; los nombres
de productores externos se preservan como requisitos, no como aristas falsas.

El presupuesto queda limitado al esquema, las tablas de timeout de ambos
ejecutores y sus pruebas unitarias, más la evidencia de Validación 15. Se
prohíbe cambiar casos, oráculos, resultados esperados, producto o batería
histórica. La validación carga el manifiesto y ejecuta sólo pruebas Python del
esquema; no compila ni ejecuta producto.

### Enmienda: denominadores vivos de gates host por componente

Los gates de componente host deben usar la misma carga, validación y selección
que el controlador productivo. Se prohíbe duplicar en cada gate rangos de IDs,
listas de exclusión, repartos por perfil o capa y cantidades de resultados que
dependan de una revisión concreta. Cada gate conserva la obligación de recibir
exactamente un resultado terminal por caso seleccionado y de cotejar los
estados con `expected_outcome`.

El presupuesto queda limitado al helper compartido de selección/resultados,
los quince gates host por componente, sus pruebas Python y la evidencia de
Validación 16. `Host.nsc`, que aún no tiene filas ejecutables, deriva su
inventario pendiente de los registros canónicos y conserva esa frontera. No se
modifican casos, oráculos, resultados esperados, producto ni batería histórica;
no se configura, compila ni ejecuta producto.

### Enmienda: cierre central de gates foundation por contrato

Los gates dirigidos de Host.Basic, Host.Diagnostics y Host.Source que invocan
`run_suite.py` por contrato no deben volver a validar una copia local del rango
de casos ni de los conteos. El selector central ya exige una selección no vacía,
el reporte exige un resultado terminal exacto por caso seleccionado y el código
de retorno sólo acepta `PASS` o `KNOWN_FAILURE`. Los gates conservan su contrato,
perfil, build mode, herramientas y ejecución; se retira únicamente la segunda
fuente de verdad de IDs y cantidades.

El presupuesto queda limitado a los veintisiete gates foundation que contienen
esa duplicación y la evidencia de Validación 17. El gate especial de ownership
de Host.Source queda fuera de esta tanda porque además valida el inventario y
registro completos del componente. No se modifican producto, casos, oráculos,
resultados esperados, selector, runner, reporte ni batería histórica. No se
configura, compila ni ejecuta producto.

### Enmienda: contrato exacto de timeouts en NSC Builder 3

Builder 3 debe reconocer exactamente las clases y duraciones publicadas por el
esquema central del manifiesto. Queda prohibido degradar una clase conocida o
inválida a `normal`: una clase ajena al contrato debe fallar antes de iniciar
el proceso.

El presupuesto queda limitado a `TIMEOUT_SECONDS`,
`ExecutionEngine._timeout_class`, una prueba Python y la evidencia de
Validación 32. No se modifican manifiestos, clases declaradas, casos, oráculos,
producto ni batería histórica; no se configura, compila ni ejecuta producto.

### Enmienda: precondiciones estáticas de rutas especializadas

Cada fila ejecutable debe proporcionar los campos que consume su ruta concreta
antes de iniciar procesos: proyectos y argumentos self-host, operaciones de
identidad y ordinales/variantes de BuildSystem y Packaging. Los proyectos de
fixture deben existir dentro de `tests/compiler-suite` y cada gate importado
debe reconocer la fila mediante exactamente una rama.

El presupuesto queda limitado a una prueba Python con dobles de las operaciones
externas y a la evidencia de Validación 33. No se ejecutan CMake, NSC, NSLink,
archivadores, fixtures ni procesos; no se modifican manifiestos, controladores,
casos, oráculos, producto ni batería histórica salvo que el cotejo demuestre un
defecto concreto.

### Enmienda: estado global cerrado de la evidencia de Builder 3

Un reporte final sólo puede calcularse a partir de uno o más resultados con
estados pertenecientes al protocolo de Builder 3. Un conjunto vacío o un estado
desconocido no puede convertirse en `PASSED`; debe impedir la publicación del
cierre y conservarse como error de infraestructura del worker.

El presupuesto queda limitado al dominio de estados y
`EvidenceStore._overall_status`, una prueba Python y la evidencia de Validación
34. No se modifica la ejecución de casos, resultados conocidos, GUI,
manifiestos, producto ni batería histórica; no se configura, compila ni ejecuta
producto.

### Enmienda: proyección determinista de reejecución de fallos

La reejecución debe derivarse del plan visible en su orden original, incluir
exactamente los estados terminales y rechazar resultados duplicados o claves
que no pertenecen al plan. `PASS` y `KNOWN_FAILURE` permanecen fuera del retry.

El presupuesto queda limitado a una función pura de proyección en
`nscbuilder3.py`, su uso por `_suite_finished`, una prueba Python y la evidencia
de Validación 36. No se modifican estados, casos, manifiestos, ejecución,
producto ni batería histórica; no se configura, compila ni ejecuta producto.

### Enmienda: cardinalidad terminal de la evidencia de Builder 3

Cuando la metadata de una corrida declara `variant_count`, la evidencia final
debe contener exactamente esa cantidad de claves terminales únicas. Una
omisión, duplicado o cardinalidad inválida debe impedir `results.json`,
`summary.md` y `RUN_FINISHED`; los eventos y log progresivos ya escritos se
conservan para diagnóstico.

El presupuesto queda limitado a `EvidenceStore`, una prueba Python que escribe
sólo en un temporal de `A:\nsc-devel\_nscbuilder3` y la evidencia de Validación
35. No se modifican motores, casos, manifiestos, GUI, producto ni batería
histórica; no se crea ningún proceso ni se configura, compila o ejecuta
producto.

### Enmienda: cierre central de gates Host.Language por contrato

Los veintiún gates dirigidos de Host.Language que ejecutan un contrato mediante
`run_suite.py` deben conservar su contrato, perfil, build mode, herramientas y
oráculos declarados, pero no repetir rangos de IDs ni cantidades de `PASS` y
`KNOWN_FAILURE`. El selector, ledger, reporte y código de retorno compartidos
son la única autoridad para la selección completa y su cierre terminal.

El presupuesto queda limitado a esos veintiún gates y la evidencia de
Validación 18. No se modifican características del lenguaje, producto, casos,
oráculos, disposiciones, resultados esperados, selector, runner, reporte ni
batería histórica. No se configura, compila ni ejecuta producto.

### Enmienda: clausura selectiva de dependencias externas

El selector debe conservar en cada caso los productores externos declarados,
pero sólo puede recorrer como aristas los valores que tengan forma de ID de
caso. Un ID de caso ausente continúa siendo error; nombres como `CMake`,
`LLVM 23`, `NSLink` o bibliotecas no se convierten en casos ficticios.

El presupuesto queda limitado a `nsc_suite/selector.py`, sus pruebas unitarias
y la evidencia de Validación 20. No se modifican el manifiesto, dependencias
declaradas, orden, filtros, producto, ejecutores, oráculos ni batería histórica.
No se configura, compila ni ejecuta producto.

### Enmienda: cierre central de gates Host.Target por contrato

Los treinta gates dirigidos de Host.Target que ejecutan un contrato mediante
`run_suite.py` deben conservar cada selector, perfil, build mode, herramienta y
oráculo de Target, pero no repetir rangos de casos ni cantidades de resultados.
El selector, ledger, reporte y código de retorno compartidos son la única
autoridad para la selección completa y su cierre terminal.

El presupuesto queda limitado a esos treinta gates y la evidencia de Validación
19. Se preservan íntegros los contratos y oráculos de target identity, ABI,
layout, alineación, bitfields, CPU, LLVM y registros de ensamblado. No se
modifican producto, casos, resultados esperados, selector, runner, reporte ni
batería histórica. No se configura, compila ni ejecuta producto.

### Enmienda: decisiones causales con productores externos

El libro de resultados debe distinguir las dependencias entre casos de los
productores externos declarados por un caso. Sólo los valores con forma de ID
de caso participan en espera y bloqueo causal; requisitos como `CMake`,
`LLVM 23` o `NSLink` permanecen en el manifiesto y en la evidencia, pero no
pueden producir una espera imposible dentro del ejecutor.

El presupuesto queda limitado a `nsc_suite/result.py`, sus pruebas unitarias,
una prueba dirigida del runner y la evidencia de Validación 21. Se preserva el
error causal cuando un productor que sí es un caso no fue seleccionado. No se
modifican casos, oráculos, resultados esperados, selector, manifiesto,
producto ni batería histórica. No se configura, compila ni ejecuta producto.

### Enmienda: partición canónica de Host.Source

El gate especial de ownership debe obtener el denominador completo de
`Host.Source` desde los inventarios canónicos y su partición ejecutable desde
el manifiesto central. Los cuatro casos que requieren infraestructura especial
se transcriben a una clasificación JSONL legible por máquina; el gate deja de
duplicar rangos, cantidades y listas privadas de IDs.

El presupuesto queda limitado al archivo de clasificación de `Host.Source`,
`run_host_source_file_ownership_gate.py` y la evidencia de Validación 22. La
clasificación no cambia disposiciones: sólo convierte en autoridad verificable
lo ya registrado por los módulos y la auditoría de bloqueos. No se modifican
casos, oráculos, resultados esperados, producto ni batería histórica. No se
configura, compila ni ejecuta producto.

### Enmienda: protocolo vivo del gate sintético

El gate de infraestructura sintética debe cargar y seleccionar su manifiesto
mediante los mismos componentes productivos que prueba. La cobertura de
resultados se coteja con las identidades y resultados esperados de esa selección;
la secuencia de eventos se valida por el protocolo `RUN_STARTED`, un inicio y
un cierre por caso, y `RUN_FINISHED`, con cardinalidades derivadas del conjunto
vivo y nunca de los números `3` u `8` de una revisión.

El presupuesto queda limitado a `run_infrastructure_gate.py`, una prueba Python
de sus validadores puros y la evidencia de Validación 23. No se modifican el
fixture, manifiesto central, casos, oráculos, resultados esperados, producto ni
batería histórica. No se configura, compila ni ejecuta producto.

### Enmienda: auditoría residual y cierre causal del manifiesto vivo

Los 118 controladores de integración se auditan por listas de IDs, conteos de
casos/resultados/eventos, hashes, versiones y cadenas exactas. Sólo se retiran
expectativas que dupliquen el inventario de pruebas; se conservan cardinalidades
y valores que constituyen oráculos observables del producto. Una prueba estática
permanente debe cargar el manifiesto real y demostrar que cada selección no
vacía por componente, perfil y build mode puede alcanzar cierre causal.

El presupuesto queda limitado a esa prueba de cierre vivo y la evidencia de
Validación 24. No se modifican controladores adicionales mientras el barrido no
demuestre una duplicación dependiente de revisión. No se modifican casos,
oráculos, resultados esperados, producto ni batería histórica. No se configura,
compila ni ejecuta producto.

### Enmienda: cierre estático del grafo de módulos C++

Cada archivo `*Module.cpp` de la suite debe tener exactamente una función de
registro y aparecer simultáneamente en el target `nsc_component_tests`, en las
declaraciones de `TestRegistry.h` y en las llamadas de `main.cpp`. Una prueba
estática deriva los cuatro conjuntos desde los archivos vivos y exige igualdad,
sin fijar el número de módulos ni sus nombres en una quinta lista.

El presupuesto queda limitado a esa prueba del grafo y la evidencia de
Validación 25. No se reordena ni modifica el target, el registro, los módulos,
casos, oráculos, producto o batería histórica salvo que la prueba demuestre una
omisión concreta. No se configura, compila ni ejecuta producto.

### Enmienda: cotejo global del registro con el inventario

Un gate único debe ejecutar `nsc_component_tests --list` y comparar el registro
completo con los casos canónicos de las familias `Host.*` y `SelfHost.*`, más
los casos del fixture sintético `Infrastructure`. El conjunto esperado se
deriva de los CSV y JSONL vivos; se prohíben rangos, listas y conteos privados.
El gate rechaza líneas mal formadas, duplicados, omisiones, casos inesperados y
cambios de contrato o componente.

El presupuesto queda limitado al nuevo gate global, su conexión CTest, sus
pruebas Python y la evidencia de Validación 26. No se modifican módulos, casos,
oráculos, disposiciones, producto ni batería histórica. La implementación se
valida estáticamente; la ejecución contra el host actualizado queda en el cierre
offline del usuario. No se configura ni compila producto.

### Enmienda: contrato de argumentos entre CTest y controladores

Cada bloque CTest que invoca un `run_*.py` debe proporcionar todas las opciones
que el parser del script declara `required=True` y no puede pasar opciones que
el parser desconoce. La comprobación deriva opciones mediante el AST Python y
extrae bloques `add_test` mediante un lector iterativo de paréntesis CMake; no
se mantiene una tabla manual de scripts u opciones.

El presupuesto queda limitado a la prueba estática de wiring y la evidencia de
Validación 27. Una discrepancia demostrada podrá corregirse únicamente en el
bloque CTest o parser causal correspondiente. No se modifican casos, oráculos,
resultados esperados, producto ni batería histórica. No se configura, compila
ni ejecuta producto.

### Enmienda: contrato de configuración de NSC Builder 3

Los comandos que Builder 3 prepara para NSC y para la infraestructura nueva
deben conservar separadas las configuraciones `Debug` y `Release`, propagar la
optimización host elegida, fijar el NSC y NSLink seleccionados, construir sólo
los targets autorizados y normalizar como rutas CMake todos los argumentos de
cache. Las opciones vacías deben retirar cualquier valor heredado del árbol.

El presupuesto queda limitado a una prueba Python de las funciones puras de
construcción de comandos y a la evidencia de Validación 28. `nscbuilder3.py`
sólo podrá modificarse si esa prueba demuestra una discrepancia concreta. No se
descubren herramientas reales, no se configura ni compila producto, no se abre
la GUI y no se ejecutan casos. Permanecen protegidos el compilador, CMake, los
manifiestos, oráculos, batería histórica, seis archivos preexistentes, `C:`,
despliegues y remotos.

### Enmienda: frontera de catálogo exclusiva del compilador

El catálogo y la superficie ejecutable de Builder 3 deben coincidir exactamente
con los componentes declarados en `implementation-waves.csv`. Esa autoridad
sólo puede contener componentes `Host.*`, `SelfHost.*`, `BuildSystem` y
`Packaging`; las bibliotecas `Library.*`, el SDK, tooling y terceros permanecen
en sus inventarios documentales y fuera de esta suite.

El presupuesto queda limitado a una prueba Python que derive ambos conjuntos
de los CSV y JSONL canónicos y a la evidencia de Validación 29.
`nscbuilder3.py` sólo podrá modificarse si el cotejo demuestra contaminación
real. No se eliminan inventarios externos, no se modifican casos, olas,
manifiestos, producto ni batería histórica; no se configura, compila ni ejecuta
producto y no se escribe en `C:`.

### Enmienda: matriz lógica de ejecución de NSC Builder 3

La selección granular y la expansión de variantes deben representar exactamente
las filas canónicas: los perfiles son acumulativos, cada alcance devuelve sólo
su partición y la matriz final es el producto de modos y optimizaciones que cada
caso declara. Una disposición no ejecutable nunca puede producir una variante
ni un resultado implícito.

El presupuesto queda limitado a una prueba Python sobre el catálogo vivo y a
la evidencia de Validación 30. `nscbuilder3.py` sólo podrá modificarse si el
cotejo demuestra una discrepancia concreta. No se fijan conteos como condición,
no se modifican inventarios, manifiestos, disposiciones, oráculos, producto ni
batería histórica; no se configura, compila ni ejecuta producto.

### Enmienda: despacho cerrado de ejecutores en NSC Builder 3

Builder 3 debe aceptar únicamente los ejecutores que la infraestructura actual
implementa. `nsc_component_tests` se dirige al host consolidado y
`nsc_suite_controller+nsc` se especializa para self-host, BuildSystem,
Packaging o identidad. Cualquier otro valor, incluido `historical-weekly`,
debe terminar como error de infraestructura sin invocar un controlador por
caída.

El presupuesto queda limitado a `ExecutionEngine._run_variant`, una prueba
Python del despacho completo y la evidencia de Validación 31. No se modifica el
esquema, manifiesto, casos, oráculos, producto ni batería histórica; no se
configura, compila ni ejecuta producto.

### Enmienda: frontera de rutas y recibos de NSC Builder 3

Toda raíz escribible de Builder 3 debe permanecer en `A:`. El recibo del
compilador debe identificar exactamente los archivos NSC y NSLink seleccionados,
con ruta resuelta, tamaño, fecha y SHA-256, sin sustituirlos por herramientas de
otra ubicación.

El presupuesto queda limitado a una prueba Python de `BuilderConfig.validated`,
`require_a_path`, `file_receipt` y `compiler_receipt`, más la evidencia de
Validación 37. `nscbuilder3.py` sólo podrá modificarse si el cotejo demuestra una
discrepancia concreta. Los archivos de prueba temporales se crean exclusivamente
bajo `A:\nsc-devel\_nscbuilder3`; no se descubre ni ejecuta ninguna herramienta,
no se configura, compila o ejecuta producto y no se escribe en `C:`.

### Enmienda: selección explícita de herramientas en NSC Builder 3

Iniciar una configuración o construcción no autoriza volver a detectar NSC,
NSLink ni LLVM. Una ruta escrita o ya visible en la interfaz debe validarse tal
como fue seleccionada y, si es inválida, producir un error; queda prohibido
sustituirla silenciosamente por un fallback. La sustitución sólo pertenece al
botón explícito de autodetección y a la migración inicial de configuración.

El presupuesto queda limitado a `_start_infrastructure` en `nscbuilder3.py`, una
prueba AST de la conexión de GUI y la evidencia de Validación 38. No se cambia
la heurística de descubrimiento, los comandos, rutas predeterminadas, recibos,
casos o producto; no se abre la GUI ni se configura, compila o ejecuta producto.

### Enmienda: integridad del historial de NSC Builder 3

Una fila del historial sólo puede mostrar un estado terminal cuando metadata,
resultados, cardinalidad, conteos y evento final pertenecen a la misma corrida y
concuerdan entre sí. La ausencia de cierre se presenta como `INCOMPLETE`; una
estructura contradictoria o desconocida se presenta como `MALFORMED`. Ninguna
de ambas puede aparecer como `PASSED`.

El presupuesto queda limitado a un lector puro de evidencia en `nscbuilder3.py`,
su consumo por `_refresh_history`, pruebas Python con directorios sintéticos
bajo `A:\nsc-devel\_nscbuilder3` y la evidencia de Validación 39. No se altera
la escritura de evidencia, ejecución, GUI restante, casos, producto o historial
existente; no se abre la GUI ni se configura, compila o ejecuta producto.

### Enmienda: cierre terminal de cancelación en NSC Builder 3

Cancelar una operación de infraestructura debe producir evidencia terminal
`CANCELLED` para el comando activo y para cada comando todavía no iniciado. No
puede convertir la cancelación en `FAIL`, dejar una corrida vacía o publicar un
estado global cancelado sin ninguna fila cancelada que lo sustente.

El presupuesto queda limitado a dos proyecciones puras de resultados y su uso
por `CommandWorker` en `nscbuilder3.py`, pruebas Python sin procesos reales y la
evidencia de Validación 40. No se modifica `ProcessController`, la terminación
del árbol, comandos, ejecución de casos, producto ni GUI restante; no se abre la
GUI ni se configura, compila o ejecuta producto.

### Enmienda: terminación confirmada del árbol de procesos

`ProcessController` no puede considerar terminado un proceso sólo porque invocó
`taskkill`. Debe comprobar su salida y, si `taskkill` falla o la espera vence,
aplicar `kill` al proceso hijo conocido y volver a esperar. Un proceso ya
terminado no recibe ninguna acción.

El presupuesto queda limitado a `ProcessController._terminate_tree`, pruebas
Python con un doble de `Popen` y la evidencia de Validación 41. No se crean
procesos reales, no se cambia el bucle de lectura, los comandos, estados,
ejecución de casos o producto; no se abre la GUI ni se configura, compila o
ejecuta producto.

### Enmienda: diagnósticos progresivos de control de procesos

Los mensajes generados por `ProcessController` —cancelación anterior al
lanzamiento, cancelación activa y timeout— deben pasar por el mismo callback de
salida que alimenta `full.log`. No basta conservarlos en el texto de retorno,
porque `CommandWorker` no usa ese texto como evidencia progresiva.

El presupuesto queda limitado a `ProcessController.run`, pruebas Python con un
doble de `Popen` y la evidencia de Validación 42. No se cambia la terminación,
los códigos, comandos, estados, casos, producto o GUI; no se crea ningún proceso
real ni se configura, compila o ejecuta producto.

### Enmienda: aislamiento de artefactos host por modo

La ejecución consolidada debe resolver `nsc_component_tests.exe` exclusivamente
dentro del árbol correspondiente al modo solicitado. Debug no puede consumir un
artefacto Release ni Release uno Debug. Si no existe ninguna disposición CMake
soportada en ese árbol, la ruta esperada se conserva para el error de preflight,
sin fallback a otro modo.

El presupuesto queda limitado a pruebas Python de `effective_build_dir`,
`host_executable` y `compiler_build_dir`, más la evidencia de Validación 43.
`nscbuilder3.py` sólo podrá modificarse si el cotejo demuestra una discrepancia.
Los artefactos sintéticos se crean bajo `A:\nsc-devel\_nscbuilder3`; no se abre
la GUI ni se configura, compila o ejecuta producto.

### Enmienda: envolvente cerrada del host consolidado

La salida de `nsc_component_tests --case` debe contener exactamente un objeto
JSON cuya identidad completa coincida con el caso solicitado. Sólo se aceptan
`PASS` con exit `0` y `FAIL` con exit `1`; componente, contrato, duración y lista
de fallos deben tener su forma contractual. Una envolvente malformada es error
de infraestructura y nunca se traduce a `KNOWN_FAILURE`.

El presupuesto queda limitado a un parser puro, `ExecutionEngine._run_host`,
pruebas Python sintéticas y la evidencia de Validación 44. No se modifica el
host C++, sus casos, oráculos, estados esperados, producto o manifiestos; no se
ejecuta ningún binario ni se configura, compila o ejecuta producto.

### Enmienda: envolvente cerrada del controlador de identidad

Los casos de identidad delegados a `run_suite.py` sólo pueden adoptar un estado
terminal cuando `results.json` conserva el esquema canónico, identifica la misma
corrida, revisión, modo, optimización y caso solicitados, contiene exactamente un
resultado bien formado y concuerda con el código de salida del controlador. Un
JSON truncado, una identidad ajena, un estado desconocido, evidencia de tipo
incorrecto o una contradicción proceso/reporte es un error de infraestructura.

El presupuesto queda limitado a un parser puro, `ExecutionEngine._run_identity`,
pruebas Python sintéticas y la evidencia de Validación 45. No se modifica
`run_suite.py`, el controlador canónico, sus casos, oráculos, manifiestos,
producto ni batería histórica; no se ejecuta ningún binario ni se configura,
compila o ejecuta producto.

### Enmienda: envolvente cerrada de gates Python

Los gates importados de BuildSystem y Packaging deben devolver exactamente una
tupla `(bool, str)`. Builder 3 no puede convertir por veracidad cadenas,
números, contenedores ni objetos a un resultado de producto. Una forma de
retorno inválida o una excepción del gate es un error de infraestructura y debe
conservar el directorio del caso y sus logs como evidencia.

El presupuesto queda limitado a un parser puro,
`ExecutionEngine._run_python_gate_case`, pruebas Python con dobles y la
evidencia de Validación 46. No se modifican los gates importados, sus rutas,
casos, oráculos, manifiestos, producto ni batería histórica; no se ejecuta
ningún gate real ni se configura, compila o ejecuta producto.
