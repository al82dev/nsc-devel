# T19-029: unión C de solo `float` por valor con `regcall`

La revisión del 2026-11-16 (REVISION-CIERRES-INTEROP-C-CPP.md) la listó como «rechazo en el plan ABI». Sin embargo, el
commit `de7af8353` (2026-11-14, [agregados homogéneos](t09-regcall-agregados-homogeneos.md)) ya la admitía.

- Clang expande `union { float f; float g; }` con `regcall` como un `float`.
- NSC declara y llama `__regcall3__t09_uf` con `x86_regcallcc` y la unión como `float`, igual que el control de
  Clang (`x86_regcallcc float @__regcall3__t09_uf(float %0, float noundef %1)`).

No hubo cambio de código: esta tarea se cierra con evidencia nueva, tomada con el NSC de `b5ea26021`.

## Evidencia

Con `--precompilado`, O0–O3:

- en llamada directa y por puntero;
- desde una plantilla precompilada, con y sin encabezado.

| Fixture | Estática | DLL |
|---|---|---|
| `pruebas-tanda-09-c-union-convenciones` (también `T09UF` con las otras cuatro convenciones) | `A:/temp/nsc-t09-c-union-convenciones-kfu0g3i3` | `A:/temp/nsc-t09-c-union-convenciones-0auz8zvc` |
| `pruebas-tanda-09-regcall-hfa-uniones` (uniones de `float` y `double`, con arreglo, de tres y cuatro piezas) | `A:/temp/nsc-t09-regcall-hfa-f_nodvih` | `A:/temp/nsc-t09-regcall-hfa-is8f_teg` |

Logs: `logs/fx29-*.log`.

Cada fixture conserva un rechazo genuino: el struct C++ con un campo de bits de ancho cero, que NSC no expande.

## Límites

Los de [agregados homogéneos](t09-regcall-agregados-homogeneos.md#límites): los campos de bits y las bases C++ siguen
fuera de la expansión `regcall`.
