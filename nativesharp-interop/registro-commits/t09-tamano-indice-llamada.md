# T09: llamadas dentro de índices para tamaños ocultos

NSC rechazaba una expresión como `&arreglo[obtenerIndice() + 0]` cuando la
función importada recibe un tamaño oculto. El backend ahora distingue la
aritmética pura de los árboles que contienen una llamada directa. En la
consulta dinámica con efectos utiliza el valor desconocido que obtiene Clang,
sin volver a ejecutar la llamada; para las consultas estáticas conserva el
cálculo sobre el puntero ya evaluado.

## Validación

- Construcción incremental de `nsc` y verificación del runtime del host.
- C y C++: comparación con Clang para los modos estáticos y dinámicos en
  O0–O3, enlace estático y DLL. Un contador comprueba que cada llamada del
  índice se ejecuta exactamente una vez. En C++ se contrastaron suma y
  expresión anidada en modos 1 y 3; los ocho modos se verificaron además
  desde plantilla precompilada. En C se verificaron los ocho modos directa
  y precompiladamente. Dieciséis consumidores precompilados por lenguaje y
  convención, con y sin encabezado.
- Convención predeterminada antes del último ajuste conservador: C
  `A:/temp/nsc-t09-c-size-index-call-4fmi5_oe`; C++
  `A:/temp/nsc-t09-size-index-call-ck09dy1w`.
- SysV sobre el compilador final: C `A:/temp/nsc-t09-c-size-index-call-ayzuqe3p`;
  C++ `A:/temp/nsc-t09-size-index-call-u7gqxlj2`.
- Regresión de aritmética sin efectos: `A:/temp/nsc-t09-c-size-index-expression-aklsx_6g`
  antes del último ajuste, que solo restringió lecturas volátiles anidadas.

## Límites

La prueba cubre llamadas directas a funciones libres y las expresiones
indicadas en Windows x64. Las lecturas volátiles anidadas y otras formas de
efectos, expresiones y convenciones siguen pendientes. T09-003 no se cierra.
