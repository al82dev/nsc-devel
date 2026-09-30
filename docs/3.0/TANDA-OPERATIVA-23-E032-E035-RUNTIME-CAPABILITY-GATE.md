# Tanda operativa 23 — E032–E035: reporte y gate de capacidades de runtime

Fecha: 2026-09-29

Rama: `development`

Alcance: observabilidad verificable del soporte de IPC, sincronización, futures y zonas horarias

Estado de E032–E035: `DISCOVERED` / `IMPLEMENTING`

## Objetivo

Impedir que NativeSharp 3.0 presente como terminadas capacidades de runtime que sólo existen de
forma parcial o todavía no poseen hooks ABI. El compilador ahora deriva un inventario desde su
contrato ABI real y publica tanto evidencia positiva como dependencias faltantes.

Los comandos nuevos son:

```text
nsc --print-runtime-capabilities
nsc --check-runtime-capability <E032|E033|E034|E035>
```

El segundo comando es un gate fail-closed: sólo devuelve éxito cuando la capacidad está completa.
Una capacidad parcial, ausente o desconocida se rechaza con una causa concreta.

## Auditoría previa

- E032, shared-memory regions: no existen hooks ABI de create/open/map/unmap, offset pointers,
  regional allocator ni validación cross-process.
- E033, shared synchronization: sí existen mutex, semáforo y evento nombrados, timed wait y
  monitor/condition-variable in-process; faltan `SharedMutex`, atomic wait/notify y mediciones de
  contención/fairness.
- E034, future continuations: no existen hooks de continuation scheduling, `WhenAll`, `WhenAny`,
  result slots ni propagación cooperativa de cancelación.
- E035, Windows time zones: existe el acceso crudo a system `FILETIME`; faltan base/versionado de
  reglas, conversiones UTC/local y política para tiempos DST ambiguos o inválidos.

## Diseño

Cada entrada contiene:

- identificador de roadmap;
- nombre estable;
- estado `ready`, `partial` o `missing`;
- evidencia encontrada en `kRuntimeAbiSymbols`;
- contratos ausentes que impiden declararla completa.

El reporte incluye el fingerprint de `NativeSharpRuntime.abi`. De esta forma no es una lista manual
desconectada del compilador construido: la evidencia se calcula contra los símbolos ABI que ese
`nsc` reconoce.

## Archivos modificados

- `CMakeLists.txt`: integra el reporte en `nsc_runtime` y registra su ejecutable de prueba aislado.
- `src/Driver/Driver.cpp`: agrega los dos comandos CLI y sus entradas de ayuda.
- `docs/3.0/TANDA-10-COROUTINES-Y-CONCURRENCIA.md`: enlaza el gate con la deuda real de E032–E035.

## Archivos nuevos y por qué

- `include/nativesharp/Runtime/RuntimeCapabilityReport.h`: modelo público del estado de capacidad.
- `src/Runtime/RuntimeCapabilityReport.cpp`: clasificación derivada del ABI, reporte determinista y
  gate fail-closed.
- `tests/unit/NativeSharp3E032E035CapabilityTests.cpp`: ocho comprobaciones específicas sobre
  clasificación, evidencia, deuda y rechazo causal.
- `docs/3.0/TANDA-OPERATIVA-23-E032-E035-RUNTIME-CAPABILITY-GATE.md`: expediente de alcance,
  cambios, validación y deuda de esta tanda.

No se agregaron implementaciones ficticias de shared memory, futures, zonas horarias o primitivas
de sincronización. Esta tanda hace observable la frontera actual y evita falsos positivos.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido correctamente con Clang y advertencias tratadas como error.
2. Target único `nsc_ns3_e032_e035_capability_tests`: `PASS: 8 checks`.
3. `nsc --print-runtime-capabilities` publicó cuatro entradas y el fingerprint ABI.
4. E032 y E034 fueron reportadas `missing`.
5. E033 y E035 fueron reportadas `partial`, conservando la evidencia existente.
6. `nsc --check-runtime-capability E033` fue rechazado con exit code 1 y la primera dependencia
   faltante.
7. La carpeta temporal de la prueba CLI fue eliminada de forma dirigida.
8. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó en ese estado.

No se ejecutó baseline, CTest general, O1/O2/O3, bibliotecas, instalación, deploy, debugger ni
remotas.

## Mejora concreta

El compilador puede responder de manera reproducible qué parte del runtime 3.0 existe realmente.
Automatizaciones futuras podrán bloquear consumidores que requieran E032–E035 sin depender de
roadmaps ambiguos, nombres de clases o documentación optimista.

## Deuda explícita

- implementar y versionar los hooks completos de E032;
- cerrar `SharedMutex`, atomic wait/notify y stress/fairness de E033;
- conectar el contrato de concurrencia T10 con continuations, `WhenAll` y `WhenAny` de E034;
- definir el origen/versionado de reglas Windows y semántica DST de E035;
- agregar lowering, runtime, metadata cross-module y pruebas cross-process donde corresponda;
- cambiar un estado a `ready` sólo cuando la evidencia ABI y la batería dirigida completa existan.

Por estas deudas E032–E035 no se reclasifican como terminadas. La tanda 23 cierra la observabilidad
y el gate del compilador, no las implementaciones de runtime.
