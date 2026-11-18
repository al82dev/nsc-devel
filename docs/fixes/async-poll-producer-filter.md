# Filtro de error de poll en productor binario

El bridge de poll usa un filtro SEH tipado para no capturar objetos NativeSharp
incompatibles ni excepciones extranjeras. Antes de este cambio, los filtros se
generaban sólo a partir de catches presentes en MIR: un productor de future
sin catch local podía carecer del filtro aunque declarara un error de clase.
En ese caso el bridge conserva el unwind previo, por lo que el defecto queda
abierto para el consumidor binario.

El cambio integrado como trabajo en curso agrega a los filtros los tipos de error de los polls con cuerpo y
tabla de descriptor existente, sin duplicar filtros ni convertir en error de
compilación un tipo no resoluble. Un tipo sin ordinal, sin descriptor o que no
sea referencia de clase conserva el fallback. La selección usa el mismo sufijo
de poll que el lowering de corrutinas. Este cambio no modifica el manejo de
cleanups activos ni acredita compatibilidad de metadata o excepciones ajenas.

`tests/integration/async_poll_external_consumer.py` prepara un productor binario
sin catch y un consumidor separado que captura el error en O0/O2; aún no se
ha compilado ni ejecutado. También queda pendiente verificar particiones,
errores importados, genéricos y la identidad del filtro en bibliotecas
separadas. No se ejecutaron pruebas ni compilaciones para este cambio.
