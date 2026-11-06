# Prueba preparada de arreglo C empaquetado de punteros

El fixture prepara un arreglo `int*[2]` dentro de `pack(1)`: NativeSharp
escribe un slot, modifica el objeto apuntado y C comprueba el valor. El IR debe
cargar y guardar el slot con alineación 1; un caso negativo intenta exponer su
dirección como `int**`. La prueba y la validación agrupada están pendientes.
