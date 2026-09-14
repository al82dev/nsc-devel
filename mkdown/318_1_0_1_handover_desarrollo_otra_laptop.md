# 318 — Handover integral de desarrollo / Complete development handover

Fecha de corte / Cutoff date: **2026-09-14**  
Repositorio / Repository: `A:\nsc-devel`  
Rama activa / Active branch: `development`

## Propósito / Purpose

Este documento permite continuar NSC en otra laptop sin reconstruir el contexto de conversaciones
anteriores. Separa el último milestone certificado, las entregas posteriores con pruebas
focalizadas, el compilador instalado, las reglas de trabajo y los pendientes reales.

This document supports resuming NSC development on another laptop without rebuilding previous
conversation context. It separates the last certified milestone, later focused-test deliveries,
the installed compiler, operating rules, and actual pending work.

## Estado exacto / Exact state

- Rama: `development`.
- Remoto: `origin`, `https://github.com/alberto-huerta-luna/nsc-devel.git`.
- Último commit funcional antes de este handover: `2aa327fc`.
- Al redactarlo, `development` y `origin/development` apuntaban a `2aa327fc`.
- El commit inmediatamente posterior contiene únicamente este documento.
- Versión: `1.0.1-dev`; frontend `stage 67`.
- Continuar aquí, no en `dlls`, `pdb` ni otra rama.

## Milestones conservados / Preserved milestones

| Rama | Commit | Significado |
|---|---:|---|
| `nativesharp-1.0.1-dev-000` | `5ff40d8a` | Snapshot inicial de 1.0.1-dev. |
| `nativesharp-2.0-tanda-01` | `8a10ebf8` | Milestone de tanda 1. |
| `nativesharp-2.0-tanda-02` | `0ced05ef` | Milestone de tanda 2. |
| `nativesharp-2.0-tanda-03` | `6f84ef6f` | Milestone certificado de tanda 3. |
| `nativesharp-2.0-tanda-04` | `238a4bce` | Milestone certificado de tanda 4. |
| `nativesharp-2.0-tanda-05` | `37a2c13d` | Último milestone con baseline de tanda. |

Las ramas milestone quedan congeladas. El desarrollo continúa sobre `development`.

## Entregas posteriores a tanda 5 / Deliveries after batch 5

1. `4fe0a49f` — Conserva el módulo de genéricos importados desde NSMETA. Evita que
   `Span<T>` y `ReadOnlySpan<T>` pierdan el módulo nominal al reconstruir metadata.
2. `ae2f285a` — Estabiliza cleanups EH anidados / Stabilize nested EH cleanups. Evita
   invalidar iteradores de `std::vector` mientras el lowering amplía planes de cleanup EH.
3. `483574e0` — Conserva tipos externos en plantillas NSMETA / Preserve external types in
   NSMETA templates. Califica tipos externos como `IAllocator` en
   `.nsmeta.public-templates.ns`.
4. `c16afda0` — Resuelve callables genéricos entre módulos / Resolve generic callables across
   modules. Conserva identidad física para firmas como `Bfs.Run` con `Span<NodeId>` y callbacks.
5. `244e6b28` — Expone progreso de compilación por fase / Expose phase-aware compilation
   progress. Agrega `--progress`, flushing inmediato, tiempos por fase y avance por archivo.
6. `2aa327fc` — Agrega proyectos fachada sin redeclarar módulos / Add project facades without
   redeclaring modules. Agrega `ProjectReference` sin duplicar tipos de dependencias.

Documentación:

- `mkdown/312_1_0_1_nsmeta_module_qualified_generics.md`
- `mkdown/313_1_0_1_eh_cleanup_iterator_stability.md`
- `mkdown/314_1_0_1_nsmeta_external_template_type_qualification.md`
- `mkdown/315_1_0_1_identidad_callables_genericos_multimodulo.md`
- `mkdown/316_1_0_1_progreso_compilacion.md`
- `mkdown/317_1_0_1_proyectos_fachada.md`

## Evidencia y certificación / Evidence and certification

Cada entrega se validó con prueba focalizada O0. Evidencia reciente:

```text
nsc_driver_tests: PASS: 217 checks
nsc_project_name_lookup_tests: PASS: 82 checks
cross-module callable fixture: PASS: 7 checks
```

No se ejecutaron runtime, bindings ni tutoriales. **No existe todavía un baseline posterior a estos
seis commits.** No describir `2aa327fc` como milestone certificado.

## Compilador instalado en la laptop de origen / Installed compiler

```text
C:\native-sharp\bin\nsc.exe
nsc 1.0.1-dev+build.20260914T211100Z.c7bb1a2af24dd254
NativeSharp 1.0.1, stage 67
build c7bb1a2af24dd254
SHA-256 EEDAF2EB75EAA53E076939975C06AD5491BB601F0F58107C5A9E9430B4FA22F3
```

Ese binario corresponde a `2aa327fc`. Este documento no modifica el producto. `C:\native-sharp`
es estado local y debe reconstruirse e instalarse en la nueva laptop.

## Reanudación desde Command Prompt / Resume from Command Prompt

```bat
git clone https://github.com/alberto-huerta-luna/nsc-devel.git A:\nsc-devel
A:
cd \nsc-devel
git switch development
git pull --ff-only origin development
git status --short
git log -8 --oneline
```

Si el repositorio ya existe:

```bat
A:
cd \nsc-devel
git fetch origin
git switch development
git pull --ff-only origin development
```

Confirmar que el historial contiene `2aa327fc` y el commit de este handover.

### Rebuild limpio e instalación, sin baseline

```bat
A:
cd \nsc-devel
rmdir /s /q A:\nsc-devel\build-clang
build_nsc.bat --force
C:\native-sharp\bin\nsc.exe --version
```

Eliminar `build-clang` sólo para reconstrucción intencional desde cero. No ejecutar `all.bat` ni
el pipeline completo salvo al final de una tanda o por solicitud explícita.

## Observabilidad / Observability

Para una compilación grande o aparentemente detenida:

```bat
C:\native-sharp\bin\nsc.exe --emit-project-oop-lib Project.nsproj -O0 -o Project.lib -j 16 --progress
```

`--progress` muestra fase, duración y archivos procesados durante lectura, lexer y parser. Usarlo
antes de atribuir lentitud a NSMETA, `LibraryReference`, cantidad de fuentes o paralelismo. La
causa del caso de 310 fuentes que consumió CPU más de 55 minutos **sigue sin verificarse**; ahora
existe instrumentación para localizar la fase dominante.

## Pendientes reales / Actual pending work

1. Ejecutar baseline O0 al cierre de la tanda que incluya los seis commits posteriores a
   `nativesharp-2.0-tanda-05`, sólo cuando corresponda o el usuario lo solicite.
2. Validar contra el siguiente repro mínimo reportado; corregir únicamente el defecto concreto y
   conservar el ciclo commit → documentación → rebuild limpio → instalación.
3. Medir con `--progress` el proyecto grande y optimizar sólo tras identificar la fase dominante.
4. Continuar los ítems pendientes de `docs/1.0.1/NATIVESHARP_2_0_ITEMS_FINAL.md` y
   `docs/1.0.1/ROADMAP_TO_2_0.md`.

No son defectos pendientes de NSC:

- repartir una `partial class` entre módulos; la fuente debe reorganizarse;
- agregar características deliberadamente ausentes sólo para copiar sintaxis C#;
- deformar bindings o runtime para ocultar un defecto reproducible del compilador.

## Reglas operativas / Operating rules

1. Trabajar en `development`, dentro de `A:\nsc-devel`.
2. Instalar únicamente en `C:\native-sharp`.
3. No leer ni modificar otros árboles de `A:` sin autorización expresa.
4. Un cambio lógico por commit.
5. Título y descripción de cada commit en español e inglés.
6. Cada implementación agrega Markdown bilingüe con archivos, funciones, diseño, decisiones,
   evidencia y mensaje de commit reutilizable.
7. Durante una tanda: prueba focalizada O0, rebuild limpio e instalación.
8. No ejecutar baseline por cambio; sólo una vez al final de cada tanda.
9. Tras baseline aprobado, crear rama milestone congelada y seguir en `development`.
10. No correr bindings, runtime ni tutoriales salvo petición explícita.
11. No tocar cambios ajenos o archivos no relacionados del worktree.
12. Entregar comandos al usuario para Command Prompt.

## Estado local no versionado / Unversioned local state

Al corte existen cambios ajenos que **no pertenecen a estas entregas**:

```text
 M docs/1.0.1/NATIVESHARP_2_0_ITEMS_FINAL.md
?? Claude outputs/nativesharp_rt2_remove_numpy_port.bat
?? _clean/
?? _targeted/delete_owned_array_element/
?? _targeted/generic_interface_debug/
?? _targeted/generic_owned_raw_slot/
?? _targeted/item247_*.obj
?? _targeted/item248_*.obj
?? _targeted/item262_cache/
?? _targeted/stage52_delivery245_O0.obj
?? build-development-clean/
```

No limpiar, mover, sobrescribir ni agregar esos elementos. Son del usuario u otros workers y Git no
los transferirá. El roadmap modificado localmente llegará a la nueva laptop sólo si su propietario
lo transfiere por separado.

## Primer paso recomendado / Recommended first step

Actualizar `development`, comprobar el historial y reconstruir una vez. No iniciar baseline
automáticamente. Esperar el siguiente defecto concreto o continuar el roadmap con el mismo ciclo.

## Mensaje de commit / Commit message

```text
Documenta el handover portátil de development / Document the portable development handover

ES:
Consolida el estado de development, milestones, entregas posteriores a tanda 5,
compilador instalado, evidencia focalizada, pendientes y reglas operativas para
reanudar el trabajo en otra laptop sin perder contexto ni ejecutar un baseline.

EN:
Consolidate the development state, milestones, post-batch-5 deliveries, installed
compiler, focused evidence, pending work, and operating rules so development can
resume on another laptop without losing context or running a baseline.
```

