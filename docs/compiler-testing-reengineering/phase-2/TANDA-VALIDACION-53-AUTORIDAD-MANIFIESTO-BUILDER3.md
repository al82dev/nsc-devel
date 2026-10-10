# Tanda de Validación 53: autoridad canónica del manifiesto en Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

`CanonicalCatalog.load` leía `cases.jsonl` directamente y convertía con
`str(...)` cuatro campos que el controlador de consola ya valida de forma
estricta. Builder 3 podía aceptar temporalmente un `case_id`, `executor`,
`expected_outcome` u optimización de tipo incorrecto y producir un diagnóstico
tardío o distinto al de la autoridad canónica.

## Corrección mínima

Builder 3 valida ahora el manifiesto completo con `InventoryIndex` y
`load_manifest` del controlador antes de construir su proyección gráfica. Se
eliminaron las conversiones de esos cuatro campos; el catálogo sólo proyecta
valores cuya forma, enumeraciones, propiedad de contrato y grafo de
dependencias ya fueron aceptados por el cargador canónico.

## Evidencia

- Manifiesto vivo: aceptado por la autoridad canónica.
- `case_id` numérico: rechazado.
- `executor` numérico: rechazado.
- `expected_outcome` booleano: rechazado.
- optimización numérica: rechazada.
- El catálogo invoca la autoridad antes de leer su proyección.
- Pruebas dirigidas: `3/3` PASS.
- Suite Python completa del controlador: `168/168` PASS.

No se modificó el manifiesto, los inventarios, la selección ni el producto. No
se abrió la GUI y no se configuró, compiló o ejecutó producto. No hubo baseline,
despliegue ni escritura en `C:`.
