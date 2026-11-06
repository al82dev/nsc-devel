# Validación agrupada: escalares y `_Bool` volatile C

## Ejecución

Con los cambios hasta `ce869093f`, se construyeron incrementalmente los
targets `nsc` y `nsc_cpp_session_tests` en `A:/nsc-interop-build/tanda1-ON`
con `--parallel 4`: Ninja completó 43 pasos y el contrato de enlace estático
del runtime anfitrión pasó. Se observaron advertencias de inicializadores
parciales. El compilador resultante informó el build `bd1f1ac94128ecb4`.

La prueba `nsc.cpp-interop.session` pasó 1/1 mediante CTest. El script
`pruebas-tanda-08-record-c/validar.cmd` terminó con código 0 y el marcador
`Record C por puntero: resultado esperado cero`. Comprobó el IR generado en
O0 y compiló y ejecutó el proyecto mixto en O0, O1, O2 y O3; también comprobó
los diagnósticos negativos preparados en O0.

## Alcance observado

El IR `A:/nsc-interop-build/tanda08-record-c/project.ll` contiene las cargas
`acquire` y escrituras `release` volatile exigidas por el script para los
arreglos escalares cubiertos y para accesos directos a `volatile int*` y a
punteros obtenidos de arreglos C. El script comprueba que el slot del arreglo
de punteros no adquiere incorrectamente el calificador del objeto apuntado.

Para el arreglo global C `volatile _Bool`, la lectura y escritura acceden al
almacenamiento como `i8` con los órdenes observados; la conversión del valor
se hace hacia o desde el `i1` usado por NSC. Los logs
`volatile-bool-compound.log` y `native-bool-compound.log` registran que tanto
`volatile_bool_array[0] |= true` como `value |= true` se rechazan con
`compound assignment requires compatible numeric operands`. Estos rechazos no
demuestran soporte de la operación.

La comprobación es dirigida: no cierra T08-015 ni la tanda 08. Permanecen
pendientes el transporte del modo a través de alias locales, reasignaciones,
ramas, parámetros y retornos; otras combinaciones y la validación integral
de la tanda tampoco quedan demostradas por esta ejecución.
