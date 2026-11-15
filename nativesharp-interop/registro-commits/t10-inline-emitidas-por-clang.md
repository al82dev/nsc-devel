# T10-012 y T10-006: definiciones inline C++ emitidas por Clang

## Defecto

En la [auditoría inicial](../TANDA-10-AUDITORIA-INICIAL.md), dos casos compilaban pero fallaban al
enlazar:

- `inline int sq(int x)`, definida en el encabezado y no usada por la biblioteca, daba
  `NSL1020: unresolved external symbol: '?sq@A@@YAHH@Z'`;
- `static constexpr int k = 12` en una clase daba lo mismo con `?k@S@A@@2HB`.

Causa: una función o variable inline de C++ se define en cada unidad de traducción que la usa, así
que la biblioteca no tiene por qué exportarla. Clang solo emite la copia que esa unidad necesita.
NSC importaba la declaración y la llamaba por su nombre, pero ninguna unidad del programa NativeSharp
aportaba la definición. Para las funciones `static inline` de C ya existía una vía
(`emitCStaticInlineIR` y `appendCStaticInlineObjects`), y también para los miembros implícitos de
C++. Para las definiciones inline de C++ no había ninguna.

## Cambio

- `CppInteropSession.h`: nuevo método `emitCppInlineIR(symbols, emitted)`, que por defecto no emite
  nada.
- `ClangSession.cpp` lo implementa así:
  1. Recorre la unidad: namespaces, `extern "C"`, `export` y records no dependientes.
  2. Toma las funciones inline con cuerpo, visibles desde fuera, sin plantilla, no `dllimport` y que
     no sean constructores ni destructores. Toma también las variables inline con inicializador,
     incluidos los miembros estáticos `constexpr`.
  3. Para cada símbolo pedido que coincide con el nombre que Clang da a una de esas declaraciones,
     emite la definición con `CodeGen` y cambia `linkonce_odr` por `weak_odr`. Así la definición
     sobrevive a la optimización y el COMDAT la sigue fusionando con una copia que tenga la
     biblioteca, como en `emitImplicitLifetimeIR`.

  Solo se calcula el nombre de las declaraciones cuyo identificador coincide con alguno de los
  símbolos pedidos.
- `CppLifetimeObjectEmitter.cpp`: `emitCppInlineObject` convierte ese IR en un objeto.
- `DriverArtifactGeneration.cpp` (`appendCppInlineObjects`): ofrece a cada sesión C++ del proyecto
  estos símbolos:
  - los de las funciones C/C++ importadas del MIR, salvo las `dllimport` o de `extern C from`;
  - los de los estados C++ externos de namespace;
  - los de los miembros C++ externos.

  Cada sesión emite los que define inline y cada símbolo se emite una sola vez. Los objetos se
  añaden al paquete del programa o de la biblioteca NativeSharp.

Un símbolo que no tiene una definición inline en el encabezado queda a cargo de la biblioteca, como
antes. No se modificó Clang ni NSLink, y el contrato NSMeta no cambia.

## Evidencia

Fixture nuevo `pruebas-tanda-10-inline/validar.py`, con y sin `--dll`. Encabezado:

- `inline int sq(int)`;
- `constexpr int cube(int)`;
- `inline int mixed(int)`, que llama a `sq` y a `base()`, definida fuera de línea en la biblioteca;
- `inline int counter = 3`, que la biblioteca también usa en `bump()`;
- `struct S` con `static int add(int, int)` definido en la clase y `static constexpr int k = 12`.

De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp;
- exige que las declaraciones que NativeSharp nombra tengan los tipos físicos de Clang en el IR de
  NSC;
- consume una plantilla precompilada, con y sin encabezado, que llama a las funciones inline.

Con biblioteca estática, el programa asigna `counter = 10` y `bump()` en la biblioteca lo eleva a
14: las dos copias COMDAT de la variable se fusionan en una sola, como en el control. Con DLL, la
DLL no exporta `counter`, así que la DLL y el programa tienen cada uno su copia. También así se
comporta el control C++ (resultado 38 en DLL frente a 41 en estática).

El control negativo usa `inline int ghost(int)`, que solo se declara. Sigue sin definición y el
enlace falla con `unresolved external symbol` para `?ghost@T10@@…`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-inline-lxikf3k9` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-inline-xuoqsp2e` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Las variables C++ globales y los miembros estáticos de datos todavía no se reconstruyen en el
  cuerpo de una plantilla precompilada: el fixture los usa solo en la llamada directa.
- Las definiciones inline `dllimport` no se emiten: Clang les da enlace `available_externally` y la
  DLL las exporta.
- Plantillas de función y constructores o destructores inline quedan para las tandas 11 y 13.
