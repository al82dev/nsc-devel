# Selección para revisión: vistas de salida y capturas

Base del checkout aislado: `8046d2223c3f5de5894313d3c90fb1fa7e84bc5e`.
El diff revisable de archivos versionados es `reviewable.patch`. Las dos
pruebas originales copiadas son `tests/integration/borrowed_lambda_owned_view.py`
y `tests/integration/borrowed_lambda_delegate_ref_out.py` de
`A:/nsc-devel-fixes-agent-1`. Se comprobó que coincidían con las del árbol
principal al copiarlas. No se modificó ese árbol.

## Fragmentos y dependencias

1. `CallAnalyzer.cpp`: `prepareIndirectViewArguments` y
   `recordViewOutputOrigins`, sus puntos de uso en llamadas de método,
   namespace, delegado y evento; seguimiento de receptores de delegados y
   lambdas con captura propia, y la guarda de alias estabilizada. La extracción
   omite las piezas de `++`/`--` y de alias de arreglos del mismo archivo.
2. `ProjectMethodBodyModel.h` declara `recordViewOutputOrigins` y
   `tupleTypeKeyContainsView`; `MethodBodyTypeQueries.cpp` implementa la
   consulta iterativa de vistas en tuplas.
3. `OwnershipAnalyzer.cpp` conserva la procedencia de campos y tuplas de vista
   en llamadas. Se omite la extensión de préstamo usada por argumentos de
   incremento y campos perteneciente a otro conjunto.
4. `AssignmentAnalyzer.cpp` incluye el seguimiento de vistas en tuplas, el
   rechazo de destinos externos para tuplas que llevan vista y la comprobación
   del reemplazo de un delegado con un préstamo activo. Se omiten los cambios
   de arrays, alias desconocidos y campos del conjunto de operadores.
5. `IdentifierAnalyzer.cpp` protege el paso mutable `ref`/`out` del delegado
   propietario durante un préstamo. Se omiten las reglas de `++`/`--`.

El `CMakeLists.txt` de la copia principal ya contiene registros provisionales
para ambas pruebas, pero se excluyó del parche porque también contiene cambios
de otros conjuntos. Deberán integrarse sus dos registros de forma selectiva.
El parche no incorpora `ExpressionResult.h`, `MethodLocalSymbol.h`,
`LambdaDelegateBinder.cpp`, cambios del driver ni código de interoperabilidad.
Esto no demuestra que todos esos cambios sean prescindibles para todos los
casos de vistas: solamente se evaluaron las pruebas enumeradas abajo.

## Compilación y pruebas

Las cinco unidades C++ compilaron contra el `include` de esta selección usando
la configuración de compilación incremental preexistente; evidencia en
`compile-second.log` y `compile-alias.log`. La primera extracción usó posiciones
incorrectas y falló sintaxis, con log en `compile.log`; se retiró íntegramente y
se corrigió el extractor antes de la segunda compilación.

Se enlazó otro compilador con las cinco fuentes de la selección, pero usando
las cabeceras, objetos y bibliotecas WIP copiados del build principal, para no
reconstruir todo. Sus entradas y hashes están en
`A:/nsc-fixes-agent1-temp/view-selected-hybrid/inputs.json`, su comando en
`hybrid-build.log` y su SHA-256 es
`c5ef2923d2ede796d6dcef45c95b307220200c13dec77f2c9149b43234dd14b9`.
Es un híbrido: las cabeceras usadas para esa construcción incluyen trabajo WIP
ajeno a la selección. No prueba que un ejecutable limpio del commit base más
el parche se enlace ni se comporte de la misma forma.

Con ese híbrido terminaron sin fallas las dos pruebas lambda y las pruebas
dirigidas `borrowed_view_ref_input`, `borrowed_view_in_input`,
`borrowed_view_receiver_output`, `borrowed_bound_delegate_output`,
`borrowed_bound_delegate_alias`, `borrowed_view_owner_reference`,
`borrowed_view_owner_field`, `binary_view_receiver_output`,
`binary_view_ref_input`, `binary_view_owner_field` y
`binary_view_owner_reference`. Los logs correspondientes están junto a este
archivo, con prefijo `extra-` salvo los dos lambda; se habilitó la ejecución de
controles seguros en O0/O2 y no se ejecutaron los casos inseguros. Core fue la
biblioteca aislada `A:/nsc-fixes-evidencia/pointers-readonly-sdk/NativeSharpCore.lib`.
Las bibliotecas binarias usadas no certifican otras configuraciones o targets.

## Límites y siguiente paso

La compilación limpia de cada unidad y el enlace híbrido son evidencias
complementarias. No se deben combinar como prueba de un build limpio, pues las
cabeceras y las bibliotecas WIP incluyen miembros adicionales que pueden
afectar el layout. Falta integrar el parche con el árbol principal, revisar los
fragmentos solapados, registrar las dos pruebas y repetirlas con su compilador
incremental resultante. No se creó un commit porque esa integración y la
trazabilidad de dependencias WIP siguen pendientes. No se ejecutó batería
completa, ni se modificaron LLVM/Clang, interoperabilidad C/C++ o instalaciones.

## Verificación en la copia principal

El compilador de la copia principal se construyó incrementalmente con la
selección y otros cambios locales. `lambda-owned-final.log` registra 48
análisis con los resultados esperados y 48 ejecuciones seguras O0/O2. La
segunda invocación directa se rechaza por FnOnce; la vía por alias se rechaza
por el préstamo activo. `delegate-ref-out-integrated.log` registra 72
análisis y 48 ejecuciones seguras en las cuatro rutas de llamada. Los comandos
se conservan junto a esos logs en
`A:/nsc-fixes-evidencia/shared-tuple-call-commit-20261111/`. Los casos
potencialmente inseguros no se ejecutaron.

Los resultados corresponden al árbol WIP y no acreditan un compilador limpio.
Permanecen abiertos los orígenes desconocidos, algunas vistas compuestas y la
metadata histórica. Una lambda que usa dos veces el mismo dueño capturado por
move presenta un rechazo adicional que se investiga por separado.
