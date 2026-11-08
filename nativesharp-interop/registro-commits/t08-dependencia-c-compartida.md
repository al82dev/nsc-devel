# Dependencia C compartida por dos productores

Dos bibliotecas NativeSharp propagaban copias de la misma biblioteca C en
directorios diferentes. El recolector de NSC eliminaba duplicados por ruta,
por lo que enviaba ambas copias a NSLink y obtenía una definición duplicada.

El recolector compara el nombre y los bytes de las dependencias C ya recibidas
de productores anteriores. Una copia idéntica se enlaza una vez; el orden y las
repeticiones dentro de un mismo productor se conservan. También se evita repetir
una copia idéntica cuando aparece después como referencia C del proyecto.
No se modificó NSLink.

En O0 pasaron el caso compartido y otro con dos archivos `fixture.lib` de
contenido diferente que definen símbolos distintos; ambos se enlazaron y sus
estados permanecieron independientes. Pasaron además los controles existentes
de dos bibliotecas C, una global externa de plantilla y funciones inline.
El caso compartido se ejecutó dos veces para verificar que la prueba es repetible.
No se verificaron aquí otras optimizaciones ni dependencias dinámicas.
