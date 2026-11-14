# T09: agregado fijo y tamaño oculto en una llamada SysV no variádica

El plan observado de Clang conserva los argumentos lógicos de la función
fija y registra la posición de cada `size_t` oculto. NSC valida ese plan antes
de construir la declaración y la llamada físicas. El catálogo actual conserva
el plan y lo verifica contra los atributos de la declaración al recuperarlo;
no se introducen versiones de NSMeta. Los planes con tamaños ocultos no se
admiten como planes de punteros a función.

## Validación

- Compilación incremental; prueba del plan de Clang con un puntero, tamaño
  oculto, agregado de dos piezas y entero posterior. La prueba verifica las
  posiciones físicas, la codificación y el rechazo de planes alterados.
- Prueba de catálogo: conservación del plan y rechazo de una discrepancia
  entre atributo y plan, o de un ancho físico inválido.
- `pruebas-tanda-09-cpp-variadica-sysv-record/validar-fija-tamano.py`:
  función C++ **no variádica** con puntero y tamaño oculto, agregado fijo mixto
  y entero. Ocho comparaciones C++/NativeSharp y dieciséis consumidores
  precompilados con y sin encabezado, estática y DLL O0–O3; verifica el orden
  de la firma física. Se probaron los modos 0–3 estáticos y dinámicos
  para un objeto local; cada combinación pasa las mismas configuraciones.
- `pruebas-tanda-09-c-sysv-variadica-tamano/validar-fija-tamano.py`: el mismo
  caso en C, con ocho controles, ocho llamadas directas y dieciséis
  consumidores precompilados en las mismas configuraciones.
- `validar-fija-tamano.py --dos-punteros`: dos tamaños ocultos separados
  por el agregado, con el primero estático o dinámico en modo 3; la opción
  `--estrechos` añade `signed char` y `bool` después del segundo tamaño y
  comprueba `signext` y `zeroext` en declaración y llamada. Cada variante pasa
  ocho comparaciones directas y dieciséis consumidores precompilados O0–O3.
  Evidencia: `A:/temp/t09-fixed-size-two.log`,
  `A:/temp/t09-fixed-size-two-dynamic.log` y `A:/temp/t09-fixed-size-narrow2.log`.
- `validar-fija-retorno-tamano.py` y `validar-fija-sret-tamano.py`: retorno
  C++ mixto en registros y retorno indirecto de 24 bytes, respectivamente,
  con puntero de tamaño oculto y agregado fijo de entrada. Cada caso pasa ocho
  comparaciones directas y dieciséis consumidores precompilados, en estática
  y DLL O0–O3. Evidencia: `A:/temp/t09-fixed-return-final.log` y
  `A:/temp/t09-fixed-sret-final.log`.
- `validar-fija-tamano-presion.py`: cuatro enteros, puntero y tamaño oculto
  agotan los registros; un agregado fijo de dos enteros de 64 bits pasa por
  pila. Ocho comparaciones directas y dieciséis consumidores precompilados,
  estática y DLL O0–O3, con `byval` verificado en declaración y llamada.
  Evidencia: `A:/temp/t09-fixed-size-pressure-final.log`.
- Regresiones dirigidas de tamaños escalares C y C++, retornos variádicos C y
  C++, transporte MIR e identidad ABI. Registros y evidencia en
  `A:/temp/t09-fixed-size-regression`, `A:/temp/t09-fixed-size-modes-final` y
  `A:/temp/t09-fixed-last-check`.

## Límites

Se probaron los modos 0–3 estáticos y dinámicos en C++ y el modo 0 estático
en C, con un agregado mixto y retorno entero; C++ también acredita los dos
retornos agregados descritos, dos tamaños ocultos y un agregado fijo en pila bajo presión de
registros. Otras formas de agregados,
expresiones, convenciones y métodos no se acreditan con estas pruebas.
T09-003 permanece abierto.
