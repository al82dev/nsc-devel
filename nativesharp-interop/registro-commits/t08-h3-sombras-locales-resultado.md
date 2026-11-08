# T08-002: reproducción de captura de nombres C sombreados

El diagnóstico `pruebas-tanda-08-sombras-plantilla-c/diagnosticar.cmd`, con
NSC build `5c1e6c7fa15d8a3e`, terminó con código 1. En O0–O3 el consumidor
compiló y obtuvo 30 a partir de dos variables locales, pero el inspector del
catálogo confirmó que la global C `shadow_value` fue publicada y `llvm-nm`
encontró un símbolo `__nsc_c_inline_` en la biblioteca para la función C que
también está sombreada.

El recolector registra candidatos sintácticos y el observador conserva las
demandas sin resolver el sombreado (`ProjectCompilationSnapshots.cpp`); los
selectores de funciones y globales las consumen como candidatas. La solución
debe respetar el orden y alcance de declaraciones, parámetros, bloques y
otras formas de sombra, sin descartar por nombre una referencia C válida.
El defecto permanece abierto; no se modificó producción ni se acreditan
otras formas de sombreado a partir de este fixture.
