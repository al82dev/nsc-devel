# REF-10 — Cierre de utilidades compartidas, includes y revisión transversal

Fecha: 2026-10-20

Estado de la tanda: **9/9 ítems implementados**.

## HECHO

- `CachePayloadReader` sustituye a `ByteReader` y `MirByteReader` con la unión de sus operaciones usadas.
- `CachePayloadWriter`, `CacheMetadata` y `CacheEntryStore` concentran las implementaciones duplicadas de caché.
- `ScopedTemporaryDirectory` concentra el manejo temporal de los builders de EXE y LIB sin cambiar sus políticas.
- Los consumidores fueron migrados a las utilidades compartidas y las copias anteriores fueron retiradas.
- Los includes directos y transitivos quedaron inventariados en `REF-10-INCLUDE-INVENTORY.tsv`.
- El inventario contiene 712 unidades: 519 archivos `.cpp` y 193 headers internos.
- Los headers públicos conservaron su cierre; los archivos nuevos incluyen directamente sus dependencias.
- El analizador estático no encontró funciones modificadas con más de 60 líneas ejecutables.
- El diff no contiene líneas añadidas con más de 120 caracteres y `git diff --check` terminó sin errores.
- No existe diff bajo `nsc-builder/`.
- `R0-PREPARACION.md`, `R0-SCOPE-LOCK.md` y `refactor-baseline/` permanecen fuera de esta integración.

## Validación focal ejecutada

- La compilación focal limpia de `nsc` completó 565/565 pasos en 163.579 segundos.
- El contrato estático del runtime del host fue aprobado durante el enlace.
- Después de los últimos ajustes mecánicos, la compilación incremental completó 114/114 pasos.
- El `nsc.exe` resultante mide 65,458,688 bytes.
- La suma informativa del `.ninja_log` limpio fue 2,280.661 segundos de CPU frente a 785 segundos en R0:
  +190.53 %. Esta cifra refleja costo de compilación del producto y no rendimiento del compilador ejecutado.

## NO VERIFICADO

- No se ejecutaron suites, baterías ni baseline funcional.
- No se compararon dumps HIR, MIR o LLVM IR contra R0.
- No se compararon NSMETA, objetos, ejecutables ni PDB byte por byte contra R0.
- No se ejecutó self-host.
- No se certificó rendimiento O0/O1/O2/O3 desde la GUI.
- La repetición externa completa y el baseline de la GUI continúan a cargo del propietario.

## Integración

Esta tanda se integra con el mensaje `refactor: consolidate utilities and includes (REF-10)` y no contiene
optimizaciones funcionales.
