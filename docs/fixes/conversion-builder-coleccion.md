# Conversión implícita en un builder de colección

Una colección `Bucket result = [alias]` con `Add(int)` y un operador implícito
de la clase de `alias` a `int` superaba el análisis semántico, pero fallaba la
verificación LLVM: se pasaba el puntero de la clase directamente al parámetro
entero de `Add`. El control `Bucket result = [42]` ejecutaba 42 en O0/O2. El
diagnóstico previo está en
`A:/nsc-fixes-evidencia/collection-builder-implicit-before.log`.

La generación HIR de la llamada sintetizada a `Add` aplica ahora la conversión
implícita existente al elemento antes de pasarlo como argumento. Conserva el
orden de los elementos y no cambia el análisis que rechaza una vista prestada de
solo lectura como entrada de un builder sin contrato de lectura.

Con el compilador aislado build `363ce17d2ed8b4c6`, la reproducción con objeto
mutable, la conversión numérica `short` a `int`, el control de tipo exacto y
un control del orden de dos conversiones y dos llamadas `Add` ejecutaron 42 en
O0 y O2. El caso de vista prestada siguió rechazándose en análisis semántico.
Los registros están en `A:/nsc-fixes-evidencia/shared-implicit-collection_builder*.log` y
`shared-implicit-implicit_readonly_collection_builder_probe.log`. Un consumidor
del SDK binario aislado ejecutó la conversión en O0/O2 con salida 42
(`shared-binary-sdk43-check.log`). Las pruebas no cubren parámetros `in` ni
excepciones producidas por el operador de conversión.

Durante el trabajo había cambios simultáneos sin registrar en CMake, matemática
y MIR de arreglos decimales. Se detectó además una edición concurrente de
`OopLlvmCalls.cpp` para parámetros `in`; las pruebas de esta tanda usan un
parámetro por valor y el estado exacto de compilación concurrente no quedó
aislado. El commit excluye esos archivos y la validación debe repetirse sobre la
revisión integrada.
