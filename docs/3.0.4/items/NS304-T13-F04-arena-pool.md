# NS304-T13-F04 — Arena/pool contextual

- **Estado**: IMPLEMENTADO (`ArenaAllocator` implementa `IAllocator`; propagación explícita vía F02)
- **Rama**: development

## Resultado (2026-10-14)

- **Protocolo**: `ArenaAllocator` ahora declara `: Allocator, IAllocator`. La clase histórica
  `Allocator` se conserva y sus consumidores no cambian; `IAllocator` es el protocolo canónico para
  consumidores nuevos.
- **`Allocate(int)`**: conserva el reparto incremental actual (encadena bloques, alineación a 8).
- **`Resize(byte* ptr, int oldBytes, int newBytes)`**: valida tamaños; `newBytes <= oldBytes` conserva
  el puntero; un crecimiento reparte un bloque nuevo, copia `min(old,new)` con `Spans.CopyTo` y consume
  el puntero viejo (la memoria física vieja permanece hasta `Reset`).
- **`Deallocate(byte* ptr)`**: decrementa `asignacionesActivas` (obligación lógica terminada), sin
  recuperar bytes individuales.
- **`Reset()`**: ya invalida todos los bloques de golpe (sin cambio).
- **Propagación**: `new (allocator) T(...)` (F02) usa exactamente el allocator recibido; no hay
  herencia implícita de allocator ni global/thread-local.
- Verificado: `TL-LANG-487` — `new (arena) A(42)` construye (`a.X == 42`), `Resize` conserva bytes,
  `Deallocate` baja la cuenta activa (exit 0). Guards 477/484/485/486 PASS.

## Commits

- `697226dd` — `ArenaAllocator` como `IAllocator` (Resize/Deallocate) + corrección del rechazo de
  retorno de `scope` (sólo el valor retornado directo escapa).

## Nota de reemplazo — Congelamiento 3.0 (DEC-09, C-08)

La firma de `IAllocator` que describe este documento fue reemplazada en 3.0, y la nueva es la única que se acepta:

```
byte* Allocate(long bytes, int alignment);
byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment);
void Deallocate(byte* ptr, long bytes, int alignment);
```

`ArenaAllocator` conserva `Allocate(int)` como override de la clase histórica `Allocator`, e implementa
`IAllocator` con la firma nueva.

Una implementación con la firma anterior (`Allocate(int)`, `Resize(byte*, int, int)`, `Deallocate(byte*)`) recibe
en ese miembro un diagnóstico que nombra la firma nueva
(`docs/3.0/decisiones-congelamiento/32-P6-IALLOCATOR-FIRMA-UNICA.md`).
El texto de arriba se conserva como registro histórico de 3.0.4.
