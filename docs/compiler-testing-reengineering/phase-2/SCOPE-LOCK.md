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

### Enmienda: oráculo self-host cerrado antes de ejecutar

Los `192` casos self-host declaran un exit code entero, una descripción textual
opcional y, en siete casos, un objeto JSON esperado. Builder 3 debe validar esa
forma antes de preparar o ejecutar el fixture; queda prohibido convertir
cadenas o booleanos mediante `int(...)` o aceptar un oráculo JSON de otro tipo.

El presupuesto queda limitado a un parser puro, la lectura del oráculo en
`ExecutionEngine._run_selfhost`, pruebas Python sintéticas y la evidencia de
Validación 47. No se modifican manifiestos, fixtures, compilación, comparación
JSON, estados esperados, producto ni batería histórica; no se prepara o ejecuta
ningún fixture ni se configura, compila o ejecuta producto.

### Enmienda: entradas self-host cerradas antes de preparar productores

Los casos self-host sólo pueden referir un proyecto `.nsproj` relativo y
existente dentro de `tests/compiler-suite`, un argumento de caso textual no
vacío y el sandbox opcional exacto `fresh-case-directory`. Builder 3 no puede
convertir otros tipos mediante `str(...)`, aceptar rutas absolutas o externas,
ni interpretar por veracidad un valor de sandbox desconocido. La identidad de
los cinco productores vivos se audita también por ruta y nombre base; cualquier
colisión futura debe bloquearse antes de compartir evidencia.

El presupuesto queda limitado a un parser puro, su consumo por
`ExecutionEngine._run_selfhost`, pruebas Python sintéticas y la evidencia de
Validación 48. No se cambia la identidad de workspace porque el inventario vivo
no presenta colisiones; no se modifican manifiestos, fixtures, comandos,
producto ni batería histórica y no se prepara o ejecuta ningún fixture.

### Enmienda: cancelación y timeout no son defectos de producto

La terminación solicitada por el usuario debe producir `CANCELLED` para la
variante activa sin importar el resultado parcial de su ejecutor. Un timeout
detectado por `ProcessController` debe producir `INFRASTRUCTURE_ERROR`; ninguno
puede convertirse en `FAIL`, `KNOWN_FAILURE` o `UNEXPECTED_PASS` del compilador.
La causa de control se conserva separada del exit code del hijo.

El presupuesto queda limitado a registrar la causa de terminación en
`ProcessController`, clasificarla al cerrar `ExecutionEngine._run_variant`,
pruebas Python con dobles y la evidencia de Validación 49. No se cambian
timeouts, comandos, oráculos, procesos reales, producto ni batería histórica;
no se crea, prepara o ejecuta ningún proceso o fixture real.

### Enmienda: tipo exacto de rutas configuradas

Una ruta existente no basta para demostrar que es una herramienta. NSC,
NSLink, archivador, NSC de comparación y build-id header deben ser archivos;
`LLVM_DIR` debe ser un directorio. Las tres raíces escribibles siguen limitadas
a `A:` y, si ya existen, deben ser directorios. Una ruta explícita con tipo
incorrecto debe fallar antes de configurar o ejecutar comandos.

El presupuesto queda limitado a validadores puros de archivo/directorio,
`BuilderConfig.validated`, pruebas Python con artefactos sintéticos bajo
`_nscbuilder3` y la evidencia de Validación 50. No se redetectan herramientas,
no se cambian defaults o comandos y no se configura, compila o ejecuta producto.

### Enmienda: descubrimiento por tipo de artefacto

El descubrimiento automático debe seleccionar ejecutables sólo mediante
`is_file()` y directorios de configuración sólo mediante `is_dir()`. Se conserva
el orden exacto de candidatos y los fallbacks existentes; una ruta del tipo
incorrecto se omite, nunca se publica como herramienta encontrada.

El presupuesto queda limitado a separar los selectores `first_existing` por
tipo, actualizar sus consumidores, pruebas Python sintéticas bajo
`_nscbuilder3` y la evidencia de Validación 51. No se cambian candidatos,
prioridades, defaults, comandos o validación posterior y no se descubre o
ejecuta ninguna herramienta real.

### Enmienda: esquema cerrado de configuración persistida

`config.json` debe ser un objeto con únicamente los campos de `BuilderConfig`.
Las rutas son cadenas y `jobs` es un entero no booleano; campos desconocidos o
tipos distintos deben producir un diagnóstico causal durante la carga, nunca ser
ignorados ni propagarse hasta la GUI o la construcción de comandos.

El presupuesto queda limitado a un parser puro, `BuilderConfig.load`, pruebas
Python con un archivo sintético bajo `_nscbuilder3` y la evidencia de Validación
52. No se lee o modifica la configuración real, no se cambia autodetección,
defaults, guardado, comandos o GUI y no se ejecuta producto.

### Enmienda: una sola autoridad para el manifiesto ejecutable

Builder 3 no debe interpretar por segunda vez `cases.jsonl` con conversiones
silenciosas. Antes de proyectar el catálogo gráfico debe someter el manifiesto,
los componentes y los contratos al cargador canónico estricto de
`tests/compiler-suite/controller/nsc_suite/manifest.py`; un tipo, enumeración,
propietario, dependencia o esquema inválido debe bloquear la carga con el mismo
diagnóstico causal que usa el controlador de consola. Tras esa validación se
prohíbe convertir `case_id`, `executor`, `expected_outcome` u optimizaciones con
`str(...)`.

El presupuesto queda limitado a un adaptador puro hacia el cargador canónico,
`CanonicalCatalog.load`, pruebas Python sintéticas y la evidencia de Validación
53. No se reescribe el validador, no se modifican manifiestos, inventarios,
selección, comandos, producto ni batería histórica y no se configura, compila o
ejecuta producto.

### Enmienda: esquema mínimo de los CSV del catálogo

Los tres CSV que construyen el catálogo gráfico deben declarar sin duplicados
todos los encabezados que Builder 3 consume, contener exactamente una celda por
encabezado en cada fila y publicar valores no vacíos para esos campos. La ola
debe ser un decimal positivo canónico; queda prohibido que `int(...)` acepte
espacios, signos, ceros iniciales o formas no documentadas. Se permiten
encabezados adicionales para que el inventario pueda evolucionar sin fijar un
conteo de columnas ni de filas.

El presupuesto queda limitado a un lector CSV puro, un parser de ola,
`CanonicalCatalog.load`, pruebas Python sintéticas y la evidencia de Validación
54. No se modifican los CSV, sus dominios semánticos, conteos, selección,
manifiestos, producto ni batería histórica y no se configura, compila o ejecuta
producto.

### Enmienda: dominios cerrados de la asignación del compilador

Las filas de `suite-case-allocation.csv` pertenecientes a los componentes del
compilador no pueden publicar perfiles, capas, ejecutores, modos,
optimizaciones o aislamientos que Builder 3 no entiende. Un perfil desconocido
se excluye hoy por el fallback de selección y un modo u optimización desconocido
no genera variantes; ambos son omisiones silenciosas. `FROZEN_CONTRACT` sólo
puede aparecer como valor único de su campo y `N/A` no puede mezclarse con
optimizaciones concretas.

El presupuesto queda limitado a dominios consumidos por Builder 3, un validador
puro, `CanonicalCatalog.load`, pruebas Python sintéticas y la evidencia de
Validación 55. El manifiesto conserva autoridad sobre filas ejecutables; no se
exige paridad entre las dos fuentes, no se cambian CSV, selección, manifiestos,
producto ni batería histórica y no se configura, compila o ejecuta producto.

### Enmienda: proyección ejecutable desde el manifiesto validado

Para una fila ejecutable, `CaseRecord` debe tomar capa, perfil, modos,
optimizaciones, ejecutor y resultado esperado del `CaseManifest` ya validado,
no de copias más débiles en la asignación ni del objeto JSON crudo. La fila
cruda se conserva únicamente para `inputs` y `oracle`, cuyos esquemas dependen
del ejecutor. Las filas no ejecutables siguen usando la asignación validada.

El presupuesto queda limitado a `CanonicalCatalog.load`, su normalizador de
optimizaciones no ejecutables, pruebas Python estáticas y la evidencia de
Validación 56. No se reconcilian las diferencias documentales entre fuentes,
no se cambian manifiestos, asignaciones, selección, variantes, producto ni
batería histórica y no se configura, compila o ejecuta producto.

### Enmienda: identidad obligatoria en catálogos de disposición

Cada fila JSONL descubierta como catálogo de casos debe declarar exactamente
una identidad soportada (`case_id`, `case_ids`, `affected_case_ids` o
`case_range`) y una disposición conocida o pertenecer al único catálogo cuya
disposición se infiere de forma documentada. Rangos descendentes, prefijos o
anchos distintos, listas vacías o duplicadas y disposiciones desconocidas deben
fallar; una fila documental sólo puede omitirse en los cuatro archivos
metadato explícitos ya inventariados. Ninguna identidad ajena al diseño puede
quedar sin consumidor.

El presupuesto queda limitado al parser de disposiciones,
`CanonicalCatalog._load_dispositions`, su cierre contra el diseño, pruebas
Python sintéticas y la evidencia de Validación 57. Se conserva la precedencia
de una clasificación específica sobre `Executable`; no se modifican catálogos,
manifiestos, selección, producto ni batería histórica y no se configura,
compila o ejecuta producto.

### Enmienda: procesos de gates Python bajo el controlador cancelable

Los gates importados de BuildSystem y Packaging no pueden ejecutar
`subprocess.run` fuera de `ProcessController`: de hacerlo, el botón Cancelar no
posee el proceso, no puede terminar su árbol y el timeout queda desconectado de
la clasificación central. Builder 3 debe inyectar temporalmente un runner que
preserve por separado stdout y stderr, respete el timeout solicitado y restaure
el módulo incluso ante excepción. La captura debe seguir alimentando la consola
progresiva y conservar el `CompletedProcess` que esperan los oráculos.

El presupuesto queda limitado a un método de captura separada en
`ProcessController`, la inyección de `run_process` en
`ExecutionEngine._run_python_gate_case`, pruebas Python con dobles y la
evidencia de Validación 58. No se modifican gates, comandos, oráculos, timeouts,
producto ni batería histórica y no se crea o ejecuta ningún proceso real.

### Enmienda: configuración durable y acotada

`jobs` debe pertenecer a `1..256` desde que se interpreta `config.json`; no
puede llegar a la GUI para que un control visual lo recorte silenciosamente.
El guardado debe validar primero la configuración y publicar JSON mediante un
reemplazo atómico en el mismo directorio, de modo que una interrupción no
destruya la última configuración válida ni deje temporales. Cargar y guardar
deben conservar exactamente el payload persistido antes de la autodetección.

El presupuesto queda limitado a `parse_builder_config_payload`, un escritor
JSON atómico, `BuilderConfig.load/save`, pruebas Python bajo un directorio
sintético de `_nscbuilder3` y la evidencia de Validación 59. No se lee o
modifica el `config.json` real, no se cambian defaults, autodetección, comandos,
GUI, producto ni batería histórica y no se configura, compila o ejecuta
producto.

### Enmienda: publicación terminal atómica de evidencia

`metadata.json`, `results.json` y `summary.md` son marcadores durables de una
corrida y no pueden hacerse visibles a medio escribir. Deben publicarse por
reemplazo atómico; `RUN_FINISHED` sólo puede agregarse después de que resultados
y resumen estén completos. Si cualquiera falla, no existe evento terminal y el
historial debe seguir clasificando la corrida como incompleta, conservando
`events.jsonl` y `full.log` progresivos.

El presupuesto queda limitado a escritores atómicos de texto/JSON,
`EvidenceStore.__init__/finish`, pruebas Python en un historial sintético y la
evidencia de Validación 60. No se modifica el historial real, contenido de
reportes, estados, GUI, producto ni batería histórica y no se configura,
compila o ejecuta producto.

### Enmienda: causa exacta de productores self-host

La preparación de un fixture self-host no puede reducir cancelación, timeout,
exit code no cero y ausencia de artefacto al mismo mensaje genérico. Debe
diagnosticar la causa observada por `ProcessController`; un timeout del productor
se conserva en el primer `INFRASTRUCTURE_ERROR` y en cada consumidor posterior
como `BLOQUEADO_POR`. La cancelación sigue dominando la corrida y no se convierte
en defecto del compilador.

El presupuesto queda limitado a `_prepare_selfhost_fixture`, el cache causal de
`_run_selfhost`, pruebas Python con dobles y la evidencia de Validación 61. No se
modifican fixtures, comandos, timeout, oráculos, producto ni batería histórica y
no se crea o ejecuta ningún proceso real.

### Enmienda: cierre estático sin promoverlo a validación funcional

El cierre de esta auditoría debe inventariar mecánicamente propietarios de
procesos, conversiones de entrada, productores de estado, escrituras durables y
autoridades de catálogo. Sólo puede declarar cero defectos estáticos residuales
en esas fronteras si el catálogo no mutante, la matriz relacional, la GUI smoke
y toda la batería Python pasan. Eso no demuestra que NSC, NSLink ni los `12542`
variantes ejecutables funcionen: dicha afirmación queda reservada a la corrida
funcional externa solicitada por el usuario.

El presupuesto queda limitado a auditoría y evidencia Markdown de Validación
62. No se modifica código, no se configura o compila producto, no se ejecutan
casos NativeSharp, no hay baseline, despliegue ni escritura en `C:`.

### Enmienda: enteros de ruta sin coercion en gates de integracion

Los gates de BuildSystem y Packaging no pueden convertir con `int(...)` los
campos `contract_ordinal` y `variant` del manifiesto. Una cadena, un flotante o
un booleano no son enteros validos del contrato aunque Python pueda
convertirlos; aceptar `true` como `1` puede ejecutar una rama distinta y
ocultar la corrupcion del manifiesto. Ambos gates deben exigir presencia y
tipo entero exacto antes de seleccionar cualquier operacion.

El presupuesto queda limitado a un validador puro compartido en
`component_gate_support.py`, su uso en los dos `run_case`, pruebas Python con
payloads sinteticos y la evidencia de Validacion 63. No se cambian
manifiestos, rutas, comandos, oraculos, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: resultados de controladores sin coercion

El cierre comun de resultados no puede convertir identidades mediante
`str(...)` ni conteos mediante `int(...)`. Cada `case_id` debe ser una cadena
exacta y cada conteo un entero exacto no negativo; en particular, `true`,
`"1"` y `1.0` no pueden acreditar un caso o un resultado. El gate sintetico de
infraestructura debe reutilizar esa misma autoridad en vez de conservar una
copia permisiva.

El presupuesto queda limitado a `assert_component_results`, la delegacion de
`run_infrastructure_gate.validate_results`, pruebas Python sinteticas y la
evidencia de Validacion 64. No se cambian resultados validos, seleccion,
ejecucion, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: protocolo de eventos con tipos exactos

El gate sintetico de infraestructura no puede aceptar booleanos como numeros
de secuencia, flotantes como cardinalidades ni convertir identidades de caso
con `str(...)`. Cada evento debe ser objeto, cada secuencia un entero exacto,
cada identidad de caso una cadena exacta y los payloads terminales objetos con
conteos enteros exactos. La cardinalidad, orden y cobertura existentes se
conservan.

El presupuesto queda limitado a `run_infrastructure_gate.validate_events`,
pruebas Python con eventos sinteticos y la evidencia de Validacion 65. No se
cambia el productor de eventos, manifiestos, seleccion, producto ni bateria
historica y no se configura, compila o ejecuta producto.

### Enmienda: exit code self-host sin coercion

Los cinco gates self-host ejecutables no pueden convertir `oracle.exit_code`
mediante `int(...)`. El oraculo debe existir como objeto y su exit code debe ser
un entero exacto; cadenas, flotantes y booleanos son corrupcion de
infraestructura, no resultados del producto. La validacion debe vivir en la
autoridad compartida de inventario y ser consumida por Basic, Diagnostics,
Lexer, Source y Types.

El presupuesto queda limitado a `selfhost_inventory.required_oracle_exit_code`,
sus cinco consumidores, pruebas Python sinteticas y la evidencia de Validacion
66. No se cambian valores de oraculo, fixtures, comandos, producto ni bateria
historica y no se configura, compila o ejecuta producto.

### Enmienda: resumen Packaging sin coercion

Los conteos declarados por contrato en `PackagingContractCatalog.jsonl` no
pueden convertirse mediante `int(...)`. `required`, `known_product_defect` e
`infrastructure_pending` deben existir como enteros exactos no negativos antes
de compararse con el conjunto derivado; cadenas, flotantes y booleanos no
acreditan cobertura.

El presupuesto queda limitado a un lector puro compartido, los tres campos de
`run_packaging_gate.validate_classifications`, pruebas Python sinteticas y la
evidencia de Validacion 67. No se cambian catalogos, conteos derivados,
clasificaciones, producto ni bateria historica y no se configura, compila o
ejecuta producto.

### Enmienda: igualdad JSON tipada e iterativa

El oraculo JSON de SelfHost.Diagnostics no puede usar la igualdad permisiva de
Python, donde `true == 1` y `1 == 1.0`. Debe comparar estructura, tipos y
valores exactos en objetos y arreglos anidados. El recorrido debe ser
iterativo, mediante una lista de trabajo explicita, sin recursion.

El presupuesto queda limitado a un comparador puro en `selfhost_inventory`, su
uso por `run_selfhost_diagnostics_gate.oracle_satisfied`, pruebas Python
sinteticas y la evidencia de Validacion 68. No se cambian oraculos, fixtures,
salidas, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: CaseResult con tipos e invariantes completos

`CaseResult` no puede depender de anotaciones no verificadas. La identidad debe
ser cadena no vacia, el estado una instancia de `CaseStatus`, la duracion un
numero finito no negativo que no sea booleano y `failures`/`blocked_by` tuplas
de cadenas no vacias. Los bloqueadores no pueden repetirse. Las invariantes
causales existentes de PASS, BLOCKED y estados con evidencia se conservan.

El presupuesto queda limitado a `CaseResult.__post_init__`, pruebas Python
sinteticas y la evidencia de Validacion 69. No se cambian productores, ledger,
reportes, estados permitidos, producto ni bateria historica y no se configura,
compila o ejecuta producto.

### Enmienda: evidencia host no vacia

`HostExecutor` debe rechazar una lista `failures` que contenga cadenas vacias
antes de construir `CaseResult`. Un payload host mal formado debe producir
`INFRASTRUCTURE_ERROR` con diagnostico, no propagar la excepcion de la
invariante central. La forma valida y las relaciones status/exit code no
cambian.

El presupuesto queda limitado a la validacion de `failures` en `HostExecutor`,
una prueba Python sintetica y la evidencia de Validacion 70. No se cambian
binarios host, comandos, estados, producto ni bateria historica y no se
configura o compila producto.

### Enmienda: retorno de executor con tipo causal

`SuiteRunner._execute` debe verificar que el executor retorne un `CaseResult`
antes de leer su identidad. `None`, cadenas u otros objetos deben materializar
un `INFRASTRUCTURE_ERROR` para el caso y permitir que el ledger y el flujo de
eventos cierren; no pueden escapar como `AttributeError` y truncar la corrida.

El presupuesto queda limitado a la frontera de retorno en `_execute`, una
prueba Python sintetica y la evidencia de Validacion 71. No se cambian
executors, orden, dependencias, estados, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: run_id validado antes de escribir evidencia

El controlador no puede construir la ruta de `events.jsonl` mediante
`work_root / run_id` antes de validar el segmento. `WorkspaceManager` debe
resolver una unica raiz de corrida con la misma politica segura usada por los
casos, y esa ruta debe alimentar eventos y reporte. Un `run_id` vacio, oculto,
anidado o con traversal debe fallar antes de crear archivos.

El presupuesto queda limitado a `WorkspaceManager.run_root`, su uso por
`prepare` y `run_suite.run`, pruebas Python sinteticas y la evidencia de
Validacion 72. No se cambian layouts validos, seleccion, ejecucion, producto ni
bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: razon de cancelacion tipada

`CancellationToken.request` debe aceptar solamente una cadena con contenido
no blanco. Numeros, booleanos, `None`, vacio o espacios deben fallar antes de
activar el evento y sin reemplazar la autoridad de la primera razon valida.
Esto evita que una razon no textual alcance `CaseResult.failures` y rompa el
cierre causal de casos pendientes.

El presupuesto queda limitado a `CancellationToken.request`, pruebas Python
sinteticas y la evidencia de Validacion 73. No se cambia la semantica de
cancelacion, runner, procesos, producto ni bateria historica y no se configura,
compila o ejecuta producto.

### Enmienda: EventWriter con esquema previo a serializacion

`EventWriter.emit` debe validar que `kind` sea `EventKind`, `case_id` sea `None`
o cadena no vacia y `payload` sea `None` u objeto. No puede convertir
contenedores falsos como `[]` en `{}` mediante `payload or {}` ni aceptar
identidades numericas. Una entrada invalida debe fallar antes de escribir una
linea o avanzar la secuencia.

El presupuesto queda limitado a `EventWriter.emit`, pruebas Python sinteticas
y la evidencia de Validacion 74. No se cambian eventos validos, flush/fsync,
runner, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: RUN_FINISHED posterior a reportes

`RUN_FINISHED` no puede publicarse antes de que `results.json`, `summary.md` y
`failures.log` existan completamente. `SuiteRunner` debe aceptar un publicador
terminal que reciba el conjunto completo de resultados y ejecutarlo antes del
evento final. Si el publicador falla, la excepcion se conserva, el stream
progresivo queda disponible y no aparece un cierre terminal falso.

El presupuesto queda limitado al callback terminal de `SuiteRunner.run`, la
reordenacion de metadata/reporte en `run_suite.run`, pruebas Python sinteticas
y la evidencia de Validacion 75. No se cambia contenido de reportes, ejecucion
de casos, estados, producto ni bateria historica y no se configura, compila o
ejecuta producto.

### Enmienda: identidad terminal validada al construir

`ToolIdentity` y `RunMetadata` deben rechazar forma invalida al construirse,
antes de que el controlador abra eventos. Nombre y ruta de herramienta deben
ser cadenas no vacias, SHA-256 debe ser hexadecimal minusculo de 64 caracteres,
los dominios de modo/optimizacion/perfil/alcance deben ser canonicos y la
seleccion debe ser una tupla no vacia de IDs unicos. Herramientas repetidas o
objetos ajenos no pueden serializarse.

El presupuesto queda limitado a los `__post_init__` de ambas dataclasses,
pruebas Python sinteticas y la evidencia de Validacion 76. No se cambian hashes,
rutas, contenido de reportes, producto ni bateria historica y no se configura,
compila o ejecuta producto.

### Enmienda: protocolo minimo de identidad obligatorio

`CompilerIdentityExecutor` debe validar que toda respuesta `NSCOMPILER 1`
contenga valores no vacios para `compiler_version`, `compiler_release`,
`language`, `stage` y `build` antes de evaluar cualquier operacion. La prueba
de identidades distintas no puede acreditar como diferencia la ausencia de un
campo en uno de los compiladores.

El presupuesto queda limitado a la frontera `_identity`, una prueba Python
sintetica y la evidencia de Validacion 77. No se cambian comandos, oraculos,
producto ni bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: filtros de seleccion sin omisiones silenciosas

`select_cases` debe rechazar cada ID de caso, contrato, componente o capa que
no exista en el universo recibido, incluso cuando el mismo filtro incluya otro
valor valido. Una solicitud parcial no puede ejecutar un subconjunto y omitir
silenciosamente identidades desconocidas.

El presupuesto queda limitado a la validacion previa de los cuatro filtros de
union, una prueba Python parametrizada y la evidencia de Validacion 78. No se
cambian semantica de interseccion, cierre de dependencias, producto ni bateria
historica y no se configura, compila o ejecuta producto.

### Enmienda: evidencia parcial de timeout preservada

`HostExecutor` debe normalizar `stdout` y `stderr` parciales de
`TimeoutExpired`, que Python puede entregar como bytes aun con `text=True`,
antes de escribir los logs. El timeout debe conservar su diagnostico causal y
no degradarse a un `TypeError` del adaptador.

El presupuesto queda limitado a la normalizacion UTF-8 con reemplazo de la
salida parcial, una prueba Python sintetica y la evidencia de Validacion 79. No
se cambia el tiempo limite, estado terminal, producto ni bateria historica y no
se configura, compila o ejecuta producto.

### Enmienda: evidencia parcial de timeout de identidad

`CompilerIdentityExecutor._invoke` debe publicar los logs parciales asociados
al `stem` antes de volver a propagar `TimeoutExpired`. La salida en bytes debe
normalizarse igual que en el adaptador host para que el resultado conserve el
timeout causal y la evidencia producida por el compilador.

El presupuesto queda limitado a `_invoke`, la normalizacion local de salida,
una prueba Python sintetica y la evidencia de Validacion 80. No se cambian
comandos, tiempos limite, oraculos, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: estados de resultado cotejados fila por fila

`assert_component_results` debe validar el estado textual de cada resultado en
el mismo orden que la seleccion, exigir que coincida con `expected_outcome` y
derivar los conteos observados de esas filas. El objeto `counts` no puede
acreditar estados distintos de los publicados en `results`.

El presupuesto queda limitado al validador central, sus pruebas Python y la
evidencia de Validacion 81. No se cambian estados permitidos del runner,
oraculos, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: estados terminales del stream por caso

`validate_events` debe exigir que cada `CASE_FINISHED` tenga payload objeto,
estado textual no vacio y correspondencia exacta con `expected_outcome` del
caso identificado. El stream progresivo no puede acreditar cobertura por IDs
mientras publica estados contradictorios.

El presupuesto queda limitado al gate de infraestructura, su prueba Python y
la evidencia de Validacion 82. No se cambia la emision de eventos, el runner,
producto ni bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: JSON self-host exacto en Builder 3

La ruta monolitica `_run_selfhost` debe comparar el JSON observado por
estructura, tipo y valor mediante una lista de trabajo iterativa. La igualdad
permisiva de Python no puede hacer que `true` acredite `1` ni que un entero
acredite un flotante.

El presupuesto queda limitado a un comparador local no recursivo, su uso en
`_run_selfhost`, pruebas Python sinteticas y la evidencia de Validacion 83. No
se cambian oraculos, fixtures, producto ni bateria historica y no se configura,
compila o ejecuta producto.

### Enmienda: duracion finita del reporte enfocado

`parse_identity_envelope` debe rechazar duraciones `NaN` e infinitas. Un valor
numericamente no finito no representa tiempo transcurrido valido y no puede
acreditar un resultado terminal aun cuando la comparacion con cero no lo
detecte.

El presupuesto queda limitado al predicado de `duration_seconds`, el import
estandar indispensable, una prueba Python sintetica y la evidencia de
Validacion 84. No se cambia el esquema, estados, conteos, producto ni bateria
historica y no se configura, compila o ejecuta producto.

### Enmienda: stdout host de una sola linea

`parse_host_envelope` debe aplicar el contrato real del productor y del
adaptador canonico: una ejecucion enfocada emite exactamente una linea no
vacia y esa linea es el objeto JSON. No puede descartar ruido textual para
convertir una salida de varias lineas en un resultado valido.

El presupuesto queda limitado a la seleccion de lineas de stdout, una prueba
Python sintetica y la evidencia de Validacion 85. No se cambia el productor,
esquema, estados, producto ni bateria historica y no se configura, compila o
ejecuta producto.

### Enmienda: coherencia de evidencia host

`parse_host_envelope` debe exigir el mismo contrato de evidencia que el
adaptador canonico: `PASS` no contiene fallas, `FAIL` contiene al menos una
falla y cada mensaje es una cadena no vacia. El codigo de salida coherente no
basta para acreditar una combinacion contradictoria.

El presupuesto queda limitado a la validacion local de `failures`, pruebas
Python sinteticas y la evidencia de Validacion 86. No se cambian estados,
codigos de salida, producto ni bateria historica y no se configura, compila o
ejecuta producto.

### Enmienda: recibos exactos de herramientas del reporte enfocado

`parse_identity_envelope` debe cotejar la lista `run.tools` producida por el
controlador contra las herramientas que Builder 3 selecciono para esa corrida:
nombre, ruta resuelta y SHA-256. Un resultado no puede acreditarse si omite,
sustituye, duplica o altera el recibo de NSC, el comparador o el encabezado de
identidad configurados.

El presupuesto queda limitado al parser enfocado, la construccion local del
recibo esperado en `_run_identity`, pruebas Python sinteticas y la evidencia de
Validacion 87. No se cambian herramientas, comandos, producto ni bateria
historica y no se configura, compila o ejecuta producto.

### Enmienda: detalle obligatorio en gates Python fallidos

`parse_python_gate_result` debe rechazar `(False, "")`. Todos los productores
reales publican una causa cuando el gate no se satisface; una falla sin detalle
no puede convertirse en `FAIL` o `KNOWN_FAILURE` sin evidencia causal.

El presupuesto queda limitado al parser del resultado Python, una prueba
sintetica y la evidencia de Validacion 88. No se cambian gates, estados,
producto ni bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: evidencia no vacia del reporte enfocado

`parse_identity_envelope` debe rechazar cadenas vacias tanto en `failures`
como en `blocked_by`. El productor canonico no puede construir un `CaseResult`
con evidencia vacia; el consumidor tampoco debe aceptar ese estado si el
reporte fue alterado o truncado.

El presupuesto queda limitado a los dos predicados de evidencia, pruebas
Python sinteticas y la evidencia de Validacion 89. No se cambian estados,
productores, producto ni bateria historica y no se configura, compila o
ejecuta producto.

### Enmienda: enteros exactos del reporte enfocado

`parse_identity_envelope` debe exigir enteros exactos para `schema` y
`run.selected`. La igualdad permisiva de Python no puede permitir que `true`
acredite la version de esquema `1` ni la cardinalidad seleccionada `1`.

El presupuesto queda limitado a esos dos predicados, pruebas Python sinteticas
y la evidencia de Validacion 90. No se cambian valores validos, productores,
producto ni bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: identidad de argumento self-host

`parse_selfhost_inputs` debe cotejar `case_argument` contra el `case_id` de la
fila seleccionada. Una variante no puede acreditarse ejecutando por error otro
caso del mismo fixture, aunque ese caso termine con el codigo esperado.

El presupuesto queda limitado al parser de entradas self-host, su llamada en
`_run_selfhost`, pruebas Python sinteticas y la evidencia de Validacion 91. No
se cambian manifiestos, fixtures, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: claves JSON unicas en el manifiesto canonico

`load_manifest` debe rechazar claves duplicadas en cualquier objeto de una
linea JSONL, incluidas estructuras anidadas de `inputs` y `oracle`. La politica
last-wins de `json.loads` no puede decidir silenciosamente cual definicion
forma parte del contrato ejecutable.

El presupuesto queda limitado al cargador canonico de manifiestos, una prueba
Python sintetica y la evidencia de Validacion 93. No se cambian filas vivas,
oraculos, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: claves JSON unicas en catalogos de disposicion

El lector JSONL local de Builder 3 debe rechazar claves duplicadas en los
catalogos que asignan disposiciones, bloqueos y contradicciones. Una segunda
definicion no puede sobrescribir silenciosamente la identidad o disposicion de
una fila antes de integrarla al catalogo canonico.

El presupuesto queda limitado a `read_jsonl`, un constructor local de objetos
JSON unicos, una prueba Python sintetica y la evidencia de Validacion 94. No se
cambian catalogos vivos, disposiciones, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: claves JSON unicas en el sobre host

`parse_host_envelope` debe rechazar claves duplicadas en el resultado JSON del
host consolidado. Una segunda definicion no puede sustituir silenciosamente
identidad, estado, duracion o evidencia antes de validar el sobre terminal.

El presupuesto queda limitado al parseo JSON del host, una prueba Python
sintetica y la evidencia de Validacion 95. No se cambia el productor, esquema,
producto ni bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: claves JSON unicas en el reporte enfocado

`parse_identity_envelope` debe rechazar claves duplicadas en todo el reporte
JSON producido por `run_suite.py`, incluidos metadata, recibos, conteos y
resultados. La politica last-wins no puede alterar evidencia antes del cotejo.

El presupuesto queda limitado al parseo JSON del reporte enfocado, una prueba
Python sintetica y la evidencia de Validacion 96. No se cambia el productor,
esquema, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: claves JSON unicas en la salida self-host

La salida JSON de un fixture self-host debe rechazarse si cualquier objeto
contiene claves duplicadas. La comparacion exacta con el oraculo no puede
recibir primero un objeto ya normalizado por la politica last-wins.

El presupuesto queda limitado a un parser local de salida self-host, su uso en
`_run_selfhost`, una prueba Python sintetica y la evidencia de Validacion 97.
No se cambian oraculos, fixtures, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: claves JSON unicas al reabrir historial

`history_record` debe rechazar claves duplicadas tanto en `metadata.json` como
en `results.json`. La vista historica no puede mostrar como terminal una
corrida cuya identidad, estado o conteos fueron resueltos mediante last-wins.

El presupuesto queda limitado a esos dos parseos, una prueba Python sintetica
y la evidencia de Validacion 98. No se cambian productores, artefactos
existentes, producto ni bateria historica y no se configura, compila o ejecuta
producto.

### Enmienda: claves JSON unicas en configuracion persistida

`BuilderConfig.load` debe rechazar claves duplicadas en `config.json`. Una
segunda definicion no puede sustituir silenciosamente rutas de NSC, NSLink,
LLVM, directorios de build o paralelismo antes del descubrimiento.

El presupuesto queda limitado al parseo de la configuracion, una prueba Python
sintetica y la evidencia de Validacion 99. No se cambian defaults, descubrimiento,
producto ni bateria historica y no se configura, compila o ejecuta producto.

### Enmienda: claves JSON unicas en el adaptador host canonico

`HostExecutor` debe rechazar claves duplicadas en la linea JSON emitida por
`nsc_component_tests`. El controlador canonico no puede aceptar una identidad,
estado o evidencia resuelta mediante la politica last-wins.

El presupuesto queda limitado al parseo del adaptador host, un constructor
local no recursivo, una prueba Python sintetica y la evidencia de Validacion
100. No se cambia el productor, producto ni bateria historica y no se
configura, compila o ejecuta producto.

### Enmienda: duracion valida en el adaptador host canonico

`HostExecutor` debe validar el `duration_us` que forma parte de cada resultado
emitido por `nsc_component_tests`. Ausencia, coercion booleana, tipo no entero
o valor negativo no pueden atravesar la frontera como resultado terminal.

El presupuesto queda limitado al esquema del adaptador host, sus fixtures
Python sinteticos y la evidencia de Validacion 101. La duracion efectiva de
`CaseResult` sigue midiendose en el proceso controlador. No se cambia ni
ejecuta productor, producto o bateria historica.

### Enmienda: claves JSON unicas en inventarios self-host

El lector `selfhost_inventory.read_jsonl`, compartido por los controladores
self-host, debe rechazar claves duplicadas en cualquier nivel de sus documentos.
Ningun `oracle`, `input`, estado o identidad puede resolverse mediante
last-wins fuera del cargador canonico.

El presupuesto queda limitado al lector compartido, un constructor iterativo,
una prueba Python sintetica y la evidencia de Validacion 102. No se cambian los
catalogos, manifiestos, producto ni bateria historica y no se ejecuta producto.

### Enmienda: claves JSON unicas en gates BuildSystem y Packaging

Los gates BuildSystem y Packaging deben consumir sus catalogos JSONL mediante
un unico lector estricto. Las claves repetidas en cualquier nivel no pueden
modificar identidad, disposicion, conteos u oraculos mediante last-wins.

El presupuesto queda limitado a consolidar los dos lectores equivalentes en
`component_gate_support`, una prueba Python sintetica y la evidencia de
Validacion 103. No se cambian catalogos, casos, producto ni bateria historica
y no se ejecuta producto.

### Enmienda: evidencia terminal JSON unica en gates de componentes

Los gates host e Infrastructure deben reabrir `results.json` y `events.jsonl`
sin politica last-wins. Una clave repetida en identidad, estado, conteo o
evento no puede acreditar la clausura de un componente.

El presupuesto queda limitado a un lector de objeto JSON estricto, reutilizar
el lector JSONL estricto, sustituir lecturas equivalentes en los gates y la
evidencia de Validacion 104. No se cambian seleccion, ejecucion, producto ni
bateria historica y no se ejecuta producto.

### Enmienda: claves JSON unicas en catalogo bootstrap infrastructure

El catalogo de `SelfHost.Bootstrap.Infrastructure` debe rechazar claves
duplicadas antes de validar linaje, cobertura y productores. Una segunda
identidad, ruta o lista de casos no puede sustituir a la primera mediante
last-wins.

El presupuesto queda limitado al parser de una fila del catalogo, un
constructor iterativo, una prueba Python sintetica y la evidencia de Validacion
105. No se cambia el catalogo vivo, inventario, producto ni bateria historica
y no se ejecuta producto.

### Enmienda: claves JSON unicas en salida diagnostics

El oraculo JSON de `SelfHost.Diagnostics` debe rechazar objetos con claves
duplicadas antes de la comparacion exacta. La politica last-wins no puede
convertir una salida contradictoria del fixture en coincidencia valida.

El presupuesto queda limitado al parser JSON exacto compartido, el evaluador
Diagnostics, una prueba Python sintetica y la evidencia de Validacion 106. No
se cambia ni ejecuta fixture, producto o bateria historica.

### Enmienda: esquema CSV estricto en inventario canonico

`InventoryIndex` debe rechazar encabezados vacios o duplicados, campos
requeridos ausentes, filas truncadas y celdas extra. Las columnas adicionales
con nombre permanecen permitidas para conservar la extensibilidad documental.

El presupuesto queda limitado al lector CSV del inventario canonico, una
prueba Python sintetica y la evidencia de Validacion 107. No se cambian los
CSV vivos, manifiesto, seleccion, producto ni bateria historica y no se ejecuta
producto.

### Enmienda: esquema CSV estricto en gates de integracion

Los gates self-host, BuildSystem y Packaging deben consumir los tres
inventarios CSV mediante un lector comun que rechace estructura ambigua. Cada
consumo declara sus campos requeridos; las columnas adicionales con nombre
siguen permitidas.

El presupuesto queda limitado a consolidar los tres lectores, actualizar sus
diez consumos, una prueba Python sintetica y la evidencia de Validacion 108.
No se cambian los CSV vivos, reglas de gates, producto ni bateria historica y
no se ejecuta producto.

### Enmienda: lector CSV generico estricto en Builder 3

El lector `nscbuilder3.read_csv`, usado por las auditorias de alcance, debe
compartir la validacion estructural de `read_catalog_csv`. No puede aceptar
encabezados duplicados, filas truncadas o celdas extra mientras calcula los
componentes incluidos y excluidos.

El presupuesto queda limitado a delegar el alias generico, una prueba Python
sintetica y la evidencia de Validacion 109. No se cambian campos requeridos,
catalogos, seleccion, producto ni bateria historica y no se ejecuta producto.

### Enmienda: identidades exactas en gates BuildSystem y Packaging

Los indices de BuildSystem y Packaging deben exigir `case_id` y `contract_id`
como cadenas no vacias. Numeros, booleanos u otros valores no pueden
convertirse mediante `str(...)` antes de validar duplicados y linaje.

El presupuesto queda limitado a un indice compartido de cadenas, sustituir los
dos helpers equivalentes, una prueba Python sintetica y la evidencia de
Validacion 110. No se cambian catalogos, denominadores, producto ni bateria
historica y no se ejecuta producto.

### Enmienda: checkpoint estatico posterior al endurecimiento

Tras las Validaciones 100 a 110 se debe revalidar la suite Python completa, el
catalogo vivo y la construccion/cierre de la GUI. El checkpoint no autoriza
configurar, compilar o ejecutar producto y no sustituye la matriz funcional.

El presupuesto queda limitado a documentar la evidencia de Validacion 111 y
actualizar tracking/cursor. No se cambia codigo, producto ni bateria historica.

### Enmienda: arreglo exacto de tags en manifiestos crudos

Los gates BuildSystem, Packaging y self-host que consumen `cases.jsonl` como
objetos crudos deben conservar el contrato canonico de `tags`: arreglo, cadenas
no vacias y valores unicos. Un texto como `"known-product-defect"` no puede
acreditar una clasificacion mediante pertenencia de substring.

El presupuesto queda limitado a un validador compartido de arreglos de cadenas,
las tres fronteras que validan esos manifiestos crudos, una reproduccion Python
sobre BuildSystem, Packaging, SelfHost.Diagnostics y SelfHost.Lexer, y la
evidencia de Validacion 112. No se cambian manifiestos, clasificaciones,
oraculos, producto ni bateria historica y no se ejecuta producto.

### Enmienda: autoridad canonica en cargas raw de manifiesto

Los siete gates que necesitan conservar objetos raw de `cases.jsonl` deben
validar primero el documento completo mediante `nsc_suite.load_manifest` y su
inventario canonico. Sus validadores locales no pueden aceptar campos
desconocidos, escalares en lugar de objetos o arreglos, enums invalidos ni
otras formas que el parser canonico rechaza.

El presupuesto queda limitado a una frontera compartida que devuelve las filas
raw despues de la validacion canonica, sustituir las siete lecturas directas,
una matriz sintetica de esquema, pruebas de integracion y la evidencia de
Validacion 113. No se cambian manifiestos, parser canonico, casos, oraculos,
producto ni bateria historica y no se ejecuta producto.

### Enmienda: esquema exacto del catalogo bootstrap infrastructure

El cargador de `SelfHostBootstrapInfrastructureCatalog.jsonl` no puede
convertir con `str(...)` identidades, rutas, metadatos ni elementos de
`case_ids`. Debe exigir el conjunto exacto de campos observado en las 73 filas
vivas, cadenas no vacias y un arreglo no vacio de cadenas unicas antes de
validar linaje o existencia de productores.

El presupuesto queda limitado al cargador compartido, pruebas sinteticas de
esquema, validacion del catalogo vivo y la evidencia de Validacion 114. No se
agregan enums ni conteos rigidos, no se cambian catalogo, inventarios, casos,
producto ni bateria historica y no se ejecuta producto.

### Enmienda: cierre exhaustivo de consumidores raw omitidos

La auditoria posterior a Validacion 113 encontro tres consumidores adicionales
del manifiesto central: `Host.Source`, `Host.nsc` y la validacion generica de
componentes pendientes. Deben usar `component_manifest_rows` igual que los
siete consumidores ya migrados; la evidencia y las metricas deben rectificar
el denominador de siete a diez fronteras.

El presupuesto queda limitado a esas tres sustituciones, ampliar el guard de
cableado, validar todos los componentes afectados y documentar la rectificacion
en Validacion 115. No se cambian manifiesto, inventarios, casos, oraculos,
producto ni bateria historica y no se ejecuta producto.

### Enmienda: identidades exactas en inventarios self-host

`validate_manifest_cases` y `classification_sets` no pueden convertir con
`str(...)` identidades, contrato, capa, perfil ni disposicion. Deben rechazar
tipos JSON distintos de cadena y cadenas vacias en la frontera comun, antes de
comparar linaje o particiones.

El presupuesto queda limitado a un validador textual compartido, sustituir las
coerciones de esas dos funciones, pruebas sinteticas y la evidencia de
Validacion 116. No se cambian manifiestos, clasificaciones, inventarios, casos,
producto ni bateria historica y no se ejecuta producto.

### Enmienda: campos exactos en catalogos de sistema

BuildSystem y Packaging no pueden convertir con `str(...)` rangos, arreglos de
casos, disposiciones, contratos ni claves de contradiccion provenientes de
JSON. Los campos consumidos deben validarse como cadenas no vacias o arreglos
de cadenas antes de derivar particiones y conteos.

El presupuesto queda limitado a un validador textual comun, sustituir las
coerciones JSON de ambos gates, pruebas sinteticas contra sus validadores y la
evidencia de Validacion 117. No se cambian catalogos, manifiestos, inventarios,
casos, producto ni bateria historica y no se ejecuta producto.

### Enmienda: identidades exactas en catalogos self-host restantes

Los gates de `SelfHost.Project`, `SelfHost.Bootstrap.Infrastructure`,
`SelfHost.Contracts` y `SelfHost.Documentation` no pueden convertir con
`str(...)` las identidades y listas que consumen desde JSON. Los campos
`id`, `contract_id`, `source_path`, `case_ids` y `affected_case_ids` deben
conservar su tipo original y rechazarse en su frontera cuando no sean cadenas
no vacias o arreglos de cadenas unicas.

La entrada exacta es una fila JSON sintacticamente valida con alguno de esos
campos reemplazado por un numero, booleano, escalar, elemento no textual,
cadena vacia o duplicado. La operacion defectuosa es la conversion previa al
cotejo de identidad, linaje, cobertura o contradiccion. La invariante violada
es que la evidencia JSON debe tener tipo exacto antes de participar en el
gate. Los consumidores afectados son exclusivamente los cuatro gates citados.

El presupuesto de Validacion 118 queda limitado a reutilizar los validadores
textuales ya existentes, sustituir esas siete conversiones demostradas,
agregar pruebas Python sinteticas sobre las fronteras reales y documentar la
evidencia. Permanecen protegidos los catalogos vivos, manifiestos, inventarios,
casos, reglas de disposicion, producto, bateria historica y los seis archivos
del usuario registrados en `CURSOR.json`. No se configura, compila o ejecuta
producto; no se ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: identidad exacta en modulos self-host materializados

`validate_unique_materialized_ids` no puede convertir con `str(...)` la
primera columna de una fila materializada antes de detectar vacios o
duplicados. `SelfHost.Bootstrap`, `SelfHost.CodeGen` y
`SelfHost.Documentation` producen identidades textuales; un numero, booleano
u objeto no debe adquirir una identidad valida por representacion.

La entrada exacta es una fila materializada cuyo primer elemento no es una
cadena no vacia. La operacion defectuosa es su conversion previa al indice. La
invariante violada es que el identificador del caso conserva tipo textual
exacto. El simbolo responsable y unico autorizado es
`validate_unique_materialized_ids`; sus tres consumidores deben conservar el
mismo denominador y linaje.

El presupuesto de Validacion 119 queda limitado a eliminar esa conversion,
agregar pruebas sinteticas del helper y ejecutar los tres gates estaticos
contra sus productores vivos. No se cambian parsers, modulos C++, catalogos,
manifiestos, inventarios, casos, producto ni bateria historica. Los seis
archivos del usuario registrados en `CURSOR.json` permanecen protegidos. No se
configura, compila o ejecuta producto; no se ejecuta baseline y no se toca la
unidad `C:`.

### Enmienda: checkpoint estatico y conciliacion de reportes externos

Despues de las Validaciones 118 a 120 se debe ejecutar el diagnostico no
mutante del catalogo y conciliar los cinco reportes externos mas recientes con
el codigo y los commits actuales. La conciliacion no puede presentar una
correccion estatica como corrida funcional posterior.

El presupuesto de Validacion 121 queda limitado a documentar hashes,
diagnosticos, commits causales, productores presentes, resultado de
`--audit-catalog`, limite de evidencia y siguiente validacion externa. No se
cambia codigo, CMake, pruebas, catalogos, manifiestos, producto ni bateria
historica. Los seis archivos del usuario registrados en `CURSOR.json`
permanecen protegidos. No se configura, compila o ejecuta producto; no se
ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: productores self-host contenidos en el repositorio

Los catalogos `SelfHost.Contracts` y `SelfHost.Bootstrap.Infrastructure` no
pueden acreditar un productor mediante una ruta absoluta o una ruta relativa
que, despues de resolverse, quede fuera del repositorio. Comprobar solamente
`is_file()` permite que un archivo externo existente sustituya al productor
versionado esperado.

La entrada exacta es un `source_path` textual valido que apunta fuera de
`repository_root`. La operacion defectuosa es unir o resolver la ruta y
aceptarla por existencia. La invariante violada es que todo productor
catalogado pertenece al checkout auditado. Los consumidores exactos son el
cargador Bootstrap Infrastructure y `catalog_ranges` de Contracts.

El presupuesto de Validacion 120 queda limitado a un resolvedor comun de
archivo contenido, sustituir esas dos comprobaciones, agregar mutaciones de
ruta absoluta y traversal, validar los catalogos vivos y documentar la
evidencia. No se cambian rutas vivas, catalogos, modulos, manifiestos,
inventarios, casos, producto ni bateria historica. Los seis archivos del
usuario registrados en `CURSOR.json` permanecen protegidos. No se configura,
compila o ejecuta producto; no se ejecuta baseline y no se toca la unidad
`C:`.

### Enmienda: autoridad explicita del runtime en la suite de Builder 3

La configuracion de `tests/compiler-suite` emitida por Builder 3 no puede
heredar `CMAKE_INSTALL_PREFIX` del entorno ni materializar la biblioteca
runtime bajo el prefijo predeterminado de Windows. Debe transmitir de forma
explicita `NSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY` derivada del
`install_prefix` ya seleccionado y validado en la unidad `A:`.

La entrada exacta es una configuracion de Builder 3 con
`install_prefix=A:/native-sharp-3.0-baseline`. La operacion defectuosa es
omitir la variable especifica de la suite, lo que deja que CMake derive
`C:/Program Files (x86)/NativeSharpCompilerSuiteInfrastructure/...`. La
invariante violada es que la infraestructura aislada no puede descubrir ni
consumir artefactos de `C:`. El consumidor exacto es el comando producido por
`configure_commands`.

El presupuesto de Validacion 122 queda limitado a agregar ese argumento a
`configure_commands`, ampliar su prueba contractual, documentar la evidencia
y actualizar tracking y cursor. No se cambia CMake, el compilador, NSLink,
deploy, manifiestos, casos, oraculos, producto ni bateria historica. Los seis
archivos del usuario registrados en `CURSOR.json` permanecen protegidos. No se
configura, compila o ejecuta producto; no se ejecuta baseline y no se toca la
unidad `C:`.

### Enmienda: consumo efectivo de la autoridad runtime explicita

La auditoria posterior a Validacion 122 demostro que el comando explicito no
basta: `tests/compiler-suite/CMakeLists.txt` asigna
`NSC_COMPILER_SUITE_DEFAULT_WINDOWS_RUNTIME_LIBRARY` como variable normal e
incondicional y, por tanto, sombrea la entrada de cache recibida mediante
`-D`. El consumidor debe declarar un valor predeterminado de cache sin
`FORCE`, de modo que preserve la autoridad del argumento explicito y solo use
`CMAKE_INSTALL_PREFIX` cuando el llamador no proporcione una ruta.

La entrada exacta es la opcion `-D` emitida por Builder 3 en Validacion 122.
La operacion defectuosa es sobrescribirla durante la evaluacion inicial del
`CMakeLists.txt`. La invariante violada es que el valor seleccionado por el
controlador llega sin sustituciones hasta la definicion de compilacion de
`nsc_component_tests`.

El presupuesto de Validacion 123 queda limitado a convertir esa unica
asignacion en una entrada `CACHE FILEPATH`, agregar una comprobacion estatica
del contrato CMake, rectificar tracking y cursor y conservar evidencia. No se
cambian otras variables CMake, el compilador, NSLink, deploy, manifiestos,
casos, oraculos, producto ni bateria historica. Los seis archivos del usuario
registrados en `CURSOR.json` permanecen protegidos. No se configura, compila o
ejecuta producto; no se ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: LLVM C derivada sin cache obsoleta

`NSC_COMPILER_SUITE_LLVM_C_IMPORT_LIBRARY` y
`NSC_COMPILER_SUITE_LLVM_C_RUNTIME` se derivan de `LLVM_DIR`, pero las llamadas
`find_library` y `find_file` las almacenan en el cache CMake por omision. Al
reutilizar el arbol con otro `LLVM_DIR`, ambas pueden conservar rutas de la
toolchain anterior y romper la identidad del enlace o la copia del runtime.

La entrada exacta es una reconfiguracion del mismo arbol con una autoridad
`LLVM_DIR` distinta. La operacion defectuosa es omitir una nueva busqueda
porque la variable derivada ya existe en cache. La invariante violada es que
todos los artefactos LLVM de una configuracion proceden de la autoridad LLVM
seleccionada en esa misma configuracion.

El presupuesto de Validacion 124 queda limitado a marcar esas dos busquedas
como `NO_CACHE`, agregar una comprobacion estatica de ambas fronteras y
actualizar evidencia, tracking y cursor. No se cambian rutas, nombres de
biblioteca, toolchain, comandos, producto, casos, manifiestos, oraculos ni
bateria historica. Los seis archivos del usuario registrados en `CURSOR.json`
permanecen protegidos. No se configura, compila o ejecuta producto; no se
ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: interprete Python explicito y no hardcodeado

Los controladores CTest usan `Python3_EXECUTABLE`, pero Builder 3 no transmite
el interprete con el que se esta ejecutando. `find_package(Python3)` puede
conservar en el arbol reutilizado una seleccion anterior y ejecutar los 118
controladores con un Python distinto al de la sesion vigente.

La entrada exacta es una configuracion posterior iniciada con otro interprete
Python valido. La operacion defectuosa es delegar otra vez la seleccion a una
cache CMake existente. La invariante violada es que toda la infraestructura
Python de la corrida usa el interprete real y verificable que ejecuta Builder
3, sin asumir una instalacion fija.

El presupuesto de Validacion 125 queda limitado a incorporar `sys.executable`
al recibo de herramientas descubierto, transmitirlo como
`Python3_EXECUTABLE`, ampliar las pruebas puras de descubrimiento y comandos y actualizar
evidencia, tracking y cursor. No se hardcodea una ruta Python, no se cambian
controladores, CMake, producto, casos, manifiestos, oraculos ni bateria
historica. Los seis archivos del usuario registrados en `CURSOR.json`
permanecen protegidos. No se configura, compila o ejecuta producto; no se
ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: runtime perteneciente al compilador seleccionado

La auditoria posterior demostro que derivar el runtime de `install_prefix`,
como hizo Validacion 122, mezcla el NSC recien construido con una biblioteca
instalada anterior. El arbol vivo contiene
`build-clang/generated/NativeSharp.Runtime.Win32.lib` del 2026-10-10 y el
prefijo instalado contiene otro artefacto del 2026-10-07.

La autoridad runtime debe ser el mismo directorio `generated` que proporciona
el `BuildId.h` compilado por `Version.cpp`: cuando existe un encabezado
explicito se deriva de sus ancestros y, en caso contrario, se deriva del arbol
propietario del `nsc.exe` seleccionado. No se permite fallback al prefijo
instalado.

La entrada exacta es un NSC seleccionado bajo `<build>/bin/nsc.exe`. La
operacion defectuosa es construir la ruta desde un `install_prefix` ajeno a la
identidad de ese binario. La invariante violada es que binario, BuildId y
runtime pertenecen al mismo productor de compilacion.

El presupuesto de Validacion 126 queda limitado a una funcion pura de
derivacion, usarla en `configure_commands`, ampliar las pruebas de comandos,
rectificar la evidencia previa y actualizar tracking y cursor. No se cambian
artefactos, CMake, producto, casos, manifiestos, oraculos ni bateria historica.
Los seis archivos del usuario registrados en `CURSOR.json` permanecen
protegidos. No se configura, compila o ejecuta producto; no se ejecuta baseline
y no se toca la unidad `C:`.

### Enmienda: diagnostico causal en resultados de infraestructura

La corrida `20261009-234910-fcda0e5c` demostro una perdida de evidencia en el
controlador: `full.log` contiene errores concretos de `lld-link`, pero
`results.json` y `summary.md` reducen el fallo de construccion a
`exit code 4294967295`. `ProcessController.run` ya devuelve la salida completa;
`CommandWorker` la descarta al desempaquetarla como `code, _`.

La entrada exacta es un comando de infraestructura no cancelado que termina
con codigo distinto de cero y salida que contiene un diagnostico reconocible.
La operacion defectuosa es proyectar el resultado terminal sin esa salida. La
invariante violada es que todo fallo conserva en `CaseOutcome.detail` una causa
accionable y mantiene `full.log` como evidencia completa.

El presupuesto de Validacion 127 queda limitado a una funcion pura e iterativa
que selecciona el primer diagnostico causal de CMake, compilador, linker o
Ninja; transmitir la salida ya capturada desde `CommandWorker`; ampliar las
pruebas de resultados de infraestructura; y actualizar evidencia, tracking y
cursor. El detalle permanece acotado y no sustituye ni duplica el log completo.
No se cambian procesos, cancelacion, comandos, CMake, producto, casos,
manifiestos, oraculos ni bateria historica. Los seis archivos del usuario
registrados en `CURSOR.json` permanecen protegidos. No se configura, compila o
ejecuta producto; no se ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: conciliacion exhaustiva del historial local Builder 3

El historial local contiene 17 recibos estructurados producidos entre
`20261009-225634-de4956a7` y `20261009-234910-fcda0e5c`: siete terminan en
`PASSED` y diez en `FAILED`. Los diez fallos deben inventariarse completos y
agruparse por evidencia, no contarse como diez defectos vigentes ni descartarse
por pertenecer a revisiones anteriores.

El presupuesto de Validacion 128 queda limitado a leer `metadata.json`,
`results.json` y `full.log` de los 17 recibos; calcular hashes de los diez logs
fallidos; mapear cada causa a su cierre ya existente; registrar duplicados y
limites de validacion; y actualizar tracking y cursor. No se cambia codigo,
controladores, CMake, procesos, producto, casos, manifiestos, oraculos ni
bateria historica. Los seis archivos del usuario registrados en `CURSOR.json`
permanecen protegidos. No se configura, compila o ejecuta producto; no se
ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: diagnostico causal visible al cerrar infraestructura

Validacion 127 conserva ya el diagnostico causal en `CaseOutcome.detail`,
`results.json` y `summary.md`, pero el cierre grafico de una configuracion o
construccion fallida reduce esa informacion a `Operacion terminada con
errores`. La pestaña de errores condensados solamente se actualiza al terminar
la ejecucion de casos, no al terminar comandos de infraestructura.

La entrada exacta es una lista terminal de resultados de infraestructura con
al menos un estado de `TERMINAL_FAILURES`. La operacion defectuosa es cerrar el
worker sin renderizar esos mismos resultados ni dirigir la interfaz a su
reporte. La invariante violada es que toda causa persistida por Builder 3 sea
visible en la GUI sin buscar manualmente en `full.log`.

El presupuesto de Validacion 129 queda limitado a extraer el formateo
condensado a una funcion pura compartida, usarla en los cierres de
infraestructura y suite, abrir la pestaña de errores cuando la infraestructura
termine con fallos y ampliar pruebas estaticas del reporte. No se cambian
procesos, comandos, CMake, producto, casos, manifiestos, oraculos ni bateria
historica. Los seis archivos del usuario registrados en `CURSOR.json`
permanecen protegidos. No se configura, compila o ejecuta producto; no se
ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: clausura terminal completa del plan de infraestructura

Cuando un comando de configuracion o construccion falla, `CommandWorker`
publica el resultado de ese comando y rompe el ciclo. Los comandos posteriores
declarados en `metadata.commands` desaparecen de `results.json`: no figuran
como ejecutados, cancelados ni bloqueados. La cancelacion ya cierra
explicitamente su cola, pero el fallo causal no hace lo mismo.

La entrada exacta es un plan de dos o mas comandos donde uno anterior termina
con codigo distinto de cero. La operacion defectuosa es abandonar el ciclo sin
materializar estados terminales para el sufijo no iniciado. La invariante
violada es que cada comando declarado tenga exactamente una fila terminal y
que la cardinalidad publicada coincida con el plan original.

El presupuesto de Validacion 130 queda limitado a proyectar el sufijo no
iniciado como `BLOCKED` por el comando fallido, declarar `variant_count` en la
metadata de infraestructura, ampliar las pruebas puras de clausura y actualizar
evidencia, tracking y cursor. No se ejecutan comandos adicionales ni se cambia
su orden; tampoco se cambian procesos, CMake, producto, casos, manifiestos,
oraculos ni bateria historica. Los seis archivos del usuario registrados en
`CURSOR.json` permanecen protegidos. No se configura, compila o ejecuta
producto; no se ejecuta baseline y no se toca la unidad `C:`.

### Enmienda: propiedad estable del metadato de variantes de Passes

La primera ejecucion completa de la suite nueva en el arbol Debug configurado
en `A:` llego a construir `536/536` pasos, pero el productor comun
`nsc_component_tests --list` termina antes de enumerar casos con
`test case metadata is incomplete`. La auditoria estatica de los catorce
helpers que expanden contratos en variantes demostro una sola divergencia:
`addPassContract` mueve `layer` dentro del bucle de ocho variantes, mientras
los otros trece helpers lo conservan. La primera variante consume el valor y
la segunda presenta una cadena vacia a `TestRegistry::add`.

La entrada exacta es cualquier contrato de `Host.Passes` con mas de una
variante. La operacion defectuosa es `std::move(layer)` dentro del bucle. La
invariante violada es que cada variante materializada conserve el mismo
metadato no vacio de contrato, componente y capa.

El presupuesto de Validacion 131 queda limitado a
`tests/compiler-suite/host/modules/PassTestSupport.h`, sustituir el movimiento
iterativo de `layer` por su copia estable, validar primero el productor
`--list`, ejecutar despues los gates dependientes y actualizar evidencia,
tracking y cursor. No se cambian `TestRegistry`, casos, IDs, contratos,
oraculos, disposiciones, manifiestos, CMake, producto ni bateria historica. Los
seis archivos del usuario registrados en `CURSOR.json` permanecen protegidos.
No se ejecuta baseline, no se despliega y no se toca la unidad `C:`.

### Enmienda: inventario observable de casos pendientes fuera del registro

La clasificacion completa de los `1,062` IDs restantes demostro dos fronteras
distintas: `841` casos Host estan materializados deliberadamente en cuatro
colecciones `InfrastructurePending` fuera de `TestRegistry`, y `221` casos son
producidos por el controlador y proyectos NativeSharp externos al ejecutable
C++. El gate creado en Validacion 26 confundio las tres superficies y exigio
que todas pertenecieran al registro ejecutable ordinario.

Validacion 134 no cambia esa separacion. Debe exponer, mediante una opcion de
solo lectura distinta de `--list`, las identidades ya conservadas en las
colecciones pendientes de AST, Interop, Linker y Runtime. La salida usa el
mismo contrato tabulado `(case_id, contract_id, component)`, ordena por ID y
rechaza metadato incompleto o duplicado. No agrega `TestCase`, no crea funciones
vacias y no convierte un pendiente en PASS, FAIL o SKIP.

El presupuesto queda limitado a declarar una fila de inventario comun,
agregar un agregador de solo lectura, conectar `--list-pending`, incorporar el
nuevo fuente al target, reconstruir `nsc_component_tests` y comprobar la
cardinalidad e identidad completas. El gate de paridad se corregira en una
tanda causal posterior. No se cambian cuerpos, contratos, disposiciones,
oraculos, inventarios canónicos, selectores de ejecucion, producto ni bateria
historica. Los seis archivos protegidos permanecen intactos. No se ejecuta
baseline, no se despliega y no se toca la unidad `C:`.

### Enmienda: ordenamiento unico del registro materializado

La Validacion 131 recupero `nsc_component_tests --list`, que enumera `8,054`
filas con salida cero. La ejecucion del gate de inventario demostro un defecto
independiente: el proceso excede su limite de 60 segundos porque
`TestRegistry::add` ordena el vector completo despues de cada insercion. El
costo observado bloquea el controlador aunque el inventario sea valido.

La entrada exacta es la construccion del registro completo. La operacion
defectuosa es repetir `std::sort` para cada alta. La invariante violada es que
la materializacion tenga costo apto para el timeout del controlador sin perder
el orden lexicografico estable que comprueban `cases()`, `all()` y
`byComponent()`.

El presupuesto de Validacion 132 queda limitado a
`tests/compiler-suite/host/src/TestRegistry.cpp`: localizar con `lower_bound`
la unica posicion de insercion, rechazar ahi mismo un ID duplicado y conservar
las busquedas, las selecciones y el orden observable sin reordenar el vector
completo despues de cada alta.
Se validan el contrato interno de infraestructura, `--list` bajo 60 segundos y
el gate de inventario. No se cambian main, controladores, timeouts, casos,
metadatos, CMake, producto, manifiestos, oraculos ni bateria historica. Los seis
archivos del usuario registrados en `CURSOR.json` permanecen protegidos. No se
ejecuta baseline, no se despliega y no se toca la unidad `C:`.

### Enmienda: identidad canonica de Sema 087 a 099

La conciliacion exhaustiva posterior a Validacion 132 comparo las `8,054`
filas vivas del registro con las `9,113` filas Host/SelfHost del inventario de
diseno. Los trece IDs inesperados son exactamente `TS-SEMA-87` a
`TS-SEMA-99`, mientras el inventario declara `TS-SEMA-087` a
`TS-SEMA-099`. Contrato, componente y cuerpos coinciden; el productor
`registerSemaAnalyzerLifecycleModule` omite el cero inicial solamente para ese
intervalo.

La entrada exacta es registrar los ordinales 87 a 99 del modulo de ciclo de
vida de Sema. La operacion defectuosa es concatenar `std::to_string(ordinal)`
sin conservar el ancho canonico de tres digitos. La invariante violada es que
la identidad materializada sea identica a la identidad estable del inventario.

El presupuesto de Validacion 133 queda limitado a
`tests/compiler-suite/host/modules/SemaAnalyzerLifecycleModule.cpp`: completar
el cero inicial para ordinales menores que 100, reconstruir el productor,
demostrar que desaparecen los trece pares missing/unexpected y actualizar
evidencia, tracking y cursor. No se cambian cuerpos, contratos, disposiciones,
casos pendientes, gate, inventario, CMake, producto ni bateria historica. Los
seis archivos del usuario registrados en `CURSOR.json` permanecen protegidos.
No se ejecuta baseline, no se despliega y no se toca la unidad `C:`.
