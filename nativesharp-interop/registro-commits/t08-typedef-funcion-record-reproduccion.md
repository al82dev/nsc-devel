# Typedefs de función con parámetros record C

La prueba crea dos productores precompilados. Ambos definen un typedef
`Callback` a función variádica que recibe `CRecord*`; una plantilla pública de
cada productor declara una variable de ese tipo. En O0, el consumidor rechaza el
typedef aun cuando ambos records tienen el mismo campo `int`. Una variante cambia
el campo de uno de los records a `long long`; también se rechaza con el mismo
diagnóstico de conflicto de alias.

Se ejecutaron `validar.py 0 --expect-current` y `validar.py 0 --incompatible`.
Ambos controles verifican el diagnóstico y que no se publique el ejecutable.
Son controles de diagnóstico: el primero no representa el comportamiento final
deseado y no cierra la capacidad. El caso equivalente debe acabar compilando y
el incompatible debe rechazarse por evidencia suficiente, no por el mismo filtro
general que rechaza ambos.

El código de conciliación consultado compara el cierre de records vivos con
catálogos precompilados. En este caso hay dos productores precompilados; queda
por comprobar cómo obtener y comparar sus definiciones completas antes de
permitir la conciliación. El nombre, el USR o el tamaño por sí solos no prueban
que dos records sean compatibles. No se modificó el compilador ni NSMETA.
