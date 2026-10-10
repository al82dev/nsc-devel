# Tanda de Validación 54: esquema CSV del catálogo de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Builder 3 consumía los CSV de olas, diseño y asignación mediante `DictReader`
sin comprobar sus encabezados ni la cardinalidad de cada fila. Un encabezado
duplicado podía sobrescribir una columna, una celda adicional quedaba bajo la
clave `None`, un campo vacío avanzaba hasta la proyección y `int(...)` aceptaba
formas de ola no canónicas.

## Corrección mínima

`read_catalog_csv` exige los campos que Builder 3 realmente consume, rechaza
encabezados vacíos o duplicados, celdas adicionales y valores requeridos vacíos.
Permite columnas adicionales con nombre para no congelar la evolución del
inventario. `parse_wave_number` acepta únicamente un decimal positivo canónico.

## Evidencia

- Los tres CSV vivos satisfacen su esquema consumido.
- Encabezado requerido ausente: rechazado.
- Encabezado duplicado: rechazado.
- Celda adicional: rechazada en su fila.
- Valor requerido vacío: rechazado en su fila.
- Columna futura con nombre: preservada.
- Ocho formas no canónicas de ola: rechazadas.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `173/173` PASS.

No se modificaron los CSV ni sus dominios semánticos. No se abrió la GUI y no
se configuró, compiló o ejecutó producto. No hubo baseline, despliegue ni
escritura en `C:`.
