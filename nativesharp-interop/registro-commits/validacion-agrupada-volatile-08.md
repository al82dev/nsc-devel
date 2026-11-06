# Validación agrupada de casos volatile de la tanda 08

## Construcción y pruebas

Con los cambios de código hasta `eb0fee6c9` se ejecutó construcción
incremental de los targets `nsc` y `nsc_cpp_session_tests` en
`A:/nsc-interop-build/tanda1-ON`, con `--parallel 4`. Ninja programó 217
pasos afectados y terminó con código 0. El enlace de `nsc.exe` pasó su
comprobación de runtime estático; hubo advertencias de inicializadores
parciales en archivos existentes.

`ctest --test-dir A:/nsc-interop-build/tanda1-ON -R nsc.cpp-interop.session
--output-on-failure` pasó 1/1, incluidos los controles de inventario,
procedencia y alineación incorporados recientemente.

`pruebas-tanda-08-record-c/validar.cmd` se detuvo inicialmente en un control
antiguo que exigía `store volatile` sin orden para la escritura de 61. El IR
generado tenía `store atomic volatile ... release`; el commit `01ad0cefe`
ajustó ese control sin retirarlo. La repetición terminó con código 0 y el
marcador `Record C por puntero: resultado esperado cero`. El script compiló y
ejecutó el proyecto en O0, O1, O2 y O3, comprobó el IR O0 y ejecutó sus casos
negativos O0.

## Alcance

En el IR O0 comprobado, el arreglo global `volatile int` y su matriz usan
acquire para lectura y release para escritura; la asignación compuesta usa
ambos. La desreferencia directa del global C `volatile int*` usa los órdenes
correspondientes. El fixture también pasó sus casos preparados de arreglos de
punteros y globales `volatile void*`.

Esto no cierra T08-015 ni la tanda. La lectura y escritura mediante alias
locales y arreglos de punteros todavía carecen del transporte del modo MS;
el IR generado aún muestra accesos volatile sin el orden observado en la
referencia Clang. Tampoco se validaron todas las combinaciones de C.
