# T08-004-04b: globales C de enum, puntero a enum y record

Fecha: 2026-11-08. Casilla: T08-004-04b.

## Reproducción

Sondas `claude/sondas/openarr/sonda2.py` y `sonda6.py` (O0). Una plantilla precompilada usa una global C
record o enum (`Pair g;`, `Pair g[2];`, `Color g;`, `Color *g;`), y el consumidor se compila sin el
encabezado:

| Plantilla | Consumidor antes |
|---|---|
| solo `one_pair.a` | «cannot import C/C++ global 'one_pair': C/C++ global type is not yet mapped» (antes de `1d7b1c00f`, error fatal «precompiled C global cannot be projected») |
| `Pair p = one_pair;` (el tipo aparece en la plantilla) | correcto |
| `make_pair(1).a + one_pair.a` (el tipo aparece en una función) | correcto |
| `(int)one_color`, `color_ptr == null` | mismo error |

Causa: el productor sembraba en el catálogo C de la metadata los records y enums de las firmas publicadas y
de las funciones C capturadas, pero no los de las **globales** capturadas. Si el tipo solo aparecía como tipo
de una global, el consumidor no lo tenía y no podía proyectar la global.

## Cambio

`CppSemanticRecord.cpp` (`provideCppCRecordMetadataCatalog`): cada global capturada para una plantilla se
proyecta con el lookup del productor, y su clave de tipo se visita como las de las funciones. Así se
siembran el record o enum, también a través de punteros y arreglos. No cambia NSMeta: el catálogo ya podía
contener esos tipos.

## Prueba

Nueva: `pruebas-tanda-08-globales-c/validar-enum-record.py`. Resultado: aprobada en O0–O3, con rechazos en O0.

- Globales:
  - enum con nombre y con enumerador negativo;
  - enum anónimo con typedef;
  - puntero a enum;
  - struct con `long long`;
  - union;
  - puntero a struct;
  - record `const`;
  - record `volatile`.
- NativeSharp lee los valores, escribe las mutables (C comprueba las escrituras) y lee lo que C cambia
  después. También copia la global record a un local y la vuelve a asignar entera.
- Las mismas comprobaciones se hacen:
  - con un control Clang;
  - directo con biblioteca estática;
  - con datos `dllimport` de una DLL;
  - desde una plantilla precompilada sin encabezado, con una plantilla por global para que cada tipo
    aparezca **solo** como tipo de su global.
- Rechazos:
  - escribir un campo de la global `const`, o la global entera: «left side of assignment is not
    assignable»;
  - asignar un puntero a la global enum: «cannot implicitly convert assignment RHS».
- Asignar a `g_color` un enumerador de otro enum se acepta, como en C.

## Hallazgos registrados

- Un cast a un typedef C dentro de una plantilla pública, por ejemplo `(Color)2` o `(MyInt)2`, impide
  publicar la biblioteca: «generated precompiled public template surface is not valid NativeSharp source:
  expected ';', found '2'». Un cast a un enum NativeSharp funciona. Sonda: `claude/sondas/openarr/sonda8.py`.
  Se registra en T08-005-03c (consumo de typedef a través de metadata).
- Una plantilla pública que llama a un método `static` **privado** de su clase no lo encuentra en el
  consumidor («unknown identifier 'changed'»). Ya se había observado con campos privados en
  [t08-global-no-importable](t08-global-no-importable.md). Es una regla general de las plantillas
  transportadas, no de la interoperabilidad C.
