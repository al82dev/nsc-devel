# T08-012: layout obtenido de Clang

Fecha: 2026-11-08. Casillas: T08-012-03a–e, y la verificación de T08-012-02. También incluye la validación del
bloque T08-012.

## Resultado

- **T08-012-03a–e:** verificadas sin cambios en el compilador.
- **T08-012-02:** verificado el contrato vigente de alineación de acceso. La casilla queda bloqueada por la
  decisión pendiente T08-012-02a, ya planteada en `t08-layout-comparacion-clang.md`.

## Prueba

Nueva: `pruebas-tanda-08-layout-c/validar-layout.py`. Aprobada en O0–O3.

**Valores de referencia.** Los da Clang: `sizeof`, `_Alignof`, `offsetof` y `__alignof__`.

**Accesos.** Para cada uno hay un getter y un setter en NativeSharp y otro par en C.

- NativeSharp escribe con su setter y C lee el valor.
- C escribe con su setter y NativeSharp lee el valor.

**Casillas cubiertas.**

- **03a:** campos escalares en un struct natural y bajo `pack(1)`, `pack(2)` y `pack(4)`.
- **03b:** arreglos de records naturales y empaquetados, records anidados en un record `pack(1)`, y accesos de
  dos niveles (`items[1].i`, `packed[1].ll`, `whole.d`).
- **03c:** campos de bits con nombre, sin nombre y de ancho cero, en unidades de 8, 32 y 64 bits.
- **03d:** `_Alignas(16)` en un campo, y typedefs con `aligned(8)` y `aligned(1)`.
- **03e:** un consumidor sin encabezado mide los tipos publicados en su propio código con `sizeof`, `alignof` y
  `offsetof`, y los compara con Clang. Ejecuta además todos los accesos en una plantilla precompilada.

  El hallazgo 3 de `t08-layout-formas-pendientes.md` ya no se reproduce: `sizeof(LNatural)` resolvía como
  identificador desconocido en un consumidor solo con metadata. La sonda de comprobación es
  `A:\nsc-interop-build\claude\sondas\openarr\sonda23.py`.

## Alineación de acceso (T08-012-02)

La prueba compara el `load` y el `store` de cada getter y setter en el IR -O1 de NSC con los de Clang.

**Campos ordinarios.** Los 56 accesos cumplen `NSC = min(alineación de Clang, alineación natural)`, en lectura y
en escritura. Clang usa a veces una alineación mayor que la natural, porque conoce más sobre la dirección:

| Campo | Clang | NSC |
|---|---|---|
| `N.c` | 8 | 1 |
| `N.f` | 8 | 4 |
| `Arr.h` | 4 | 2 |
| `Al.x` (`_Alignas(16)`) | 16 | 4 |
| `AT.a` (`aligned(8)`) | 8 | 4 |

NSC nunca excede la alineación de Clang.

**Campos de bits.** Los 10 accesos usan ventanas que pueden ser más estrechas que las de Clang. Por ejemplo,
`Bits.a` es `i8 align 1` en NSC e `i32 align 8` en Clang. La alineación nunca es mayor que la de Clang, y C
comprueba los valores leídos y escritos.

El informe completo queda en `alineacion.txt`, dentro del directorio de cada ejecución.

## Decisión pendiente T08-012-02a

**Contrato vigente.** La alineación de acceso de un campo no se toma literalmente de Clang. Se deriva como
`min(alineación natural del tipo, gcd(alineación del record, offset))`. Es segura y coincide en los tres caminos
(directo, precompilado con encabezado y solo metadata).

**Opción alternativa.** Adoptar la alineación declarada por Clang (`getDeclAlign`) permitiría emitir accesos más
alineados. Para eso habría que guardarla en la metadata NSMeta, lo que implica cambiar su contenido.

**Lo que se pregunta.** Si se mantiene el contrato derivado o se adopta el de Clang.

## Validación del bloque

- La prueba nueva.
- Las muestras previas `pruebas-tanda-08-layout/validar.cmd` (T08-012-01) y
  `pruebas-tanda-08-layout-clang/validar.cmd`, ejecutadas en la validación del bloque T08-010 con el mismo
  compilador.

Ningún archivo del compilador cambió en este bloque.
