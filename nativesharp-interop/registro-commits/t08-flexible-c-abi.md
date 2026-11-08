# T08-010: ABI Win64 de structs con miembro flexible C

## Causa y corrección

La clasificación MIR usaba solo el tamaño del record: los tamaños 1, 2, 4 y
8 se pasaban directamente. Clang 23.1.0 usa paso indirecto para los structs C
de la reproducción con miembro flexible, tanto para retorno como para argumento.
La discrepancia producía violaciones de acceso. Ahora la propiedad del miembro
flexible se conserva desde Clang o se reconstruye de los campos ya existentes
del catálogo C y se utiliza para clasificar el ABI. No cambia el formato de
metadata ni la clasificación de structs sin ese miembro.

## Evidencia

- Construcción incremental de `nsc`: código 0.
- `pruebas-tanda-08-flexible-c/validar.cmd`: código 0. El control exige que
  argumento y retorno por valor de los structs natural y empaquetado pasen
  O0–O3; el consumidor de biblioteca precompilada comprueba argumento y
  retorno del struct natural con y sin encabezado. Antes ambas sondas de
  retorno terminaban con `-1073741819`.
- Referencia Clang: ambos argumentos, natural y empaquetado, se emiten como
  punteros en `A:/nsc-interop-build/flex-abi-probe.ll`.
- Regresiones agrupadas `_Bool`, layout-clang, layout, record-c y tres
  controles de bitfields volatile: código 0, `VALIDACION_CALL_OK`.
- El control se añadió al gate de tanda. No se ejecutó el gate completo.

El miembro flexible `const` y el desalineado siguen sin proyectarse; la
conversión implícita a puntero sigue rechazada. El retorno empaquetado se probó
directamente, pero no desde una biblioteca precompilada. No se acredita soporte de
otras variantes ni se cierra T08-010 o la tanda 8 con estos casos.
