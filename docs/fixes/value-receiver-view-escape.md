# Vistas de receptores de estructura por valor

Un método podía devolver una vista obtenida de un campo de una estructura
recibida por valor. La vista apuntaba a la copia del parámetro, cuya vida
termina con la llamada. La reproducción anterior al cambio fue aceptada por
el análisis (`A:/nsc-fixes-evidencia/value-receiver-view-probe.log`).

El análisis reconoce la estructura por valor como raíz local y conserva la
protección previa para tipos move-only. La regresión dirigida
`nsc.compiler.value-receiver-view-escape` rechaza las fugas desde estructuras
ordinarias y move-only; el uso del receptor por `ref` devuelve 42 en O0 y O2
(`A:/nsc-fixes-evidencia/value-receiver-focused-test.log`).

El compilador usado incluye cambios locales adicionales, por lo que la prueba
no certifica un árbol limpio. Esta corrección no cubre por sí sola todos los
receptores temporales, las vistas dentro de tuplas ni el uso entre bibliotecas;
esos cambios siguen en curso por separado. No se ejecutó una batería completa.
