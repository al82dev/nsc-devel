# Tanda de Validacion 109: lector CSV generico de Builder 3

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`nscbuilder3.read_csv`, usado por las auditorias de alcance vivo, conservaba
una ruta permisiva distinta de `read_catalog_csv`. Un encabezado duplicado,
fila truncada o celda extra podia alterar silenciosamente los componentes
incluidos y excluidos por la auditoria.

## Correccion

- el alias generico delega en `read_catalog_csv` sin imponer campos requeridos;
- conserva columnas adicionales con nombre;
- rechaza la misma estructura ambigua que las cargas funcionales;
- catalogos, dominios y seleccion permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas de esquema CSV y alcance: `8/8` PASS;
- suite Python completa del controlador: `243/243` PASS;
- un encabezado `component,component` es rechazado por el alias generico;
- las auditorias cargaron los catalogos vivos y conservaron su alcance;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra estructura CSV no ambigua en el lector generico de
Builder 3. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
