# Enum C++ global: proyección parcial del tipo

La muestra `enum Mode : int { Chosen = 10, Changed = 11 };` no permitía
declarar `Mode value` desde NativeSharp. Clang entregaba el enum, pero el
inventario excluía explícitamente las declaraciones C++. Sin records, las
reproducciones de tipo y enumerador también fallaban por separado:
`A:/temp/t09-enum-solo/tipo.log` y `constante.log`.

Se proyecta el tipo global completo no scoped con identidad USR de Clang,
base y disposición verificadas. Su origen es distinto de `CEnum`, de modo que
las reglas numéricas específicas de C no se activan por este cambio. El
análisis de vida y la inicialización LLVM reconocen este enum como valor
escalar, sin exigirle construcción de objeto C++.

La prueba `pruebas-tanda-10-enum-cpp-global/validar.py` compara la conversión
explícita de `10` a `Mode`, la copia y la conversión de regreso con Clang
23.1.0 y ejecuta ambos programas en O0–O3. También ejecuta en O0 una regresión
equivalente de enum C. En O0 verifica además que sigan rechazándose los enums
scoped, con namespace o con base `unsigned int`. Resultado: `OK`, registros en
`A:/temp/nsc-t10-enum-cpp-global-oje_hap5`. La compilación incremental terminó
con código 0; pasaron el plan `nsc.cpp-interop.regcall-record-plan` y los
controles dirigidos de transporte, ABI e identidad (`transport 0`, `abi 0`,
`identity 0`).

No se acredita acceso a `Chosen`, enums dentro de namespaces o clases, enums
scoped, campos de record, llamadas ABI con esos campos, ni conservación y
consumo precompilado. Los ítems T09-003, T10-002 y T11-004 permanecen abiertos.
No se modificaron Clang ni NSLink ni se desplegó en C:.
