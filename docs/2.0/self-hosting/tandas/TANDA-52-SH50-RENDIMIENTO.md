# Tanda 52 — SH50: rendimiento medido

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Base | T51/SH49 `4d4b82e5c` |
| Gate | SH50 |

## Objetivo y diseño

Convertir O0–O3 en contratos observables del compilador autoalojado y certificar la infraestructura
de build. El gate compila las 80 fuentes de biblioteca tres veces por perfil con `nsc2`, mide
medianas, exige identidad dentro de cada perfil y cuatro objetos distintos. O0 debe tardar como
máximo 5 s y no más de 60% de O2. Nueve pruebas adicionales cubren caché, incrementalidad,
perfiles, optimización, LTO, ThinLTO, worker pool y scheduler.

## Implementación

| Archivo | Responsabilidad |
|---|---|
| `selfhost/contracts/performance.contract.json` | presupuestos, repeticiones y matriz |
| `selfhost/bootstrap/verify_performance.py` | medición, hashes y validación |
| `cmake/VerifyNativeSharpPerformance.cmake` | gate SH50 reproducible |
| `mkdown/NativeSharp-2.0-Compiler-Performance-and-Diagnostics-Roadmap.md` | integración normativa |

## Resultado dirigido

| Perfil | Mediana | Bytes | SHA-256 |
|---|---:|---:|---|
| O0 | 1.470463 s | 166023 | `431255f65f9069498108b1748a05f17918b84c070827122f159e78761060db75` |
| O1 | 7.248189 s | 126290 | `06e4d0faf87c93fa88bb33d6562c69bc7572ad1292d38bbfd1028eb9d6f107ca` |
| O2 | 8.116816 s | 128406 | `4d1a30feebcd708d5f08b6f7e25372d2d36f23a72baa35d05c2f64dde93b5340` |
| O3 | 9.240456 s | 131532 | `2c606b73d88c9637f9167cf6badb3c6184a9ef770855d29451335c8d342ac5aa` |

O0 usa 18.1% del tiempo de O2; los cuatro perfiles son deterministas y distintos. Las 9/9 pruebas
de build pasaron. SHA-256: contrato
`06e67c75757ff728e8f47f5771aaf2377bcbe1e4b8af55bb4a2553ff864a6bf8`, verificador
`44e736ecb01cbdbb943f77da9b9fb9872658b65dc1f629e151f8de4def106b57`, reporte dirigido
`9b6ae957c57044ee1243b7acc42070094a156c5e89e4e76c446dd8d9093a1ca3`.

## Baseline

`tanda-52-sh50-performance-20260922`, build `5200000000000001`: 485/485 CTests,
148/148 fixtures y 18/18 triaje por cada linker; 8078.172 s, cero fallos y Gate 0 `LANDED`.
SHA-256 del reporte: `9d82a28eec380c5a42bbb2b75b105176a7cdf21ac7a9b2cb85ee2a9765a0870e`.

- [x] O0 es la ruta de menor latencia y cumple ambos presupuestos.
- [x] O1, O2 y O3 producen contratos efectivos distintos.
- [x] caché, incrementalidad, scheduling, paralelismo, LTO y ThinLTO certificados.
- [x] baseline y commit único de T52.
