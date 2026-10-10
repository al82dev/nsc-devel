# Tanda de Validación 46: envolvente de gates Python de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados

`_run_python_gate_case` convertía `satisfied` mediante `bool(...)` y `detail`
mediante `str(...)`. Una cadena como `"false"` se convertía en verdadero y,
para un defecto conocido, podía publicar `UNEXPECTED_PASS`. Además, una
excepción del gate escapaba hasta el despacho general y perdía el directorio
del caso como evidencia.

## Corrección mínima

`parse_python_gate_result` acepta exclusivamente la tupla contractual
`(bool, str)`. `_run_python_gate_case` conserva los logs y el directorio del
caso tanto para formas inválidas como para excepciones, y clasifica ambos casos
como `INFRASTRUCTURE_ERROR` antes de evaluar el resultado esperado del producto.

## Evidencia

- Retornos válidos verdadero/falso: preservados sin conversión.
- Lista, cadena, nulo y tuplas con cardinalidad incorrecta: rechazados.
- Entero o cadena usados como satisfacción: rechazados.
- Detalle nulo o contenedor: rechazado.
- Excepción del gate: error de infraestructura con evidencia conservada.
- Retorno malformado de un defecto conocido: no se convierte en
  `UNEXPECTED_PASS`.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `144/144` PASS.

No se ejecutaron gates reales, no se abrió la GUI y no se configuró, compiló o
ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
