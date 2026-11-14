# T09: índices aritméticos para tamaños ocultos

NSC rechazaba `&arreglo[indice + 0]` en parámetros con tamaño de objeto
cuando el índice no era una constante, una variable aislada o una llamada
directa. El backend ahora reconoce árboles de aritmética sin efectos
laterales compuestos por constantes, locales y parámetros, y utiliza el
puntero ya evaluado para calcular el tamaño. Las formas que no se puedan
clasificar siguen rechazándose; no se vuelve a evaluar el índice.

## Validación

- Construcción incremental de `nsc` y comprobación del runtime del host.
- Prueba C++: suma, resta y expresión anidada en modos de subobjeto 1 y 3,
  estáticos y dinámicos, contrastadas individualmente con Clang O0–O3. Se
  probaron las convenciones predeterminada y SysV, bibliotecas estáticas y
  DLL, y dieciséis consumidores precompilados por convención que usan suma.
  Evidencia: `A:/temp/nsc-t09-size-index-expression-sczsk10j` y
  `A:/temp/nsc-t09-size-index-expression-ah4sp_sz`.
- Prueba C: suma en los mismos modos, convenciones y configuraciones, con
  controles de Clang y dieciséis consumidores precompilados por convención.
  Evidencia: `A:/temp/nsc-t09-c-size-index-expression-vq5847vf` y
  `A:/temp/nsc-t09-c-size-index-expression-w9j4hb0h`.
- Regresión C++ de índices locales y llamadas como índice en los cuatro modos,
  con controles Clang O0–O3: `A:/temp/nsc-t09-object-size-gqph543k`.

## Límites

Los casos se probaron en Windows x64. No se afirma que las expresiones con
llamadas o efectos laterales anidados, otras formas MIR, reservas variables o
convenciones adicionales estén resueltas. T09-003 permanece abierto.
