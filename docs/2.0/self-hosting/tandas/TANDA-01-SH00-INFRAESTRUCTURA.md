# NativeSharp 2.0 self-hosting — tanda 01: infraestructura SH00

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-000`–`NS2-SH-009` |
| Gate | `SH00` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` (`6a9b94850fcb24a911b8f736acaa753422f1cfd5`) |
| Commit de entrada | `6a9b94850fcb24a911b8f736acaa753422f1cfd5` |
| Commit de cierre | este commit (`selfhost(tanda-01): establish bootstrap infrastructure`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-01-sh00-final4-20260919` |

## 2. Objetivo y alcance

### Objetivo

Crear una frontera reproducible y segura para iniciar el self-hosting sin portar todavía el
compilador. La tanda identifica las generaciones, congela herramientas externas, separa salidas,
permite configurar todas las rutas del baseline y evita escrituras fuera de raíces autorizadas.

### Incluido

- Layout canónico `selfhost/`.
- Identidades `nsc0`, `nsc1`, `nsc2` y `nsc3`.
- Inventario de dependencias permitidas.
- `bootstrap.lock` versión 1 y verificador de hashes/versiones.
- Configuración CLI, entorno y JSON para el baseline.
- Identidad reproducible configurable para el `nsc0` congelado.
- Redirección de temporales de procesos hijos.
- Restricción explícita de raíces de escritura.
- Limpieza dirigida protegida por marcador.
- Schemas de reportes y gates.
- Procedimiento de recuperación mediante `nsc0`.
- Instalación aislada de trabajo en `A:\native-sharp`.

### No incluido

- Bootstrap Profile 2.0, que pertenece a la tanda 02.
- Código NativeSharp del compilador.
- Bindings LLVM C y Win32.
- Reescrituras de NSLink o NSAsm.
- Características de NativeSharp 3.0.

## 3. Diseño

### Arquitectura

`nscbuilder.py` produce un `RunPlan` con todas las rutas efectivas. Los valores se resuelven con la
precedencia CLI → entorno → JSON → predeterminado. Antes de crear la corrida, cada destino de
escritura se resuelve a ruta absoluta y debe quedar bajo alguna `allowed_write_roots`.

Los procesos hijos heredan `TEMP`, `TMP` y `TMPDIR` apuntando al área configurada. El verificador de
entorno lee `bootstrap.lock`, expande únicamente variables declaradas, comprueba existencia, tamaño,
SHA-256 y texto de versión y puede emitir un reporte JSON atómico.

La limpieza opera sobre cuatro nombres cerrados (`build`, `work`, `temp`, `runs`) y exige el marcador
`.nativesharp-selfhost-state`. El modo predeterminado sólo informa qué eliminaría.

### Contratos e invariantes

- `nsc0` es semilla/oráculo; `nsc1`, `nsc2` y `nsc3` nunca comparten salidas.
- Una ruta explícita nunca se sustituye silenciosamente por `C:\native-sharp`.
- Con raíces autorizadas, ninguna escritura comienza antes de validar todos los destinos.
- Los hashes del lock son SHA-256 en minúsculas y todo artefacto debe estar `locked`.
- Un lock pendiente falla; no se degrada a advertencia.
- NSLink y NSAsm NativeSharp, si se crean, usarán repositorios nuevos.
- `nsc-next` consumirá LLVM mediante su API C, no mediante un wrapper C++ de NSC.
- El manifiesto S0/S1 incluye `selfhost/` y excluye únicamente sus salidas y corridas generadas.

### Decisiones y alternativas

| Decisión | Alternativas consideradas | Razón |
|---|---|---|
| JSON versionado para rutas y locks | editar scripts, INI | Es comprobable, portable y fácil de registrar en evidencia. |
| Build ID fijo para `nsc0` | hash nuevo en cada build | Permite que el lock y el baseline describan el mismo binario reproducible. |
| Lista positiva de raíces escribibles | detectar sólo `C:` | Protege cualquier ruta ajena y funciona en otras máquinas. |
| Limpieza seca por omisión | borrar inmediatamente | Reduce el riesgo de eliminar fuentes o instalaciones. |
| API C de LLVM | wrapper C++ propio | Elimina la dependencia oculta de la implementación C++ de NSC. |
| Repositorios nuevos para rewrites | mezclar con herramientas actuales | Conserva el oráculo y evita dos implementaciones en un árbol. |

## 4. Inventario de archivos

| Archivo | Acción | Responsabilidad | Razón |
|---|---|---|---|
| `.gitignore` | modificar | Excluir salidas y evidencia generada | No versionar artefactos. |
| `CMakeLists.txt` | modificar | Registrar pruebas Python SH00 | Ejecutarlas en ambos árboles CTest. |
| `nscbuilder.py` | modificar | Configuración y aislamiento de rutas | Eliminar dependencia operativa de un prefijo fijo. |
| `all.bat`, `test_nsc.bat`, `run_windows_checklist.bat` | modificar | Respetar `NS_ROOT` y `NSC_LLVM_ROOT` | Permitir mover instalación y LLVM sin editar scripts. |
| `nslink_solo.bat`, `nslink_pipeline.py` | modificar | Resolver NSLink desde configuración | Eliminar fallback operativo fijo cuando existe `NS_ROOT`. |
| `run_bug_triage.bat` | modificar | Registrar y usar el compilador configurado | Evitar reportes que nombren otro binario. |
| `tools/nsc_performance_baseline.py` | modificar | Derivar toolchain desde entorno/CLI | Medir la instalación seleccionada. |
| `tools/prepare_libraries.cmd`, `lib/.source.txt` | modificar | Marcador de sincronización estable | Evitar que la ruta física cambie el digest de fuentes. |
| `cmake/VerifyNativeSharp*DebugO0.cmake` | modificar | Validar fuga de ruta física del fuente | Permitir que el objeto temporal viva dentro del repo sin debilitar el remapeo. |
| `tests/unit/Item370NamedCrossProcessSynchronizationTests.cpp` | modificar | Sincronizar el arranque entre procesos | Eliminar la suposición temporal de 350 ms. |
| `CMakeLists.txt` | modificar | Serializar la comprobación ABI freestanding | Conservar el retorno estricto 42 sin la intermitencia conocida bajo carga. |
| `docs/2.0/SELF_HOSTING_ROADMAP.md` | modificar | Política documental por tanda | Hacer obligatorios expediente y baseline. |
| `docs/2.0/self-hosting/*` | crear | Reglas, plantilla y expediente | Mantener diseño y evidencia junto al commit. |
| `selfhost/README.md` | crear | Layout y separación de repositorios | Congelar la estructura SH00. |
| `selfhost/bootstrap/*` | crear | Lock, configuración, schemas y herramientas | Implementar SH00. |
| `selfhost/nsc/README.md` | crear | Límite de `nsc-next` | Impedir código C++ oculto. |
| `selfhost/bindings/README.md` | crear | Frontera LLVM C/Win32 | Fijar la arquitectura. |
| `selfhost/tests/README.md` | crear | Organización de pruebas | Separar corpus bootstrap. |
| `tests/selfhost/test_bootstrap_infrastructure.py` | crear | Pruebas SH00 | Cubrir configuración, guardas, lock y limpieza. |

## 5. Tipos y datos

| Tipo | Clase | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|---|
| `RunPlan` | dataclass | Plan completo y serializable de una corrida | Contiene toda ruta de escritura | Propiedad del worker durante la corrida. |
| bootstrap lock v1 | JSON | Identidad de herramientas críticas | IDs únicos, estado locked, SHA-256 | Inmutable durante una corrida. |
| report v1 | JSON/Markdown | Evidencia estructurada y humana | Conserva entradas, etapas y problemas | Pertenece al directorio de corrida. |
| gate v1 | JSON | Estado acumulativo SH00–SH14 | Estado enumerado y problemas explícitos | Un gate enlaza la evidencia exacta. |

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas y precondiciones | Salida y postcondiciones | Errores/diagnósticos | Efectos, ownership y concurrencia |
|---|---|---|---|---|---|
| `load_builder_config(path)` | Cargar JSON v1 | Archivo legible | Diccionario validado | Rechaza schema, versión y tipos | Sólo lectura. |
| `configured_path(config,key,fallback)` | Resolver entorno/config/default | Clave conocida | Ruta sin aplicar CLI | Ninguno | Lee entorno. |
| `configured_write_roots(config)` | Resolver raíces autorizadas | JSON válido | Lista de rutas | Ninguno | Lee entorno. |
| `configured_build_id(config)` | Resolver identidad reproducible | Hexadecimal vacío o de 16 dígitos | Build ID validado | Rechaza formato distinto | Lee entorno. |
| `collect_source_files(root)` | Inventariar árbol protegido | Repo o fixture | Incluye `selfhost`, no sus salidas | Rechaza colisiones al manifestar | Sólo lectura. |
| `path_is_within(path,root)` | Comprobar contención física | Rutas resolubles | Booleano | Contención falsa ante escape | Sólo lectura. |
| `validate_plan_write_paths(plan)` | Validar todos los destinos | `RunPlan` completo | Lista vacía o rechazados | Diagnóstico antes de escribir | Sólo lectura. |
| `build_directory_allowed(...)` | Autorizar uso de build configurable | Ruta y raíces explícitas | Booleano | Rechaza escapes | Sólo lectura. |
| `build_directory_can_be_deleted(...)` | Autorizar borrado | Layout histórico o marcador | Booleano | Niega por omisión | Sólo lectura. |
| `mark_managed_build(path)` | Marcar build creado por el runner | Ruta ya autorizada | Marcador v1 | Propaga IO | Escritura confinada. |
| `PipelineWorker._base_environment(linker)` | Preparar entorno hijo | Plan validado | Copia del entorno con rutas explícitas | Propaga rutas a logs | Crea temp configurado. |
| `sha256_file(path)` | Hashear artefacto | Archivo regular | SHA-256 hexadecimal | Propaga error de IO | Lectura por bloques. |
| `parse_assignments(values)` | Interpretar `NAME=VALUE` | Nombres de variable válidos | Diccionario | Mensaje concreto por asignación | Sin efectos. |
| `expand_path(template,variables)` | Resolver placeholders | Todas las variables definidas | Ruta absoluta | Lista variables faltantes | Sin efectos. |
| `load_lock(path)` | Validar cabecera e IDs | JSON legible v1 | Lock básico validado | Rechaza duplicados/versiones | Sólo lectura. |
| `capture_version(path,args)` | Consultar identidad | Ejecutable existente | Exit y salida combinada | Timeout/OS error | Proceso sin ventana. |
| `verify_artifact(item,variables)` | Verificar archivo | Entrada de lock | Fila estructurada | Problemas acumulados | Lee y puede ejecutar `--version`. |
| `verify(lock,variables)` | Verificar entorno completo | Lock cargado | Reporte passed/failed | Nunca oculta pendientes | Determinista salvo timestamp. |
| `write_json(path,value)` | Publicar reporte atómico | Padre escribible | Archivo completo | Propaga IO | Temporal y rename. |
| `clean_selfhost.initialize(root)` | Marcar área administrada | Raíz explícita | Ruta del marcador | Propaga IO | Crea raíz/marcador. |
| `clean_selfhost.cleanup_targets(root)` | Enumerar destinos permitidos | Marcador presente | Lista cerrada | Rechaza escape/sin marcador | Sólo lectura. |

## 7. Superficie observable

`nscbuilder.py --baseline` añade `--config`, `--prefix`, `--build-dir`,
`--test-build-lld`, `--test-build-nslink`, `--llvm-root`, `--nslink`, `--history-root`,
`--work-root`, `--temp-root`, `--write-root` y `--build-id`. Las variables equivalentes están
documentadas en `selfhost/bootstrap/path-configuration.md`. `--run-id` permite preasignar una
identidad segura a la evidencia sin depender de adivinar un UUID posterior. El ejemplo fija
`2000000000000000`; el binario resultante conserva SHA-256
`556850a81de30e4a440e4c4ccead6cc27c8b5310e2abb595507e6bf500bc7859` tras reconstruirse.

El verificador termina con 0 si todo coincide, 1 si el entorno diverge y 2 si el lock o los
argumentos son inválidos. La limpieza es no destructiva salvo que se especifique `--apply`.

## 8. Implementación

Se auditó primero el runner y se demostró que una corrida aislada rechaza `C:\native-sharp` antes
de escribir. Después se creó el layout, el lock, los schemas, los verificadores y las pruebas. El
NSLink actual se construyó desde `A:\nslink-devel`, pasó 28/28 pruebas y se instaló explícitamente en
`A:\native-sharp`; su manifiesto de fuentes externo estaba desactualizado y se conserva como
incidencia del producto hermano, sin modificar su código en esta tanda.

## 9. Pruebas

| Prueba | Tipo | Propósito | Resultado | Evidencia |
|---|---|---|---|---|
| Precedencia entorno/config | positiva | Entorno gana al JSON | PASS | prueba Python SH00 |
| Config antes del default | positiva | JSON gana al valor histórico | PASS | prueba Python SH00 |
| Build ID entorno/config | positiva | Entorno gana y formato se valida | PASS | prueba Python SH00 |
| Custodia de `selfhost/` | positiva/negativa | Incluir contratos, excluir salidas y corridas | PASS | prueba Python SH00 |
| Marcador de bibliotecas | regresión | Bytes Python y `.cmd` coinciden | PASS | prueba Python SH00 |
| Guarda de escritura | negativa | Rechazar prefijo fuera de raíz | PASS | prueba Python SH00 |
| Artefacto bloqueado | positiva | Aceptar hash correcto | PASS | prueba Python SH00 |
| Lock pendiente | negativa | Fallar sin hash | PASS | prueba Python SH00 |
| Limpieza sin marcador | negativa | Evitar borrado accidental | PASS | prueba Python SH00 |
| NSLink actual | regresión | Certificar herramienta hermana | 28/28 PASS | `A:\nslink-devel\build-selfhost` |
| Stepping inline O0 | regresión | Aceptar objeto temporal y prohibir fuente físico | PASS | CTest dirigido |
| Stepping assembly O0 | regresión | Aceptar objeto temporal y prohibir fuente físico | PASS | CTest dirigido |
| Evento nombrado | concurrencia | Esperar existencia real, no tiempo supuesto | PASS | CTest dirigido/repetido |
| ABI C/C++ O0 | regresión | Exigir retorno 42 sin contención de procesos | PASS | CTest serial/repetido |

## 10. Comparación diferencial

No aplica todavía a semántica del compilador: SH00 no implementa frontend. La comparación física se
realiza mediante hashes contra `bootstrap.lock`; la comparación `nsc0`/`nsc-next` comienza cuando
existe la primera rebanada NativeSharp observable.

## 11. Baseline de cierre

| Campo | Valor |
|---|---|
| Comando | `python -B nscbuilder.py --baseline --config selfhost/bootstrap/bootstrap.config.example.json --run-id tanda-01-sh00-final4-20260919 --jobs 16` |
| Identificador | `tanda-01-sh00-final4-20260919` |
| Prefijo | `A:\native-sharp` |
| Reporte | `selfhost/evidence/runs/tanda-01-sh00-final4-20260919/report.md` |
| Estado | `PASSED`: 436/436 CTest por enlazador; 296/296 fixtures; 36/36 triage |

### Corridas fallidas anteriores

`tanda-01-sh00-final-20260919` quedó `FAILED` con dos causas de infraestructura conservadas en su
reporte: los árboles CTest configurados bajo `selfhost/out` todavía eran rechazados por una regla
histórica que sólo admitía hijos directos `build-test-*`, y `lib/.source.txt` cambió al incrustar la
ruta física `A:\native-sharp`. Las 296 fixtures y 36 ejecuciones de triage pasaron. La corrección
separa uso y borrado de builds configurables mediante marcador y hace estable el marcador de
bibliotecas.

`tanda-01-sh00-final2-20260919` conservó 296/296 fixtures y 36/36 ejecuciones de triage verdes,
pero rechazó 2/436 CTest por enlazador. Los PDB sí contenían las rutas remapeadas y líneas exigidas:
la prueba confundía la ruta legítima del objeto temporal con una fuga de la ruta física del fuente.
También detectó el cambio LF→CRLF de `lib/.source.txt`. La corrección compara la ruta exacta del
fuente y fija los bytes CRLF del marcador. Ambas pruebas dirigidas quedaron verdes antes del cierre.

`tanda-01-sh00-final3-20260919` certificó que S0 y S1 eran idénticos y que las correcciones de
stepping pasaban. No se aceptó como cierre: una espera fija de 350 ms perdió una señal nombrada en
una de las dos matrices y la prueba ABI C/C++ reprodujo la intermitencia bajo 16 procesos que ya
constaba en `tests/performance/p10/skip-unused-ir-serialization-20260917.md`. El evento ahora usa
un handshake real mediante `OpenEventA`; la prueba ABI conserva el retorno 42 estricto y se marca
`RUN_SERIAL`.

## 12. Riesgos, limitaciones y deuda

- El build ID `2000000000000000` queda reservado al bootstrap reproducible; los builds normales
  conservan su identidad deliberadamente única.
- El manifiesto SHA-256 de `A:\nslink-devel` no coincide con su árbol limpio; su binario y pruebas sí
  se fijan por hash. La reparación del manifiesto pertenece al repositorio NSLink.
- El Bootstrap Profile se mantiene bloqueado hasta la tanda 02.

## 13. Contenido del commit

### Higiene

- [x] No contiene `.obj`, `.exe`, `.dll`, `.lib`, PDB, temporales ni cachés no normativos.
- [x] No contiene cambios locales ajenos a la tanda.
- [x] El manifiesto de fuentes está actualizado cuando corresponde.
- [x] La documentación describe las firmas y archivos realmente incluidos.
- [x] El baseline se ejecutó después del último cambio funcional.

## 14. Criterio de cierre

- [x] Todas las tareas SH00 están terminadas.
- [x] Las pruebas específicas están verdes.
- [x] El baseline completo está verde.
- [x] Gate SH00 tiene evidencia y problemas vacíos.
- [x] El expediente coincide con el código del commit.
