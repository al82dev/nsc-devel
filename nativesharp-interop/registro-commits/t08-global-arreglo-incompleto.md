# T08-002: global C `extern T name[];` de tamaño desconocido

Fecha: 2026-11-08. Casilla: T08-002-09m7d1, «Importar una global C `extern T arr[];` de tamaño desconocido».

## Reproducción

Sonda `claude/sondas/ppconst/sonda.py` y luego `claude/sondas/openarr/sonda.py` (encabezado directo y
biblioteca C estática, O0). `extern int open_arr[];` con `return open_arr[1];` fallaba con
«C/C++ global type is not yet mapped». Pasaba lo mismo con elementos `const`, `volatile`, record, enum y
puntero.

Causa: `mapGlobalType` (`CppSemanticInventory.cpp`) solo aceptaba arreglos con todas las dimensiones
conocidas y distintas de cero. El tipo incompleto de C no tenía proyección.

## Diseño

NativeSharp ya tenía la forma que corresponde al uso de C: el `T[*]` sin almacenamiento de los miembros
flexibles C. Con esa forma se indexa en el lugar, el nombre decae a `T*` y nunca se asigna, se copia ni se
mide entero. La global se proyecta como `T[*]`, con el `const`/`volatile` del elemento.

Hay otro modificador, `T[?]`, para typedefs de arreglo incompleto usados detrás de un puntero. Ese modificador
no se puede indexar ni hacer decaer. Por eso no sirve para la global, y elegir `T[*]` no cambia ninguna
semántica visible del lenguaje. Para no crear una identidad que choque con `T[?]*`, se rechaza `&name`
(en C, `T (*)[]`). Indexar y decaer cubren el uso de C.

## Cambio

- `CppSemanticInventory.cpp` (`mapGlobalType`): en C, `T name[]` con elemento escalar, enum, record completo
  o puntero sin calificadores en el dato apuntado se proyecta como `T[*]`. Siguen rechazándose: un elemento
  arreglo, `void`, `_Atomic` o un puntero a función, y la TLS («thread-local C/C++ global is not yet
  projected»).
- `OopLlvmStaticState.cpp`: la global externa `T[*]` se declara como `[0 x i8]` externo.
- `IdentifierAnalyzer.cpp` (estado de espacio de nombres):
  - Una global `T[*]` se marca como miembro flexible C: no es asignable, decae a `T*` (también al inferir
    con `var`) y lleva al puntero el `const`/`volatile` del elemento.
  - `&name` se rechaza con «cannot take the address of C array 'name' of unknown size…».
  - Unión de declaraciones de plantillas: si `T a[]` y `T a[N]` coinciden en todo lo demás, forman el tipo
    compuesto de C y se usa la declaración con límite, una sola en el consumidor. Siguen siendo ambiguos
    dos límites distintos o un calificador, tipo de elemento o alineación distintos.
- `HirConversion.cpp`: el decaimiento `CFlexibleArrayToPointer` de un `NamespaceStateValue` toma la
  dirección de la global (antes solo había decaimiento para campos).
- `ClangSession.cpp`: Clang no da alineación de declaración para un tipo incompleto. Para `extern T a[];` se
  conserva un `_Alignas` explícito (`getMaxAlignment`), que así entra en la comparación de declaraciones.

No cambia NSMeta: la metadata ya guardaba el tipo C completo, incluido `isIncompleteArray`, y la clave
`T[*]` se recalcula en el consumidor.

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/global-externa-variantes/validar-arreglo-incompleto.py`. Resultado:
aprobada en O0–O3.

- Control C con Clang: las mismas lecturas, escrituras y decaimientos sobre el mismo encabezado.
- IR: `@open_arr`/`@vo_arr` son `external global [0 x i8]`, y los elementos de `vo_arr` se leen y escriben
  con `load volatile`/`store volatile`.
- Directo (encabezado y biblioteca estática), O0–O3:
  - elementos `int`, `const int`, `volatile int`, record, enum y puntero;
  - una escritura que C ve después;
  - decaimiento a `int*`, a `var` y a `int const*`;
  - `&open_arr[1]`;
  - el puntero decaído pasado a C.
- `__declspec(dllimport)` desde una DLL C, O0–O3.
- Productor precompilado con plantillas, consumido sin encabezado, O0–O3 (elementos escalares y puntero).
- Dos productores precompilados y un consumidor que también lee la global, O0–O3:
  - `T a[]`/`T a[]`;
  - `T a[]`/`T a[3]` en los dos órdenes;
  - `_Alignas(16)` en las dos declaraciones.
- Rechazos (O0):
  - escritura en un elemento `const`;
  - decaimiento de elementos `const` o `volatile` a `int*`;
  - escritura mediante el `var` inferido de un arreglo `const`;
  - un local `int[*]`;
  - `sizeof`;
  - asignar el arreglo;
  - `&open_arr`;
  - TLS;
  - uniones con otro calificador, otro tipo de elemento, otro `_Alignas` o dos límites distintos
    (`ambiguous namespace state reference`).

Regresión dirigida (`claude/scripts/regresion-globales.cmd`):
- `externa-dos-bibliotecas` (`validar.cmd` y `validar-dependencia-compartida.cmd`);
- `puntero-externo-dos-bibliotecas`;
- `static-dos-bibliotecas` y su variante mutable;
- `homonimos/validar-global-static.cmd` y `plantillas/validar-global-alias-o0.cmd`;
- `global-externa-variantes/validar.py` O0 y O3;
- miembros flexibles C (`pruebas-tanda-08-flexible-c/validar.cmd` y `validar-decay.cmd`).

El caso `arreglo-incompleto` de `global-externa-variantes/validar.py --observar` (no asertado) ahora da
24024. No se modificó esa prueba.

## Límite encontrado (para T08-004-04b)

Una global C de tipo record o enum no se proyecta desde metadata: falla con «precompiled C global cannot be
projected: C/C++ global type is not yet mapped». Pasa lo mismo con `Pair g;`, `Pair g[2];` y `Color g;`, sin
relación con el arreglo incompleto. La sonda es `claude/sondas/openarr/sonda2.py`. Corresponde a la casilla
T08-004-04b («globales de enum, puntero a enum y record C»), así que la plantilla precompilada de esta prueba
solo usa elementos escalares y puntero.
