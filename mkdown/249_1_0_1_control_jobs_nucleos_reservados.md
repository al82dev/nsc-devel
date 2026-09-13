# Entrega 249 — Control de jobs y núcleos reservados / Job and reserved-core controls

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 249.
- Resultado / Result: el compilador posee una política única, comprobable y visible para decidir
  cuántos trabajos podrá ejecutar sin ocupar toda la máquina.
- Baseline: reservado para el cierre de la Tanda 1.

## Problema / Problem

Antes de introducir concurrencia, NativeSharp necesitaba separar tres cantidades que suelen
confundirse: capacidad física informada por el host, paralelismo pedido por el usuario y capacidad
que debe quedar libre. Sin ese contrato, cada fase paralela podría interpretar `-j` de manera
distinta o consumir todos los núcleos durante compilaciones interactivas.

The compiler also needed an observable answer before a worker pool existed. Accepting `-j` while
silently ignoring it would make tests pass syntactically but would give users no way to distinguish
policy calculation from actual scheduling.

## Implementación / Implementation

### `CompilationJobPolicy`

La política normaliza `std::thread::hardware_concurrency()` a un mínimo de uno. Mantiene:

- hilos de hardware detectados;
- jobs solicitados, donde cero interno significa modo automático;
- núcleos reservados, uno por defecto;
- jobs efectivos calculados.

El cálculo es:

```text
available = max(1, hardware_threads - reserved_cores)
effective = automatic ? available : min(requested_jobs, available)
```

Even if the user reserves every reported core, one job remains so compilation can make progress.
An explicit request is a cap, not permission to violate the reserved-core guarantee.

### Interfaz de comandos / Command interface

- `-j N` and `-jN` accept 1 through 1,024;
- `--reserve-cores=N` accepts 0 through 1,023;
- duplicate, missing, non-decimal, zero-job and out-of-range values are rejected as command-line
  errors;
- automatic mode is the default and reserves one core.

`--print-job-policy` exposes the resolved state:

```text
hardware-threads: 16
requested-jobs: 8
reserved-cores: 2
effective-jobs: 8
scheduler: sequential
```

La última línea es deliberada. Este commit define control y capacidad; el ítem 250 introduce el
pool. No se anuncia paralelismo antes de que exista.

### Alcance por thread / Thread scope

`CompilationJobPolicyScope` publishes the active policy through thread-local state and restores a
previous nested policy on destruction. Compiler components introduced by the next deliveries can
read one authoritative value. Worker creation must explicitly propagate or install that policy;
there is no unsafe mutable process-global singleton.

## Decisiones de diseño / Design decisions

1. **Reservar uno por defecto.** Mantiene responsiva la máquina durante desarrollo interactivo.
2. **`-j` es límite.** No sobreescribe la reserva ni inventa capacidad superior al host.
3. **Siempre un job.** Reservas excesivas degradan a ejecución serial en vez de bloquearla.
4. **Dos spellings estándar.** Se aceptan `-j 8` y `-j8` sin crear opciones equivalentes ambiguas.
5. **Máximo explícito.** 1,024 evita valores accidentales o maliciosos antes de reservar recursos.
6. **Consulta honesta.** El reporte muestra `scheduler: sequential` hasta que el pool sea real.

## Archivos y funciones / Files and functions

- `include/nativesharp/Driver/CompilationJobPolicy.h`
  - define límites, política, scope y acceso al valor activo.
- `src/Driver/CompilationJobPolicy.cpp`
  - detecta capacidad, calcula jobs efectivos y restaura scopes anidados.
- `src/Driver/Driver.cpp`
  - analiza las opciones;
  - valida valores y duplicados;
  - instala la política durante toda la operación;
  - implementa `--print-job-policy` y actualiza ayuda.
- `tests/unit/DriverTests.cpp`
  - prueba modo automático, límites explícitos, reservas, fallback de hardware y scopes;
  - prueba el contrato visible y los diagnósticos.
- `CMakeLists.txt`
  - incorpora la implementación al componente `nsc_driver`.
- Contrato, mapa de conformidad y roadmap 1.0.1
  - publican la semántica estable y el estado de la entrega.

## Criterios de aceptación / Acceptance criteria

- Sin opciones, el compilador reserva un core y mantiene al menos un job.
- `-j N` y `-jN` producen la misma solicitud.
- La reserva limita tanto modo automático como explícito.
- Hardware reportado como cero se normaliza a uno.
- Valores inválidos y opciones duplicadas fallan antes de compilar.
- La política activa se restaura correctamente entre scopes anidados.
- `--print-job-policy` distingue capacidad, solicitud, reserva, resultado y scheduler.
- Las pruebas enfocadas y una reconstrucción limpia del compilador pasan.
- No se ejecuta baseline dentro de esta entrega.

## Commit de referencia / Reference commit

**Título / Title**

`Define jobs y núcleos reservados / Define jobs and reserved cores`

**Descripción / Description**

`Add a host-aware compilation job policy with automatic one-core reservation, explicit -j and
reserved-core controls, deterministic validation and reporting, scoped access for upcoming worker
components, and focused contract tests without claiming parallel scheduling yet.`
