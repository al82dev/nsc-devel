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

## Puntos concretos del flujo

`BodyScopeAndFlow.cpp` captura y restaura `OwnershipFlowStorageFacts` por
identidad de local o parámetro. `joinOwnershipStorageFact` conserva hechos
como no nulidad únicamente cuando ambas ramas los garantizan. El modo volatile
no está entre los hechos capturados. Las asignaciones llaman a
`invalidateAssignedStorage` y las llamadas con parámetros `ref` u `out`
invalidan hechos en `CallAnalyzer.cpp`; una implementación de procedencia
debe considerar ambas rutas y el reinicio entre funciones en
`MethodBodyCallableLifecycle.cpp`.

`StatementAnalyzer.cpp` captura y combina estados en condicionales, ciclos,
`switch` y `try`. Un detalle que impide aplicar directamente una marca al
inicializador es el retorno a la cabecera de un ciclo: el cuerpo puede cambiar
el puntero y una iteración posterior reutilizar la misma expresión de acceso.
El código analiza la condición una vez y calcula la salida con los estados de
entrada y vuelta; eso, por sí solo, no demuestra que un hecho anotado en un
uso dentro del cuerpo sea válido para todas las iteraciones. Antes de emitir
órdenes distintos por procedencia hay que demostrar el tratamiento del
back-edge o elegir una representación conservadora correcta para ese uso.

Las expresiones condicionales y `??` combinan el calificador del apuntado con
OR y la alineación conocida con el mínimo. Esas operaciones no establecen por
sí mismas cómo combinar modos diferentes o desconocidos. El modo de acceso y
la alineación demostrada son decisiones independientes: no basta conservar
una marca MS si no puede justificarse la alineación que exige la operación.

Este mapa identifica lugares a investigar; no presupone que `volatile` sea
intrínseco al valor del puntero ni que el modelo de hechos existente pueda
reutilizarse sin cambios. Se debe comparar el contrato de acceso del lenguaje
con Clang para cada modo antes de definir la unión e implementar la solución.

## Comparación del modo de la unidad consumidora

Clang 23.1.0, build `ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`, compiló
`pruebas-tanda-08-record-c/VolatileAccessMode.c` con el objetivo
`x86_64-pc-windows-msvc`, C11 y O0. En dos invocaciones que solo cambiaron
entre `-fms-volatile` y `-fno-ms-volatile`, la misma lectura y escritura por
parámetro produjeron, respectivamente:

- MS: `load atomic volatile i32 ... acquire, align 4` y
  `store atomic volatile i32 ... release, align 4`.
- ISO: `load volatile i32 ... align 4` y `store volatile i32 ... align 4`.

Los resultados están en `A:/nsc-interop-build/tanda08-record-c/` como
`volatile-mode-ms.ll` y `volatile-mode-iso.ll`. La fuente de Clang
`clang/lib/CodeGen/CGAtomic.cpp`, función `LValueIsSuitableForInlineAtomic`,
consulta `CGM.getLangOpts().MSVolatile` y comprueba el lvalue y la viabilidad
de la operación; no consulta la procedencia del puntero. `CGExpr.cpp` aplica
esa decisión al generar cada lectura y escritura. La prueba propia de Clang
`clang/test/CodeGen/ms-volatile.c`, función `test2`, incluye punteros
recibidos como parámetros.

**Consecuencia para el diseño:** el experimento demuestra que la opción de la
unidad que realiza el acceso determina el orden en esos casos. No demuestra
que el modo registrado durante la importación describa las opciones con que
se construyó la biblioteca binaria, ni que deba heredarse desde el puntero.
Actualmente `CppSemanticInventory.cpp` conserva el modo de la sesión que leyó
cada declaración, y `HirCompletionTerminalFrames.cpp` lo usa para un acceso
directo a un global C. Esto requiere revisar el contrato del acceso generado
por NSC, incluyendo importaciones con opciones divergentes, antes de extender
ese comportamiento a locales, campos, parámetros y retornos. La comparación
no cambia el código de producción ni cierra T08-015.
