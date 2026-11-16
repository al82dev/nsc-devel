# Tanda 19: esquema `-v1` del snapshot semántico precompilado (T19-037)

La [línea base](t19-linea-base-nsc.md) encontró que `nsc.precompiled-semantic-snapshot-codec` pasa en `152654e5a`
y falla ahora. Se corrige en el compilador; la prueba no se modificó.

## Defecto

`another schema is rejected`: la prueba altera el `-v1` del esquema y espera que la lectura falle. Desde la tanda
8 el esquema es `nsc-precompiled-semantic-snapshot-v3`. La prueba no encuentra `-v1`, la alteración no ocurre
y la lectura no falla.

## Causa

`fc065e44a` y `644b0af5e` (2026-11-05) agregaron dos flags `volatile`:

- `pointerPointeeVolatile` al registro de cada parámetro;
- `declaredPointerPointeeVolatile` en mitad de los flags de cada miembro.

Para invalidar los snapshots anteriores subieron la etiqueta del esquema dos veces. Eso contradice el contrato
único de `AGENTS.md`, que pide compatibilidad por contenido y no por etiquetas, y la prueba existente.

## Cambio

- El esquema vuelve a `nsc-precompiled-semantic-snapshot-v1`.
- Los registros de miembro y de parámetro recuperan su forma `-v1`, sin los dos flags.
- Los flags viajan en un bloque opcional al final de las secciones `members` y `namespace-members`, en el orden
  de sus entradas:
  - por cada miembro: el flag del miembro y los de sus parámetros;
  - por cada función de namespace: los de sus parámetros.
- El bloque se escribe solo si hay un flag activo. Una sección sin flags conserva los bytes `-v1`.
- Lectura:
  - una sección que termina con sus entradas (un snapshot `-v1` anterior, o sin flags) los deja en `false`;
  - si el número de parámetros del bloque no coincide con lo leído, la sección se rechaza y el consumidor vuelve
    a compilar desde la fuente.

Archivos:

- `PrecompiledSemanticSnapshotCodec.cpp`;
- `ClassMemberSnapshotCodec.cpp`;
- `SnapshotCodecSupport.cpp` y `SnapshotCodecSupport.h`, con `anySnapshotParameterVolatile`,
  `writeSnapshotParameterVolatileFlags` y `readSnapshotParameterVolatileFlags`;
- `SnapshotMembers.cpp`;
- `ProjectNamespaceFunctionSnapshots.cpp`.

## Evidencia

- `ninja -k 0`: sin errores ni símbolos sin definir.
- `ctest -R "snapshot|precompiled"`: 10 de 10. Incluye `precompiled-semantic-snapshot-codec`,
  `precompiled-snapshot-equivalence` y `precompiled-snapshot-fallback` (`logs/ctest-sn1.log`).
- Validación del bloque (regresión de interoperabilidad estática y DLL, y CTest completo): se registra en
  TRACKING al terminar.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
