# Tanda 20M — Migración de las 43 rutas NSC directas

## Alcance

La auditoría uno a uno conservaba 43 identificadores históricos sin una ruta
en la infraestructura nueva. Esta tanda migra exactamente esos 43 comandos a
manifiestos de `tests/cases`; no agrega pruebas ni cambia expectativas.

Cada manifiesto conserva:

- el identificador CTest histórico;
- la acción pública de NSC;
- la fuente o proyecto histórico;
- el perfil efectivo original;
- el código de salida y la expresión regular observados por CTest;
- un workspace independiente en A:.

## Inventario migrado y validación

| ID histórico | Perfil fijo | Validación de esta tanda |
|---|---:|---|
| `nsc.version` | O0 | PASÓ individualmente |
| `nsc.llvm-pipeline-o2` | O2 | Representación estática; no ejecutada |
| `nsc.stage30.project-oop-object` | O2 | Representación estática; no ejecutada |
| `nsc.stage31.generalized-receivers` | O0 | PASÓ individualmente |
| `nsc.stage31.receiver-dump` | O0 | PASÓ individualmente |
| `nsc.stage32.inheritance-graph` | O0 | PASÓ individualmente |
| `nsc.stage33.constructor-chaining` | O0 | PASÓ individualmente |
| `nsc.stage33.inheritance-layout` | O0 | PASÓ individualmente |
| `nsc.stage34.virtual-dispatch` | O0 | PASÓ individualmente |
| `nsc.stage34.virtual-object` | O2 | Representación estática; no ejecutada |
| `nsc.stage35-recovery.destruction` | O0 | PASÓ individualmente |
| `nsc.stage35-recovery.object` | O2 | Representación estática; no ejecutada |
| `nsc.cc1.generic-constraints` | O0 | PASÓ individualmente |
| `nsc.stage36-recovery.interfaces` | O0 | PASÓ individualmente |
| `nsc.stage36-recovery.interface-dump` | O0 | PASÓ individualmente |
| `nsc.stage36-recovery.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage37.method-bodies` | O0 | PASÓ individualmente |
| `nsc.stage37.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage38.method-bodies` | O0 | PASÓ individualmente |
| `nsc.stage38.interface-dispatch` | O0 | PASÓ individualmente |
| `nsc.stage38.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage39.method-bodies` | O0 | PASÓ individualmente |
| `nsc.stage39.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage40.members` | O0 | PASÓ individualmente |
| `nsc.stage40.layouts` | O0 | PASÓ individualmente |
| `nsc.stage40.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage42.method-bodies` | O0 | PASÓ individualmente |
| `nsc.stage42.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage43.method-bodies` | O0 | PASÓ individualmente |
| `nsc.stage43.object` | O2 | Representación estática; no ejecutada |
| `nsc.stage44.generics` | O0 | PASÓ individualmente |
| `nsc.stage44.object` | O2 | Representación estática; no ejecutada |
| `nsc.integration.full` | O0 | PASÓ individualmente |
| `nsc.integration.memory-types` | O0 | PASÓ individualmente |
| `nsc.integration.pass-features` | O0 | PASÓ individualmente |
| `nsc.integration.library-check` | O0 | PASÓ individualmente |
| `nsc.dump-symbols` | O0 | PASÓ individualmente |
| `nsc.dump-opt` | O0 | PASÓ individualmente |
| `nsc.dump-pass-pipeline` | O0 | PASÓ individualmente |
| `nsc.dump-llvm-opt-o2` | O2 | Representación estática; no ejecutada |
| `nsc.dump-llvm-raw-with-o2` | O2 | Representación estática; no ejecutada |
| `nsc.verify-llvm-opt-o3` | O3 | Representación estática; no ejecutada |
| `nsc.emit-exe-o2` | O2 | Representación estática; no ejecutada |

Totales de esta tanda:

- 43/43 identificadores representados en manifiestos.
- 27/27 casos O0 ejecutados individualmente y aprobados.
- 15 casos O2 representados sin ejecución.
- 1 caso O3 representado sin ejecución.

## Clausura hermética de restricciones genéricas

`nsc.cc1.generic-constraints` no podía copiar únicamente su archivo principal:
su `.nsproj` contiene un `ProjectReference` y `ProjectLoader` exige que el
proyecto referenciado exista. Para no eliminar la referencia ni debilitar la
prueba, el caso contiene una copia aislada de:

- `c44_constraints_genericos.nsproj`;
- `Main.ns`;
- `NativeSharp.Core.nsproj`;
- las 43 fuentes `.ns` de `NativeSharp.Core`.

Se compararon los 46 archivos fuente contra el fixture, normalizando sólo el
fin de línea: 46 iguales, 0 discrepancias.

## Evidencia global después de la tanda

La auditoría completa de las 684 filas produjo:

```ini
historical = 684
runnable = 642
target_runnable = 204
blocked = 41
unmapped = 0
ambiguous = 1
transitional_wrong_layer = 438
gate_passed = 0
```

La eliminación de `unmapped` no significa cierre. Continúan pendientes las 41
rutas especializadas bloqueadas, la ruta ambigua y la migración de los 438
adaptadores transitorios a su capa final.

## Archivos protegidos

No se modificaron el compilador, `nscbuilder.py`, `_nscbuilder`, las pruebas
históricas, C: ni repositorios de herramientas. No se ejecutaron baseline,
O1, O2, O3, deploy, promoción ni remotos.
