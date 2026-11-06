# T08-010: dirección de arreglo dentro de record subalineado

Se prepara una declaración C con un record que contiene un arreglo dentro de
otro record empaquetado, y un caso NativeSharp que intenta tomar la dirección
de un elemento. El caso permite verificar si se pierde la restricción heredada
del record exterior. Aún no se ha ejecutado ni demuestra un defecto.
