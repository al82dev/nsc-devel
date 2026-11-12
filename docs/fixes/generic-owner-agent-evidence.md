# Métodos genéricos de dueños genéricos: evidencia del agente 2

Base de la rama: `fc06f5a38`, descendiente de `a56498f05`. Los cambios de
implementación y las seis pruebas se copiaron selectivamente del trabajo
local de `A:/nsc-devel-fixes`; los archivos y hashes copiados están en
`A:/nsc-fixes-agente-2-evidencia/generic-copied.json`. No se copiaron los
cambios ajenos de operadores o préstamos en los otros archivos del driver.
El historial detallado de los ensayos previos está en el archivo local de la
campaña `A:/nsc-devel-fixes/docs/fixes/generic-owner-method.md`.

El `FixedMatrix.ns` reproducido coincide byte a byte con el del informe.
Un compilador experimental con `TemplateCollector.cpp` de la base rechazó
la declaración `Multiply<const int OtherColumns>` dentro de `FixedMatrix<T,
Rows, Columns>`; diagnóstico y comando en `generic-baseline/original-check.json`.

El ensayo conserva ambas dimensiones de especialización, identifica el dueño
cerrado al seleccionar y materializar el método, mantiene sus sustituciones
y restricciones, y reconstruye la superficie del dueño genérico al leer una
biblioteca. También preserva identidades de tipos genéricos anidados y el
ámbito de declaración necesario en los casos de receptor cubiertos. La
evidencia no demuestra todas las variantes ni toda la metadata histórica.

Para la comparación se recompilaron solo los diez módulos cambiados de esta
rama y se enlazaron delante de copias de bibliotecas y objetos de entrada del
build de la campaña. Los demás objetos y headers proceden del árbol principal
con trabajo sin registrar. Los comandos y hashes de bibliotecas están en
`A:/nsc-fixes-agente-2-evidencia/generic-prototype/build.log`. Es un
compilador experimental híbrido, no un build limpio de la rama.

Con ese ejecutable la reproducción original pasó análisis, compiló O0/O2 y
devolvió 6 en ambas ejecuciones (`generic-prototype/original-check.json`).
El `Main` original solo llama a `CellCount`, no a `Multiply`.
La regresión conservada de la campaña invocó `Multiply` con la fuente
original y devolvió 42 en O0/O2; también pasó controles de varios dueños y
constantes anidadas y rechazó argumentos constantes incorrectos. El caso de
tipo inexistente se rechazó durante parseo y no acredita su comprobación
semántica. Comando y salida: `generic-prototype/original-matrix-regression.json`.
Se agregó además `generic_owner_matrix.py`, que ejecuta O0/O2 la misma
llamada con una copia byte por byte de la matriz original. Pasó con el
ejecutable híbrido (`generic-prototype/generic_owner_matrix.log`).

Pasaron las seis pruebas `generic_owner_method_inference`,
`generic_owner_method_binary`, `generic_owner_method_producer_trait`,
`generic_owner_field_split`, `generic_owner_field_project_ref` y
`generic_owner_nested_alias_binary`. Las pruebas ejecutan llamadas reales,
controles de rechazo y, según el caso, productores estáticos y DLL con
consumidores O0/O2. Cada comando, diagnóstico y resultado está en su log bajo
`A:/nsc-fixes-agente-2-evidencia/generic-prototype/`. También pasaron
`dependent_generic_constraints`, `generic_type_declaration_import_binary`,
`conditional_unsafe_thread_traits`, `generic_library_alias_isolation`,
`generic_internal_owner_metadata` y `binary_struct_trait_conformance`.

El runtime de los ejemplos provino de la compilación aislada de la campaña
en A. Las regresiones se ejecutaron directamente, no mediante el CTest recién
registrado. Quedan por comprobar con el compilador integrado de la rama
principal las mismas pruebas y sus límites: metadata antigua, modificadores
genéricos que no cubren los fixtures y otras formas de receptor. El código
provisional no justifica declarar resuelto el caso general sin esa validación.

Tras integrar el cambio en `8046d2223`, la compilación incremental de `nsc`
terminó con código 0 y la selección registrada de siete pruebas pasó 7/7 sin
omisiones, incluida la matriz que invoca `Multiply`. Comando y resultados:
`A:/nsc-fixes-evidencia/generic-owner-integrated-20261111.cmd` y `.log`.
El compilador incorpora otros cambios locales, por lo que este resultado no
demuestra un árbol limpio ni las variantes de metadata y receptores no cubiertas.
