# Tanda de Validación 57: catálogos de disposición de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Builder 3 barría todo JSONL bajo la suite y omitía silenciosamente una fila si
no reconocía su identidad. Un rango descendente se expandía a cero casos y una
lista vacía, duplicada o mal tipada también podía quedar sin efecto. La
disposición faltante se infería por fragmentos amplios de ruta y no se cerraba
el resultado contra los IDs del diseño.

## Corrección mínima

`parse_disposition_row` exige una sola identidad canónica y una disposición
soportada. Sólo `SelfHostBootstrapInfrastructureCatalog.jsonl` puede inferir
`InfrastructurePending`; cuatro archivos de metadatos ya inventariados pueden
carecer de identidad. Los rangos deben compartir prefijo y ancho y ser
ascendentes. La carga final rechaza cualquier ID ajeno al diseño y conserva la
precedencia de una clasificación específica sobre `Executable`.

## Evidencia

- `15` catálogos, `669` filas y `1244` identidades vivas: consumidas.
- Cuatro filas documentales: omitidas sólo por nombre explícito.
- Cinco formas de identidad inválida: rechazadas.
- Cuatro rangos inválidos: rechazados.
- Disposición desconocida o inferencia fuera del catálogo autorizado:
  rechazadas.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `183/183` PASS.

No se modificó ningún catálogo ni la precedencia de clasificación. No se abrió
la GUI y no se configuró, compiló o ejecutó producto. No hubo baseline,
despliegue ni escritura en `C:`.
