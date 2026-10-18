# Tanda NSC 3.0.5 - D02: despacho de acciones del driver

## Clasificación y resultado

- **IMPLEMENTACIÓN VIGENTE:** extracción interna descrita por este expediente.
- **HECHO COMPROBADO:** observación directa del árbol, del diferencial o de una
  comprobación estática reproducible.
- **NO VERIFICADO:** comportamiento que requiere compilación o ejecución y queda
  diferido a la tanda integral solicitada por el usuario.

D02 extrae de `Driver::run` el despacho de las acciones públicas del compilador
hacia `CompilerActionDispatcher`. La extracción conserva el vocabulario completo
de la CLI, el orden observable de preparación y despacho, los códigos de salida,
los diagnósticos y todas las llamadas históricas del driver.

`Driver::run` pasa de 1,082 líneas físicas y 730 ejecutables en el commit D01 a 29
líneas físicas y 19 ejecutables. Las 32 funciones nuevas o modificadas por D02
quedan dentro de la cota obligatoria de 60 líneas ejecutables; el máximo es
`dispatchFourArguments`, con 49 ejecutables.

**VALIDACIÓN DINÁMICA DIFERIDA A LA TANDA INTEGRAL.** Por instrucción expresa del
usuario no se compiló NSC ni se ejecutaron pruebas, baseline, deploy o instalación.

## SCOPE LOCK

| Dimensión | Alcance vinculante |
|---|---|
| Repositorio y rama | `A:\nsc-devel`, rama `development`. |
| Naturaleza | Refactor interno no funcional por extracción física y descomposición cohesionada. |
| Objetivo exacto | Separar el despacho de acciones de `Driver::run` y dejar todas las funciones modificadas con un máximo de 60 líneas ejecutables. |
| Base inmediata | D01, commit `cf95671db6e49211a84ca7aed5d0ed0c5380a196`. |
| Contratos preservados | CLI, defaults, orden, diagnósticos, códigos de salida, perfiles, caché, FP, jobs, outputs, API, ABI y NSMETA. |
| Archivos funcionales autorizados | `Driver.h`, `Driver.cpp`, `CompilerActionDispatcher.h`, `CompilerActionDispatcher.cpp` y la lista `nsc_driver` de CMake. |
| Archivo documental autorizado | Este expediente. |
| Pruebas protegidas | Todo `tests/`; D02 no modifica ni crea pruebas. |
| Trabajo concurrente protegido | HIR, MIR, Parser, `tests/unit/ProjectMethodBodyModelTests.cpp`, tracking y expedientes ajenos. |
| Validación autorizada ahora | Auditoría estática, identidad de literales/acciones, tamaño, referencias, infraestructura y diff. |
| Validación prohibida ahora | Build, pruebas, baseline, deploy, instalación y ejecución del producto. |
| Red y unidades | Sin `push`, `pull` ni `fetch`; todas las operaciones controladas permanecen en `A:`. |
| Criterio de terminación | Despacho completo una sola vez, cero acciones/literales perdidos, toda función dentro de la cota e infraestructura cerrada. |

## Presupuesto de cambio

### Archivos nuevos

- `src/Driver/CompilerActionDispatcher.h`
  - declara únicamente el nuevo tipo `CompilerActionDispatcher`;
  - mantiene prestados los argumentos y el estado normalizado de D01;
  - expone sólo `dispatch` y el puente de help requerido por la coordinación.
- `src/Driver/CompilerActionDispatcher.cpp`
  - contiene las tablas inmutables de acciones;
  - separa el despacho por aridad y familia;
  - delega en las mismas operaciones privadas de `Driver` mediante amistad explícita.

### Archivos modificados

- `include/nativesharp/Driver/Driver.h`
  - declara la amistad mínima para que el dispatcher invoque la implementación
    privada existente sin ampliar la API pública.
- `src/Driver/Driver.cpp`
  - conserva interpretación de consola y comandos de proyecto;
  - descompone la preparación de la sesión en helpers cohesionados;
  - delega únicamente el tramo de despacho.
- `CMakeLists.txt`
  - registra `src/Driver/CompilerActionDispatcher.cpp` exactamente una vez en
    `nsc_driver`.

No se mueve, elimina ni renombra ningún archivo existente. No se agrega una prueba
nueva ni un catálogo alterno de fuentes.

## Orden observable preservado

La secuencia externa permanece:

1. interpretar opciones de consola y seleccionar idioma;
2. rechazar el error de consola antes de crear una sesión;
3. desviar comandos de proyecto al dispatcher histórico de project commands;
4. construir execution scope, presupuesto O2, política de jobs, caché y estado FP;
5. publicar temporalmente el execution scope activo;
6. mostrar help para una invocación vacía;
7. interpretar la invocación normalizada de D01;
8. transferir timing, progreso, timeout, presupuesto y jobs a sus scopes;
9. instalar y reportar el modo de punto flotante;
10. rechazar una invocación normalizada sin acción;
11. validar la invocación;
12. instalar raíz y podar caché cuando corresponde;
13. validar las opciones de enlace;
14. reportar el perfil de compilación;
15. despachar por aridad y, si no existe coincidencia, probar las formas de
    emisión antes del diagnóstico final;
16. restaurar el execution scope activo tanto en retorno normal como ante una
    excepción.

La preparación de argumentos continúa en D01. D02 no vuelve a interpretar `argv`,
no introduce defaults y no consume opciones.

## Precedencia del despacho

El dispatcher conserva las fronteras históricas:

- un argumento: consultas del compilador, lenguaje, políticas, utilidades,
  opción desconocida y finalmente modo programa;
- dos argumentos: migración, runtime/numeric, operaciones de ruta, PE y
  compatibilidad, metadata/biblioteca, proyecto y frontend;
- tres argumentos: artefactos, API, simulación de deprecación, matriz de
  migración, ABI, manifiestos, comparación, plan de retiro y bundle diagnóstico;
- cuatro argumentos: simulación de deprecación con excepción de seguridad,
  artefactos, fixes/guía de migración, manifiesto/update plan y emisiones;
- cinco argumentos: artefactos y aplicación del plan de retiro;
- fallback: generación ejecutable abreviada, emisiones con `-o` y diagnóstico
  final con el mismo stage.

Las acciones son disjuntas dentro de cada familia. Las tablas sustituyen cadenas
lineales de comparaciones sin cambiar la operación seleccionada. El dispatcher
detiene la búsqueda en la primera coincidencia; no evalúa handlers posteriores con
efectos laterales.

## Propiedad, vida útil y estado

`CompilerActionDispatcher` no posee ni copia `argv`. Conserva referencias o spans
a objetos cuya vida cubre la llamada síncrona a `dispatch`:

- `Driver`;
- `CompilerInvocation`;
- `CompilationJobPolicy`;
- `ProjectLoadOptions` dentro del execution scope;
- streams de salida y error.

No se crea singleton ni estado mutable global. El único estado thread-local
preexistente, `activeExecutionScope`, conserva instalación y restauración. La ruta
de `writeDiagnosticBundle` recibe exactamente las opciones publicadas por ese
scope; no modifica su procedencia ni su contenido.

## Evidencia mecánica de paridad

La revisión D01 de `Driver::run` es el oráculo textual inmediato.

| Inventario | D01 | D02 | Faltantes | Extras |
|---|---:|---:|---:|---:|
| Acciones `--...` únicas | 150 | 150 | 0 | 0 |
| Literales únicos del tramo funcional | 243 | 243 | 0 | 0 |

La comparación de literales excluye únicamente las directivas `#include` del nuevo
translation unit. Incluye acciones, diagnósticos, prefijos de éxito, argumentos
sintéticos y nombres de políticas. Por tanto, no se oculta un texto cambiado bajo
una comparación limitada a opciones.

La auditoría diferencial verificó además:

- el bloque de argumentos vacío sigue antes del parser de D01;
- la configuración derivada se aplica incluso si el parser devuelve error, igual
  que en D01;
- FP continúa después del parse exitoso y antes de las validaciones;
- poda de caché continúa entre validación general y validación de enlace;
- profile report continúa antes del despacho;
- el bundle diagnóstico conserva `--verify-project-oop-llvm -O0` y las mismas
  `ProjectLoadOptions` activas;
- el diagnóstico final conserva idioma, texto, stage y `ExitCode`.

## Cota estructural

Se midieron todos los cuerpos nuevos o modificados excluyendo firma, blancos,
comentarios y llaves aisladas.

| Símbolo | Físicas | Ejecutables |
|---|---:|---:|
| `Driver::run` | 29 | 19 |
| `runCompilerSession` | 31 | 20 |
| `executeCompilerSession` | 33 | 20 |
| `parseAndConfigureInvocation` | 27 | 16 |
| `installAndReportFloatingPoint` | 28 | 22 |
| `pruneInvocationArtifactCache` | 27 | 18 |
| `validateInvocationAndCache` | 17 | 8 |
| `reportCompilationProfile` | 13 | 9 |
| `CompilerActionDispatcher::dispatchThreeArguments` | 47 | 36 |
| `CompilerActionDispatcher::dispatchFourArguments` | 66 | 49 |
| `CompilerActionDispatcher::dispatchEmission` | 48 | 43 |

Las demás funciones del dispatcher tienen 23 líneas ejecutables o menos. Resultado
total: 32 funciones medidas, cero por encima de 60 líneas ejecutables. La única
función con más de 60 líneas físicas es `dispatchFourArguments`; su cuerpo tiene 49
líneas ejecutables y no comprime varias operaciones para evadir la medición.

No se deja un `CompilerRunSession` local ni un segundo tipo nuevo en `Driver.cpp`.
El nuevo header declara un solo tipo: `CompilerActionDispatcher`.

## Infraestructura y cobertura existente

Builder 3 ejecuta la configuración del repositorio raíz mediante
`nsc_builder/build.py` y consume el target `nsc` construido por ese grafo. No
mantiene un inventario paralelo de fuentes de `nsc_driver`; por ello, el registro
único en el CMake raíz es la integración aplicable al nuevo `.cpp`.

La regla posterior del repositorio, materializada por
`tests/compiler-suite/controller/integration/validate_source_registration.py`,
compara `nsc_frontend`, la suite de componentes, todas las pruebas unitarias y los
módulos registrados. D02 no agrega una fuente `nsc_frontend`, una prueba ni un
módulo; no requiere una segunda entrada en la suite.

La cobertura focal existente se conserva sin editar:

- `tests/unit/DriverTests.cpp`, registrado como `nsc_driver_tests`, cubre help
  vacío/localizado, opción desconocida, errores de parsing, job policy, acciones
  de proyecto/frontend, bundle diagnóstico y emisiones;
- `Stage46NativeLibrariesTests.cpp` cubre las variantes de tres, cuatro y cinco
  argumentos de `--verify-library-artifacts`;
- `Stage64InstallationManifestTests.cpp` cubre escritura/verificación de
  manifiestos;
- `Stage66E9E10ReleaseContractTests.cpp` cubre el plan de retiro e instalación.

D02 no cambia expectativas ni agrega una prueba duplicada de la lógica productiva.

## Gates estáticos

- acciones D01/D02: 150/150, faltantes 0, extras 0;
- literales funcionales D01/D02: 243/243, faltantes 0, extras 0;
- funciones nuevas/modificadas: 32, mayores de 60 ejecutables: 0;
- tipos nuevos definidos: uno, en su propio archivo;
- `CompilerActionDispatcher.cpp` aparece exactamente una vez en `nsc_driver`;
- pruebas nuevas o modificadas por D02: 0;
- API pública nueva: 0;
- cambios intencionales de ABI/NSMETA/formatos/diagnósticos: 0;
- recursión nueva: 0;
- trabajo concurrente incluido en el diferencial D02: 0.

## Validación diferida

La tanda integral deberá ejecutar, como mínimo:

- configuración y construcción oficial de NSC;
- `nsc_driver_tests`;
- casos positivos y negativos representativos de cada aridad;
- emisión de obj, exe, lib, dll, metadata y host header;
- acciones de metadata, ABI/API, instalación y diagnóstico;
- suite externa autorizada y, al final de todas las hojas, baseline completo.

Hasta entonces no se reclama compilación correcta ni paridad dinámica.

## Estado para integración

D02 alcanza **CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL
USUARIO**. Este resultado satisface el gate estructural pendiente de D01: el parser
de invocación y el despacho quedan separados, y `Driver::run` ya no viola la cota
de 60 líneas ejecutables. No se modifica tracking externo ni se cierra la meta
persistente por esta hoja.
