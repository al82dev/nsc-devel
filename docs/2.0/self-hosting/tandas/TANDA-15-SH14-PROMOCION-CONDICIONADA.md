# NativeSharp 2.0 self-hosting — tanda 15: promoción condicionada

## Identidad

| Campo | Valor |
|---|---|
| Estado | `WITHHELD`; Gate SH14 `OPEN` |
| Tareas | `NS2-SH-172` a `NS2-SH-181` |
| Entrada | `e48b1ef1c` |
| Commit | `selfhost(tanda-15): enforce promotion readiness decision` |

## Objetivo y decisión

La promoción deja de ser una afirmación documental y se convierte en una decisión ejecutable. El
verificador lee los gates SH10–SH13, propaga cada problema y exige además
`source_derived_codegen=true`. Sólo con todos los requisitos puede seleccionar `nsc2` como
predeterminado.

En el estado actual la decisión correcta es `withhold`: SH12 y SH13 están `OPEN`, falta la segunda
máquina y el backend completo aún no deriva la imagen de las fuentes. El selector conserva `nsc0`
como predeterminado, publica `nsc2` como candidato y fija `nsc0` como rollback.

## Diseño y métodos

```text
SH10 LANDED ─┐
SH11 LANDED ─┼─> readiness + source_derived_codegen ─> promote | withhold
SH12 OPEN ───┤
SH13 OPEN ───┘
```

| Elemento | Responsabilidad |
|---|---|
| `promotion-readiness.contract.json` | gates, codegen, seed, candidato y diez IDs |
| bucle de gates | exige `LANDED` y propaga `problems` sin filtrarlos |
| decisión | promoción sólo si la lista de bloqueos está vacía |
| `compiler-selection.json` | default/candidate/rollback y booleano de autorización |
| `VerifyNativeSharpPromotionReadiness.cmake` | prueba serial acumulativa |

## Trazabilidad

Los ítems 172–181 quedan implementados como política verificable, manifiestos y receta de
recuperación. Los que requieren promoción efectiva no se marcan realizados: el reporte enumera por
qué no puede publicarse la RC autoalojada, retirar `nsc0` ni firmar el acta final.

## Baseline

Corrida `tanda-15-sh14-withheld-final-20260919`: `PASSED` en 832.253 s.

| Dato | Resultado |
|---|---|
| CTest | 451/451 por linker |
| Fixtures / triage | 148/148 y 18/18 por linker |
| Decisión | `withhold`; promoción no autorizada |
| Selección | default `nsc0`, candidate `nsc2`, rollback `nsc0` |
| Estados | SH10/SH11 `LANDED`; SH12/SH13 `OPEN` |
| Fuente baseline | 1434 archivos; `2906dc697cd07233f30bdc3f8e17f96d069d8b9fb76592364b536a0bc7b9e790` |
| Reportes SH14 | lld `374dd0ea…7c1962`; NSLink `da9241ed…050fdc` |
| Selector | `ed4bc579b297bb6a7f6a8db72aac1b88a0d5966afdb78c8ea4a90d639915d4f0` |

## Cierre

- [x] decisión fail-closed automatizada;
- [x] seed y rollback conservados;
- [x] baseline completo verde;
- [x] Gate SH14 `OPEN` y problemas explícitos;
- [x] commit exacto preparado: `selfhost(tanda-15): enforce promotion readiness decision`.
