# Tanda de Validación 56: proyección ejecutable tipada de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Builder 3 validaba el manifiesto, pero descartaba sus objetos tipados y volvía
a formar `CaseRecord` mezclando la asignación y el JSON crudo. La auditoría
dirigida encontró `86` casos ejecutables cuya capa mostrada no coincidía con el
`CaseManifest` que realmente gobierna su ejecución.

## Corrección mínima

`CanonicalCatalog.load` conserva ahora los `CaseManifest` validados y usa esa
única autoridad para capa, perfil, modos, optimizaciones, ejecutor y resultado
esperado de cada caso ejecutable. El JSON crudo queda sólo como carga de
`inputs` y `oracle`. Las filas no ejecutables continúan proyectándose desde la
asignación ya validada.

## Evidencia

- Los `5695` casos ejecutables coinciden campo por campo con su manifiesto
  validado.
- Las `86` divergencias de capa de la proyección anterior quedaron eliminadas.
- La invocación de la autoridad precede a la proyección del catálogo.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `178/178` PASS.

No se modificó el manifiesto, la asignación, la selección ni la matriz de
variantes. No se abrió la GUI y no se configuró, compiló o ejecutó producto. No
hubo baseline, despliegue ni escritura en `C:`.
