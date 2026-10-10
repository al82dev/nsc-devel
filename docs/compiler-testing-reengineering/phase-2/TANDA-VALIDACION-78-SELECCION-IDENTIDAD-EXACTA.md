# Tanda de Validacion 78: seleccion de identidad exacta

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

Los filtros de caso, contrato, componente y capa operaban como uniones. Si una
solicitud mezclaba una identidad valida con otra inexistente, el selector
conservaba la valida, ignoraba la desconocida y podia cerrar la corrida como
exitosa sin haber ejecutado todo lo pedido.

## Correccion

- se deriva el dominio real de casos, contratos, componentes y capas del
  universo recibido;
- cada valor solicitado debe pertenecer completamente a ese dominio;
- el rechazo ocurre antes de aplicar filtros y antes de crear workspaces;
- la interseccion entre clases de filtro y el cierre de dependencias no cambian.

## Evidencia ejecutada

- pruebas dirigidas del selector: `10/10` PASS;
- suite Python completa del controlador: `218/218` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra que los cuatro filtros de union no omiten valores
desconocidos. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
