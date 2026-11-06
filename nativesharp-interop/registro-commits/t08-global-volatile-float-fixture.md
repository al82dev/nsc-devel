# Prueba mixta de globales volatile flotantes

El proyecto de records prepara globales `volatile float` y `volatile double`:
NativeSharp escribe y lee ambos, C comprueba esos valores, los modifica y
NativeSharp comprueba el resultado. El script exige en el IR de cada lado
lecturas acquire y escrituras release con ancho y alineación correspondientes.
La prueba usa la batería y optimizaciones existentes; queda pendiente de
ejecución y no demuestra otras formas de acceso volatile.
