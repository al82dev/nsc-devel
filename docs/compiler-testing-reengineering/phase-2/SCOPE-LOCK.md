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
