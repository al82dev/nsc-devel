# Cierre de la tanda 18

## Resultado

| Ítem | Evidencia |
|---|---|
| T18-001 a T18-007: línea base, inventario, reglas, combinaciones y pendientes | [inventario](../INVENTARIO-INTEROP-C-CPP.md) |
| T18-008, T18-009: zlib 1.3.2 | [zlib](t18-zlib.md) |
| T18-010, T18-011: ICU4C 80 `icuuc`; constructores que lanzan en locales | [ICU](t18-icu.md), [constructores](t18-constructores.md) |
| T18-012, T18-013: especializaciones de la STL de MSVC | [STL](t18-stl.md) |
| T18-014 a T18-016: regresión de NSC, de interoperabilidad estática y DLL | [validación agrupada](../TANDA-18-VALIDACION-AGRUPADA.md) |
| T18-017 a T18-020: distribución, entorno limpio, requisitos y versiones | [paquete](t18-paquete.md), [distribución](../DISTRIBUCION-INTEROP-C-CPP.md) |
| T18-021: pendientes con tareas específicas | [pendientes](../PENDIENTES-INTEROP-C-CPP.md) |
| T18-022: informe de capacidades y limitaciones | [informe](../INFORME-FINAL-INTEROP-C-CPP.md) |
| T18-023: criterio de terminación | abierto |

## T18-023

El objetivo no se declara terminado. El inventario tiene entradas pendientes (P) y rechazadas (R), y hay
combinaciones sin fixture. Cada pendiente tiene su tarea en la sección 19 de TRACKING (T19-001 a T19-027), que
sigue la tanda 19.

## Pendientes que quedan fuera de la tanda 18

Los encontrados en esta tanda se suman a los de los cierres anteriores en
[PENDIENTES-INTEROP-C-CPP.md](../PENDIENTES-INTEROP-C-CPP.md):

| Pendiente | Ítem | Continúa en |
|---|---|---|
| Destructores que pueden lanzar en locales | T18-011 | T19-009 |
| `null` a un puntero a función C; `new` sobre un struct C; `sizeof` como `int` | T18-009 | T19-002 |
| Alias de namespace como prefijo de tipo (`icu.UnicodeString`) | T18-011 | T19-005 |
| Constructores de `icu::UnicodeString`; métodos que devuelven `UnicodeString&` | T18-011 | T19-010 |
| `std::min` y `std::array::at` (retornos por referencia), `std::pair` (constructor plantilla) | T18-013 | T19-007, T19-010 |
| `std::optional<int>` y `std::string_view` como locales | T18-013 | T19-009 |
| La instalación de NSC no incluye NSLink, `lld-link`, `llvm-lib` ni los encabezados de recursos de Clang | T18-017 | T19-022 |

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
