# Tanda de Validación 37: rutas y recibos de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA SIN CAMBIO FUNCIONAL

## Frontera auditada

Se comprobó que las tres raíces configurables con capacidad de escritura
(`build_root`, `compiler_build_root` e `install_prefix`) rechazan cualquier ruta
en `C:`. El almacenamiento propio continúa fijado bajo
`A:\nsc-devel\_nscbuilder3`.

## Recibos

`file_receipt` conserva la ruta resuelta, tamaño y SHA-256 del archivo exacto.
`compiler_receipt` conserva, en orden, las identidades exactas del NSC y NSLink
seleccionados. Las pruebas emplean archivos sintéticos temporales sólo en `A:`.

## Evidencia

- Raíces escribibles rechazadas en `C:`: `3/3`.
- Límites de paralelismo rechazados: `0` y `257`.
- Recibo individual cotejado por contenido: `1/1`.
- Recibo NSC/NSLink cotejado por selección: `2/2` herramientas.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `111/111` PASS.

No se abrió la GUI, no se descubrieron herramientas reales y no se configuró,
compiló o ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
