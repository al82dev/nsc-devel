# Tanda de Validación 51: descubrimiento tipado de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

`first_existing` aceptaba cualquier ruta existente. Tanto los ejecutables como
`LLVM_DIR` compartían ese selector, de modo que un directorio con nombre de
herramienta o un archivo usado como directorio podían ser elegidos durante la
autodetección.

## Corrección mínima

El selector se separó en `first_existing_file` y
`first_existing_directory`. Todos los consumidores conservaron exactamente sus
candidatos y prioridades, pero ahora verifican el tipo correspondiente:
ejecutables y archivador como archivos; `LLVM_DIR` como directorio.

## Evidencia

- Directorio antes de un archivo: se selecciona el archivo.
- Archivo antes de un directorio: se selecciona el directorio.
- Conjunto sin candidato del tipo pedido: devuelve vacío.
- No quedan llamadas al selector ambiguo anterior.
- Pruebas dirigidas de rutas: `10/10` PASS.
- Suite Python completa del controlador: `161/161` PASS.

No se descubrió ni ejecutó ninguna herramienta real, no se abrió la GUI y no se
configuró, compiló o ejecutó producto. No hubo baseline, despliegue ni escritura
en `C:`.
