# Conversión decimal a entero con coeficiente amplio

El control de un valor decimal escalado equivalente a `18446744073709551615`
fallaba al convertirse a `ulong` en O0 y O2, aunque el valor sin escalar y la
igualdad numérica pasaban. La conversión descartaba los 32 bits altos del
coeficiente y construía `10^escala` en 64 bits.

La conversión conserva el coeficiente de 96 bits y divide por diez una vez por
cada dígito de escala, procesando tres segmentos de 32 bits mediante divisiones
de 64 bits. Conserva el truncamiento hacia cero y aplica el signo al resultado;
no cambia la política previa para valores fuera del rango de destino. La ruta
de conversión a flotante y otras operaciones decimales no se modificaron.

La compilación incremental pasó. Cinco proyectos de control, incluida la
reproducción, signos, escalas, `i128`/`u128`, factura e igualdad, compilaron y
ejecutaron con salida 0 en O0 y O2 (10/10). La prueba registrada
`nsc.compiler.decimal-to-integer-wide` pasó 1/1. Comandos y resultados:
`A:/nsc-fixes-evidencia/decimal-int-integrated-20261111.log` y
`A:/nsc-fixes-evidencia/decimal-int-wide-ctest-20261111.log`. El compilador y
runtime eran de A; NSLink se invocó por nombre como dependencia externa de
solo lectura. El árbol contenía otros cambios locales y no certifica una revisión
limpia. El header interno también es compartido con la rama de interoperabilidad.
