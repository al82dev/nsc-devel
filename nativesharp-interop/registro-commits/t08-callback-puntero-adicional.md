# Tanda 08: puntero adicional a un typedef C de callback

- Caso: un typedef `Callback` representa un puntero C a función; `Callback*` debe representar un puntero a ese puntero de función cuando se usa desde NativeSharp.
- Cambio: la expansión del typedef acepta modificadores de puntero adicionales y conserva los rechazos de otros modificadores y de calificadores no representados. La firma estructural y los modificadores permanecen separados para el cálculo de identidad existente.
- Prueba preparada: el proyecto de callback existente pasa tanto `Callback` como `Callback*` a funciones C, que llaman al mismo callback. El caso de puntero adicional todavía requiere construcción y ejecución en la validación agrupada.
- Limitación: el cambio no demuestra arreglos de callbacks, calificadores generales, callbacks capturantes ni cobertura completa de T08-017.
