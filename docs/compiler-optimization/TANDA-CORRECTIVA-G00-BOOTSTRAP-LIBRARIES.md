# Tanda correctiva G00 — bibliotecas bootstrap locales

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

La primera corrida con staging llegó correctamente hasta CTest, pero los verificadores self-host
fallaron al buscar `A:\nsc-devel\lib\NativeSharp.Core.IO\NativeSharpCore.lib`. Ese archivo era una
dependencia oculta de la antigua sincronización C: → A:. Al prohibir esa copia, el defecto de
infraestructura quedó expuesto.

## Corrección

- Después de instalar el candidato en `A:\native-sharp-3.0-baseline`, `nscbuilder.py` compila
  `NativeSharp.Core` con ese mismo candidato en O0.
- `NativeSharpCore.lib`, `NativeSharpCore.nsmeta` y su header se publican bajo
  `A:\native-sharp-3.0-baseline\lib`; no se escribe ningún artefacto en las fuentes.
- Los procesos hijos reciben `NS_LIB_ROOT` apuntando al staging y la biblioteca runtime generada
  por el build actual de A:.
- El driver de `nsc` usa `NS_LIB_ROOT` únicamente como fallback. Una raíz dada en la línea de
  comandos o en el `.nsproj` conserva prioridad.
- No se consulta ni copia ningún archivo desde C:.

## Archivos modificados

- `nscbuilder.py`.
- `src/Driver/Driver.cpp`.
- `tests/selfhost/test_bootstrap_infrastructure.py`.

## Validación específica

- Build incremental exclusivo del target `nsc`: correcto.
- Smoke O0 en temporal de A::
  - `NativeSharp.Core` produjo `.lib`, `.nsmeta` y `.h`.
  - `NativeSharp.Core.IO` consumió Core usando sólo `NS_LIB_ROOT`, sin
    `--library-reference-root`: correcto.
- El temporal del smoke fue eliminado al terminar.
- No se ejecutó CTest ni baseline.
