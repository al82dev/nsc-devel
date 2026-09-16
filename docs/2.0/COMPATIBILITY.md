# Certificación final de compatibilidad N-1/N

La ventana final de NativeSharp 2.0 no se obtiene restando componentes SemVer. Sus generaciones
son explícitas:

| Superficie | N-1 | N | N consume N-1 | N-1 consume N |
|---|---:|---:|---:|---:|
| Compilador/productor | 1.0.1 | 2.0.0 | acepta | rechaza |
| NSMETA | 1.20 | 1.20 | acepta | rechaza |
| Native ABI | 1.0 | 1.0 | acepta | rechaza |
| Runtime ABI | 1.0 | 1.0 | acepta | rechaza |

La igualdad de un formato o ABI significa que 2.0 conserva ese contrato físico; no autoriza a un
consumidor 1.0.1 a aceptar productos identificados como 2.0. La migración hacia adelante es una
operación explícita y un consumidor anterior rechaza productores futuros.

La fuente canónica instalada es `NativeSharp2Compatibility.matrix` (`NSCOMPAT-MATRIX 1`).

---

# Final N-1/N compatibility certification

The NativeSharp 2.0 window is explicit rather than derived through SemVer arithmetic. Compiler
producer generations are 1.0.1 and 2.0.0; both generations use NSMETA 1.20, Native ABI 1.0, and
Runtime ABI 1.0. N accepts N-1 and N, while an N-1 consumer rejects an N producer. The installed
canonical authority is `NativeSharp2Compatibility.matrix` (`NSCOMPAT-MATRIX 1`).
