# Almacenamiento estático a través de bibliotecas precompiladas

La decisión de la campaña prohíbe almacenar `Rc` o `WeakRc` en estado estático
compartido, incluso cuando están anidados en otros tipos. La prueba
`nsc.sdk.shared-static-binary-reference` verifica el análisis de un consumidor
que usa una biblioteca NativeSharp precompilada y otra biblioteca que depende
de ella. Los tipos de puntero del fixture son declaraciones mínimas con el
namespace y los nombres de Core; no son el SDK público completo.

La prueba espera rechazar 14 formas, entre ellas campos públicos y privados,
contenedores genéricos, arreglos, punteros y un campo que cruza ambas bibliotecas.
Acepta como controles `WeakArc` y un `Rc` de otro namespace. La biblioteca se
emite con `--emit-project-oop-lib -O0` y los consumidores se comprueban con
`--check-project-method-bodies`; los comandos y diagnósticos individuales están
en `A:/nsc-fixes-evidencia/shared-static-binary-current.log`.

La selección CTest pasó 1/1 en el build aislado con el compilador del árbol de
trabajo (`A:/nsc-fixes-evidencia/shared-static-binary-ctest.log`). Esa ejecución
incluía otros cambios sin registrar, por lo que no demuestra por sí sola el
resultado de un build limpio del commit. La generación produjo automáticamente
encabezados de host en el directorio temporal de A; no se usaron para pruebas
de interoperabilidad. La regresión no verifica ejecución de los consumidores,
el SDK público binario, módulos cargados posteriormente ni todo el contrato de
concurrencia de las familias.
