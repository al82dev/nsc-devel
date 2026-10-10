# Tanda de Validacion 77: protocolo minimo de identidad

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

La operacion `distinct-build-identities` comparaba campos obtenidos mediante
`dict.get`. Si la respuesta primaria incluia solamente `NSCOMPILER 1`, la
ausencia de `build` y `compiler_version` era distinta de los valores del
compilador de comparacion y podia acreditar falsamente ambos predicados.

## Correccion

- toda identidad exige `compiler_version`, `compiler_release`, `language`,
  `stage` y `build`;
- cada campo obligatorio debe contener texto no blanco;
- la validacion ocurre inmediatamente despues de leer `NSCOMPILER 1` y antes
  de cualquier operacion o comparacion;
- una respuesta incompleta queda como fallo de contrato del producto.

## Evidencia ejecutada

- pruebas dirigidas del ejecutor de identidad: `5/5` PASS;
- suite Python completa del controlador: `217/217` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra que una identidad ausente o vacia no puede acreditar
diferencias entre compiladores. No acredita la identidad de binarios reales ni
una corrida funcional. La meta permanece activa hasta cero errores inesperados
en la matriz acordada.
