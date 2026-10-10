# Tanda de Validacion 66: oraculo self-host sin coercion

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

Basic, Diagnostics, Lexer, Source y Types convertian
`oracle.exit_code` mediante `int(...)`. Una cadena, un flotante o un booleano
podian acreditar el mismo codigo de salida que un entero valido y ocultar un
manifiesto corrupto.

## Correccion

- `selfhost_inventory.required_oracle_exit_code` exige un objeto de oraculo;
- el campo debe existir y ser un `int` exacto;
- los cinco gates consumen esa autoridad y ya no convierten el valor;
- un tipo invalido se clasifica como error de infraestructura antes de evaluar
  el resultado del producto.

## Evidencia ejecutada

- pruebas dirigidas del contrato y evaluadores: `3/3` PASS;
- suite Python completa del controlador: `207/207` PASS;
- consumidores self-host cerrados: `5/5`;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra el tipo exacto del exit code esperado en los cinco
gates. No acredita la compilacion ni ejecucion de sus fixtures. La meta sigue
activa hasta cero errores inesperados en la matriz funcional acordada.
