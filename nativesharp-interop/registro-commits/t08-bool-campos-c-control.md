# T08-006: control independiente de campos `_Bool` en records C contra Clang

## Propósito

Comparar con Clang 23.1.0 (`A:\al82dev-clang`) cómo NSC importa y accede a un `_Bool` dentro de records C: tamaño
y offset del campo, byte que queda en memoria, lectura y escritura cruzadas con C, y tipo de acceso en el IR.
Este commit sólo agrega la prueba; no cambia código de producción, NSMETA, NSLink, el parser ni pruebas
existentes. No cierra T08-006.

SCOPE LOCK: [t08-claude-layout-scope-lock.md](t08-claude-layout-scope-lock.md).

## Fixture (`nativesharp-interop/pruebas-tanda-08-bool-campos-c`)

```c
typedef struct BoolFields { char tag; _Bool flag; _Bool values[3]; int after; } BoolFields;
#pragma pack(push, 1)
typedef struct BoolPacked { char tag; int value; _Bool last; } BoolPacked;
#pragma pack(pop)
typedef union BoolBytes { _Bool flag; unsigned char raw; } BoolBytes;
```

- Campo escalar, arreglo fijo (tres elementos), campo en `pack(1)` después de un `int` y union con un byte
  `unsigned char` superpuesto.
- Tamaños y offsets: `sizeof(bool)`, `sizeof`, `offsetof` de `flag`, `values` y `last` y `sizeof` de la union,
  comparados con los valores de C.
- Por cada ubicación (6): NativeSharp escribe `true` y `false` (el `false` sobre un `1` que puso C); C lee el byte
  crudo con `unsigned char` y el valor lógico `_Bool`, y comprueba que `tag`, `after` y el `int` empaquetado no
  cambian. C escribe `1` y `0` y NativeSharp lee. Ida y vuelta sólo en NativeSharp.
- Llamada a funciones C con parámetro y resultado `_Bool` (`acc_set_flag`, `acc_get_flag`): ABI de llamada, separada
  del almacenamiento.
- Caminos: directo (`Project.nsproj`), biblioteca precompilada con encabezado (`Shared`) y sólo metadata
  (`Metadata`), O0–O3. `Consumer.ns` usa `var` y `sizeof(*p)` por la limitación de nombres de T08-020.
- IR: `verificar_alineacion_clang.py` con `tabla-alineacion.txt` compara la última carga del getter y el último
  almacenamiento del setter con `acc_get_*`/`acc_set_*` compilados por Clang `-O1`, en los tres caminos.
  También se guardan `api-ref-O0.ll` y `project-O0..O3.ll` optimizados.
- Sondas que sólo registran: `ProbeAddress` (`bool* q = &p->flag`), `ProbeCompound` (`|=` y `^=` sobre el campo y
  el elemento) y `ProbeGlobal` (globales `_Bool` no volátiles, fuera de records).

El script ejecuta primero toda la parte de ejecución y al final la comparación de IR; termina con 1 si algún
acceso difiere de Clang. Hoy es la prueba de aceptación de la corrección, no una prueba que deba pasar.

## Resultados sin corrección (build `7fb3fcdc61f265b4`, HEAD de la rama)

Logs `A:\nsc-claude-t08-layout-build\logs\bool-1.log` y `bool-base.log`; IR guardado en
`run\tanda08-bool-campos-c-sin-parche`.
Repetido con el build reconstruido desde HEAD `365e1864c3703bb3` (`bool-restaurado.log`): mismo resultado.

| Comprobación | Resultado |
|---|---|
| Tamaños y offsets contra C | iguales |
| Byte en memoria tras escribir desde NativeSharp `1`/`0` | `1`/`0` en las 6 ubicaciones, vecinos intactos |
| Lectura lógica en C y en NativeSharp | correcta en ambas direcciones |
| Directo, `Shared` y `Metadata`, O0–O3 | 12 de 12 ejecuciones con código 0 |
| Llamadas C con `_Bool` | correctas (códigos 200 y 201 no se producen) |
| IR contra Clang | 24 de 24 accesos `DEFECTO`: Clang `i8`, NSC `i1` (misma alineación 1) |
| `ProbeAddress` | compila y ejecuta con 0; accede con `store i1`/`load i1` |
| `ProbeCompound` | rechazado: `compound assignment requires compatible numeric operands in Stage 34` |
| `ProbeGlobal` | compila y ejecuta con 0; `external global i1`, `[2 x i1]`, `load/store i1` (Clang: `global i8`) |

Conclusión: la representación en memoria coincide con C (un byte con 0 o 1 en el mismo offset); difiere el tipo
con que NSC accede a ese byte. El diagnóstico y el parche propuesto están en
[t08-bool-campos-c-diagnostico.md](t08-bool-campos-c-diagnostico.md).

## Estado

T08-006 sigue abierto. Sin código de producción; sin recursión; sin escrituras fuera de `A:`.
