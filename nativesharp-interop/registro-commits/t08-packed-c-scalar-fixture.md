# Prueba mixta de campo escalar C empaquetado

El fixture prepara un record C bajo `#pragma pack(1)` y compara tamaño,
alineación y offset con Clang. NativeSharp lee y modifica su campo `int`,
incluida asignación compuesta; C comprueba el valor. El verificador exige
alineación 1 en IR y un caso negativo comprueba que no escape `&campo`.
La prueba está preparada y aún no se ha ejecutado; no demuestra los demás
tipos de campos empaquetados ni todas las rutas de acceso.
