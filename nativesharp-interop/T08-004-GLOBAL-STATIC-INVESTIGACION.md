# T08-004: globales C de enlace interno

## Hallazgo comunicado

Claude de identidad informó que una global `static` de encabezado no se importa
en NSC, ni siquiera con un solo encabezado. Ese resultado se comunicó desde la
rama `interop-t08-claude-identidad`, que aún no se ha integrado.

## Reproducción en esta rama

El control `pruebas-tanda-08-global-static/validar.cmd` compiló, enlazó y
ejecutó el programa C que alterna acceso directo y funciones inline sobre la
misma variable. NSC rechazó las tres referencias a `shared_state` con
`unknown identifier` antes de producir el ejecutable. El control de NativeSharp
queda pendiente de pasar; no se ha integrado al gate general.

## Rutas observadas en la rama principal

- `src/Interop/Cpp/ClangSession.cpp`, `collectGlobals`: Clang inventaría las
  declaraciones y registra `isExternallyVisible`; únicamente asigna
  `externalSymbol` a las que tienen visibilidad externa.
- `src/Interop/Cpp/CppSemanticInventory.cpp`, `projectGlobalWithLookup`:
  requiere visibilidad y símbolo externos. La selección de globales separa
  las declaraciones internas y la lista de nombres disponibles las omite.
- `ClangSession.cpp`, `emitCStaticInlineIR`: la generación de funciones C
  `static inline` procesa las variables internas del mismo contexto de Clang;
  después exporta las funciones solicitadas. El driver solo solicita este
  objeto para funciones inline seleccionadas.

## Consecuencia para el diseño

Una global C de enlace interno pertenece a su unidad de traducción. No es
correcto enlazarla por su nombre simple como si fuera externa, ni asumir que
es la misma instancia que la de un archivo C compilado por separado. Si una
función `static inline` y el acceso desde NativeSharp proceden de una misma
sesión, es necesario comprobar que ambos observan la misma instancia y que
las sesiones distintas no se mezclan.

Una vía candidata es extender la emisión ya existente para exportar un símbolo
único de la variable en el mismo módulo Clang que las funciones de su sesión,
incluyendo el caso en que ninguna función inline se use. Es una hipótesis de
diseño, no implementación demostrada. Deben investigarse inicialización,
colisiones entre encabezados homónimos, caché y bibliotecas precompiladas antes
de seleccionar una solución. NSLink y metadata permanecen protegidos.
