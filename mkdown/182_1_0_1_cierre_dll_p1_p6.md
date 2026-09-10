# 1.0.1 — Cierre DLL-P1 a DLL-P6 / DLL-P1 through DLL-P6 closure

## Estado / Status

**ES.** Los seis lotes del roadmap DLL quedaron implementados, probados y
guardados como commits independientes en la rama `dlls` del checkout
`A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls`.

**EN.** All six DLL roadmap batches are implemented, tested, and stored as
independent commits on the `dlls` branch in
`A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls`.

| Lote / Batch | Commit | Resultado / Outcome |
|---|---|---|
| DLL-P1 | `9c74e483` | matriz cruzada 2×2×2×2 / 2×2×2×2 cross-linker matrix |
| DLL-P2 | `af5806a6` | NSMETA histórico 1.0–1.8 / historical NSMETA 1.0–1.8 |
| DLL-P3 | `c6820bd1` | genéricos fuente-cero O0/O2 / source-free generics O0/O2 |
| DLL-P4 | `0c17e3dc` | herencia, dispatch y lifetime mixtos / mixed inheritance, dispatch, lifetime |
| DLL-P5 | `adb64351` | clausura dinámica y paquete determinista / dynamic closure and deterministic package |
| DLL-P6 | `1a491fdf` | matriz física negativa / negative physical-artifact matrix |

## Baseline local / Local baseline

CTest se configuró para medir explícitamente:
CTest was configured to measure explicitly:

```text
A:\NSC-DEVEL-BRANCHES\nsc-devel-dlls\build-clang-lldlink\bin\nsc.exe
```

Resultado final / Final result:

```text
lld-link: 308/308 PASS — 162.46 s
NSLink:   308/308 PASS — 165.95 s
```

La primera pasada reveló un defecto independiente en el lector del mapa de
conformidad: el último campo conservaba `CR` al leer un archivo CRLF. Se corrigió
el test para normalizar el fin de línea y se repitieron ambos baselines completos.

The first pass exposed an independent conformance-map reader defect: the final
field retained `CR` when reading a CRLF file. The test now normalizes the line
ending, and both full baselines were repeated.

No se ejecutó `cmake --install`, no se escribió producto en `C:` y no se usó ni
modificó `A:\nsc-devel`.

No `cmake --install` was run, no product was written to `C:`, and
`A:\nsc-devel` was neither used nor modified.
