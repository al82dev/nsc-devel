# T08-020: comparación de layout y accesos con Clang

Fecha: 2026-11-09. Casillas: T08-020-02a–d, 03a–c y 04a–d. También incluye la validación del bloque T08-020.

## Resultado

- **Verificadas sin cambios en el compilador:** T08-020-02a–d, 03b, 03c (acceso directo a campos) y 04a–d.
- **T08-020-03a:** comparada. NSC emite `min(alineación de Clang, alineación natural)` y nunca supera a Clang. La
  adopción literal de la alineación de Clang sigue en la decisión pendiente T08-012-02a.
- **T08-020-03c:** el acceso por alias NativeSharp (`var`) a almacenamiento `volatile` sigue bloqueado por la
  decisión pendiente T08-010-05a. El acceso directo a campos coincide con Clang en los dos modos.

## Prueba nueva

`pruebas-tanda-08-layout-comparacion-c/validar-comparacion.py`. El layout y los accesos se aprueban en O0–O3, en el
camino directo y en el de solo metadata. El IR `volatile` se compara en -O1.

**Valores de referencia.** Los da Clang (`sizeof`, `_Alignof`, `offsetof`) mediante `layout(k)`. NativeSharp
calcula los suyos con `sizeof`, `alignof` y `offsetof` en su propio código. Cada acceso se cruza en los dos
sentidos: NativeSharp escribe y C lee con `cget_k`, y C escribe con `cset_k` y NativeSharp lee.

- **02a/02b:** `pack(1)` en struct (`PAnon`, `PHolder`) y en union (`PNum`, dentro de `PHolder` en el offset 1);
  union natural `Num` con `char bytes[12]` (tamaño 16, alineación 8).
- **02c:** miembros anónimos promovidos. Union y struct anónimos en un struct natural (`Anon.i`, `f`, `a`, `b`) y en
  uno `pack(1)` (`PAnon.pi`, `pd`, `x`, `y`). `offsetof` sobre miembros promovidos coincide con Clang. También
  records con etiqueta anidados (`Outer.inr`, `Outer.u`).
- **02d:**
  - arreglos de records naturales (`Rows.rows[3]`) y empaquetados (`Rows.packed[2]`);
  - miembros flexibles de escalares (`Flex.items[]`) y de records (`FlexRec.rows[]`), sobre almacenamiento que
    entrega C.
- **03c:** campos `volatile int` y `volatile long long`, alineados (`VA`) y en un struct `pack(1)` (`VP`). Las
  formas de acceso (operación, ancho, orden y alineación) de la función bajo prueba coinciden exactamente con las de
  Clang:
  - **Microsoft** (`-fms-volatile`): `acquire`/`release` con alineación 4 y 8 en los campos alineados; acceso
    simple con alineación 1 en los empaquetados, porque un acceso atómico desalineado no se emite;
  - **ISO** (`-fno-ms-volatile`): accesos `volatile` simples con alineación 4/8 y 1.

  Coinciden en el camino directo y en una plantilla precompilada consumida sin encabezado. La comparación se
  limita a la función bajo prueba: al integrarla en un `Main` con objetos locales, el optimizador promueve el local
  empaquetado y le sube la alineación, lo que no dice nada del acceso al campo.
- **04a:** el consumidor sin encabezado compara con Clang `sizeof` y `alignof` de los doce tipos publicados.
- **04b:** declara punteros a tipos publicados y los convierte desde `void*` (`(Outer*)raw`, `(OuterAlias*)raw`,
  `(Num*)&whole.u`).
- **04c:** usa `offsetof` y accede a los miembros restaurados (incluidos los promovidos, los anidados, los de
  arreglos y los flexibles) en su propio código, y C comprueba cada valor.
- **04d:** union (`Num`, `PNum`), enum (`Color`, con `sizeof` y paso a C de `Blue = 300000`) y typedef
  (`OuterAlias`) publicados.

El productor reenvía a C con funciones de namespace propias (`global_get_k`), porque sus métodos `cget_k` ocultan a
la función C homónima dentro de la clase (T08-018-02a).

## Cobertura previa usada

- **02a y 03a:** `pruebas-tanda-08-layout-c/validar-layout.py`. Struct natural y `pack(1/2/4)`, y comparación de
  `load`/`store` de 56 campos y 10 ventanas de campos de bits contra Clang.
- **02b:** `pruebas-tanda-08-union-c/validar-uniones.py`. Union natural, empaquetada y anónima, con bytes
  comprobados.
- **02d:** `pruebas-tanda-08-arreglos-c/validar-arreglos.py` y `pruebas-tanda-08-flexible-c/validar.cmd`.
- **03b:** las ventanas de campos de bits de `validar-layout.py`. Pueden ser más estrechas que las de Clang, nunca
  más alineadas, y C comprueba los valores.
- **T08-020-01:** `pruebas-tanda-08-layout-clang/validar.cmd`.

## Validación del bloque

`A:\nsc-interop-build\claude\scripts\validar-bloque-t08-020.cmd`: la prueba nueva, `validar-layout.py`,
`validar-uniones.py`, `validar-arreglos.py`, `validar-etiqueta.py`, `layout/validar.cmd`, `layout-clang/validar.cmd`,
`flexible-c/validar.cmd`, `anonimos-c/validar.cmd` y `bitfields-c/validar.cmd`.

Ningún archivo del compilador cambió en este bloque.
