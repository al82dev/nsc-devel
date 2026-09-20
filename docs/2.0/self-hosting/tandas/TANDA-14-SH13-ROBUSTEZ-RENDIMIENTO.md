# NativeSharp 2.0 self-hosting — tanda 14: robustez y rendimiento

## Identidad

| Campo | Valor |
|---|---|
| Estado | `LOCAL-PASSED`; Gate SH13 `OPEN` por dependencias heredadas |
| Tareas | `NS2-SH-160` a `NS2-SH-171` |
| Entrada | `17084784a` |
| Commit | `selfhost(tanda-14): measure robustness and performance budgets` |

## Resultado y diseño

La tanda reconstruye primero el punto fijo local y ejecuta sobre `nsc2`/`nsc3` una sonda separada
de robustez. El baseline acumulativo cubre las 449 pruebas existentes, ABI C/C++, DLL, COM, PDB,
LTO/ThinLTO, sanitizers, fixtures y triage; la sonda agrega 24 entradas hostiles deterministas,
metadata corrupta, destino imposible, interrupción y auditoría de temporales.

Los presupuestos no mezclan debug/release ni ocultan números: startup, compilación por O0–O3 y
tamaño tienen límites contractuales y valores medidos en JSON. La optimización sólo se autoriza
después de medir una regresión.

## Métodos y archivos

| Elemento | Responsabilidad |
|---|---|
| `robustness.contract.json` | casos, iteraciones y presupuestos máximos |
| `median_startup` | mediana de siete procesos por imagen |
| bucle fuzz | corpus reproducible con seed fija; permite diagnóstico 1, prohíbe crash |
| prueba metadata | exige rechazo de NSMETA truncada/corrupta |
| prueba de disco | exige fallo no cero y ausencia de artefacto parcial |
| matriz de tiempos | tres muestras por generación y perfil O0–O3 |
| prueba de interrupción | termina una carga larga y espera cierre acotado |
| auditoría temporal | falla ante `.tmp`/`.temp` residuales |
| `VerifyNativeSharpSelfHostRobustness.cmake` | prepara punto fijo y ejecuta la sonda |

Archivos nuevos: contrato, `verify_robustness.py`, orquestador CMake y este expediente. Se actualizan
identidad `bootstrap.sh13`, CMake, tests de infraestructura, roadmap y READMEs.

## Trazabilidad

| IDs | Evidencia |
|---|---|
| 160–162 | baseline CTest/fixtures/triage y matrices ya acumuladas de ABI/producto |
| 163 | 24 fuentes hostiles con seed y SHA-256 |
| 164 | tests de sanitizers existentes dentro de CTest |
| 165–167 | medianas, ratios y presupuestos por startup/compile/tamaño |
| 168 | carga larga e interrupción acotada |
| 169 | fallo atómico y cero temporales filtrados |
| 170 | rutas físicas distintas de nsc0/nsc2/nsc3 registradas |
| 171 | contrato, reporte y expediente operativo |

## Baseline

Corrida `tanda-14-sh13-local-final-20260919`: `PASSED` en 830.883 s.

| Dato | Resultado |
|---|---|
| CTest | 450/450 por linker |
| Fixtures / triage | 148/148 y 18/18 por linker |
| Fuzz determinista | 24/24 por linker; cero crash |
| Startup nsc3/nsc0 lld | 1.2122×, presupuesto 12× |
| Compilación nsc3/nsc2 O0/O1/O2/O3 | 0.9764× / 0.9850× / 0.9626× / 0.9944× |
| Tamaño nsc3/nsc0 | 0.0651×, presupuesto 8× |
| Interrupción / temporales | ejercida / cero fugas |
| Fuente baseline | 1431 archivos; `3f0daab228d564b09bce6fd14fd2916bba5c695ebe37d1d338d3031d8c627c2d` |
| Reportes SH13 | lld `ff31923b…be4925`; NSLink `5e678831…cfe8ee` |

El reporte local tiene `problems=[]`: la sonda cumplió todo su contrato. Gate SH13 queda `OPEN`
por dos dependencias heredadas y enumeradas: SH12 cross-machine y codegen derivado de fuentes.

## Cierre local

- [x] robustez, presupuestos y recuperación documentados y medidos;
- [x] gate aislado en ambos linkers;
- [x] baseline completo verde;
- [x] evidencia promovida;
- [x] commit exacto preparado: `selfhost(tanda-14): measure robustness and performance budgets`.
