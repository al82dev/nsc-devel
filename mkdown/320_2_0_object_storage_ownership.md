# 320 — Propiedad independiente del objeto y storage / Independent object and storage ownership

## Resultado / Outcome

El compilador ya no representa como una sola obligación dos lifetimes diferentes. Cada resultado
de expresión y local relevante conserva un par estable:

| Operación / Operation | Lifetime del objeto | Storage |
| --- | --- | --- |
| `new Widget()` | `owned` | `owned` |
| `new (storage) Widget()` | `owned` | `borrowed` |
| `stackalloc T[n]`, arreglo nativo o raw `new` | `none` | `owned` |
| `destroy objeto` | descarga `owned` | conserva `borrowed` |
| `delete objeto` | descarga objeto dinámico | descarga `owned` |

The compiler now models logical object destruction and backing-byte release as independent
obligations. Placement construction begins and owns an object lifetime without acquiring the
caller's bytes; ordinary heap construction owns both.

## Representación congelada / Frozen representation

El par nace en el análisis semántico, se guarda en símbolos locales y tipos de expresión, y cruza
explícitamente HIR y MIR. Casts que conservan dirección, `safe cast`, movimientos, parámetros con
ownership y resultados de llamadas declarados conservan el par aplicable. Los volcados muestran
`object-ownership=` y `storage-ownership=` para poder auditar la decisión sin inferirla de la
sintaxis final.

MIR avanza de esquema 1.3 a 1.4. No cambia NSMETA, runtime ni Object ABI: es una frontera interna
del compilador para impedir que consumidores tardíos vuelvan a fusionar ambos lifetimes.

## Frontera de seguridad / Safety boundary

Este punto hace explícitas y transportables las obligaciones; no convierte aliases en nuevos
dueños, no transfiere el storage suministrado a placement y no añade liberaciones implícitas.
`destroy` sigue usando la entrada object-only del punto 319 y `delete` la entrada histórica que
destruye y libera una vez.

## Cobertura / Coverage

`tests/unit/Item320ObjectStorageOwnershipTests.cpp` comprueba en O0 el modelo semántico, HIR y MIR
para storage de pila, conservación a través de cast, placement, heap, `destroy` y `delete`. Las
regresiones adyacentes de placement, destrucción y ownership permanecen verdes antes del commit.

## Mensaje de commit / Commit message

`Separa propiedad de objeto y storage / Separate object and storage ownership`
