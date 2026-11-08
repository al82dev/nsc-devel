# T08-006: control de campos volátiles de records C contra Clang (semántica volátil MS)

## Propósito

Reproducir la diferencia que quedó informativa en `ProbeVolatile` (campo `volatile _Bool`) y medir su alcance real
en todos los escalares volátiles de records C. Este commit sólo agrega la prueba; no cambia código de producción,
NSMETA, NSLink ni pruebas existentes. No cierra T08-006.

## Fixture (`nativesharp-interop/pruebas-tanda-08-volatile-campos-c`)

- `VFields`: campos `volatile` `_Bool`, `signed char`, `short`, `int`, `long long`, `float`, `double`, puntero
  (`int* volatile`), elemento de `volatile _Bool flags[2]` y de `volatile int ints[2]`; además `p->i += v` y
  `p->s++` sobre campos volátiles.
- `VPacked` (`pack(1)`): `int`, `double` y `short` desalineados y `_Bool`.
- `VUnion`: `volatile _Bool` superpuesto con `volatile int`.
- Ejecución: NativeSharp escribe y C lee con accesores compilados por Clang, y al revés; vecinos intactos;
  `sizeof`/`offsetof` contra C; directo, precompilado con encabezado (`Shared`) y sólo metadata (`Metadata`), O0–O3.
- IR: `verificar_volatile_clang.py` (nuevo) compara orden atómico, `volatile`, tipo IR y alineación de cada acceso
  con Clang `-O1`, en los tres caminos y en el IR optimizado O1–O3 del directo. Termina con 1 si hay `DEFECTO`.
- Sondas que sólo registran (`ProbeApi.h`, aparte para no contaminar el catálogo del control): `ProbeEnum`
  (campo `volatile` de tipo enum), `ProbeRecord` (campos de un miembro `volatile VInner`), `ProbeInherited`
  (campo de un global `volatile VPlain` cuyos campos no son volátiles) y `ProbeOddProducer`/`ProbeOddConsumer`
  (biblioteca cuyo encabezado contiene campos que NSC no importa, consumida sólo por metadata).

## Regla de Clang 23.1.0 observada (`api-ref.ll`, `probe-ref.ll`)

| Acceso | Clang |
|---|---|
| campo declarado `volatile`, escalar de 1, 2, 4 u 8 bytes, alineado | `load atomic volatile ... acquire` / `store atomic volatile ... release`, alineación = tamaño |
| `float`, `double`, puntero | igual, con su propio tipo IR (`float`, `double`, `ptr`) |
| elemento de arreglo de elementos volátiles | igual |
| `_Bool` en `pack(1)` y en union | igual (1 byte siempre alineado) |
| `int`, `double`, `short` desalineados en `pack(1)` | `load/store volatile` sin orden, alineación 1 |
| `+=`, `++` sobre campo volátil | carga `acquire` y almacenamiento `release` (no RMW) |
| enum `volatile` | atómico `i32` |
| campo de un miembro `volatile VInner` o de un global `volatile VPlain` (volatilidad heredada) | `volatile` sin orden |

## Resultados sin corrección

Build de la rama `af83352bcc0269aa` (log `logs\volatile-rama-4.log`) y build de `fd8ed5c1a`, el commit de
integración de Codex, `80d2c61cb947e44f` (log `logs\volatile-main-base.log`), con el mismo resultado:

| Comprobación | Resultado |
|---|---|
| Ejecución directa, `Shared` y `Metadata`, O0–O3 | 12 de 12 con código 0; valores y vecinos correctos |
| IR, directo / `Shared` / `Metadata` / O1 / O2 / O3 | 28 `DEFECTO` y 6 `IGUAL` de 34 en cada informe |
| Accesos `DEFECTO` | todos los volátiles alineados, `_Bool` empaquetado y de union, `+=` y `++`: NSC emite `volatile` sin `acquire`/`release` |
| Accesos `IGUAL` | `int`, `double` y `short` desalineados de `VPacked` |
| `ProbeInherited` | compila y termina con 0; NSC y Clang usan `volatile` sin orden |
| `ProbeEnum`, `ProbeRecord` | rechazadas: el campo enum volátil y el campo record volátil no se importan |
| `ProbeOddConsumer` | rechazado: `C record field cannot be reconstructed: CVolatileOddProducer::VOdd.mood` |

La diferencia no es propia de `_Bool`: afecta a todos los escalares volátiles de campos C. El diagnóstico y el
parche están en [t08-volatile-campos-c-diagnostico.md](t08-volatile-campos-c-diagnostico.md).

## Estado

T08-006 sigue abierto. Sin código de producción; sin recursión; sin escrituras fuera de `A:`.
