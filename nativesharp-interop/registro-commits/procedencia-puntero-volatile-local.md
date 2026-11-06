# Procedencia y orden de punteros volatile en locales

## Cadena actual

1. `CppSemanticInventory.cpp` identifica el origen C y el modo MS del global.
2. `IdentifierAnalyzer.cpp` copia a `ExpressionResult` el calificador volatile
   del objeto apuntado, pero no copia el modo MS del global.
3. `StatementAnalyzer.cpp` conserva el calificador al inicializar el local y
   lo registra en `MethodLocalSymbol`; tampoco conserva el modo MS.
4. `IdentifierAnalyzer.cpp` restituye el calificador al leer el local.
5. `MethodBodyBindingRecorder.cpp` transporta el calificador al binding de
   expresión y `HirCompletionTerminalFrames.cpp` lo utiliza para marcar el
   acceso volatile, sin información del modo de Clang.

El IR NS anterior muestra el caso `var volatile_alias = volatile_pointee`:
la lectura y la escritura mediante el alias son volatile sin acquire/release.
La referencia Clang para `volatile int*` usa esos órdenes en el caso entero
comprobado. La corrección preparada para la desreferencia directa del global
no cubre el alias local.

## Restricciones para la implementación

El local puede reasignarse. Una marca copiada solo desde el inicializador
sería incorrecta si se usara después de asignar otro puntero. El tratamiento
debe definir actualizaciones y combinaciones entre ramas, conversiones,
parámetros y retornos, y diferenciar un modo desconocido o no MS del modo MS.
Las expresiones condicionales y `??` ya combinan otros contratos de puntero en
`OperatorAndConditionalExpressions.cpp`; eso no prueba que hoy combinen el
modo volatile, que no existe en esos modelos. La alineación del valor también
debe conservarse o comprobarse antes de emitir operaciones atómicas.

Estos hallazgos son inspección de código y de IR previo. No implementan el
transporte de modo, no cierran T08-015 y requieren validación con el compilador
actual, incluidos controles de reasignación y convergencia de flujo.
