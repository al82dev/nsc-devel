# TANDA-VALIDACION-256: ejecución de pruebas sin timers automáticos

## SCOPE LOCK

- Se retiran los deadlines automáticos de los procesos que ejecutan pruebas en Builder 3,
  el controller y los gates.
- La cancelación solicitada por el usuario conserva la terminación explícita del árbol de
  procesos.
- Los límites de compilación de fixtures permanecen fuera de este cambio.
- `TA-AST-223` y `TA-AST-224` dejan el manifiesto ejecutable porque su oráculo dependía de
  un watchdog; permanecen versionados en `future-infrastructure.jsonl` hasta contar con
  terminación externa controlada por el usuario.

## Contrato resultante

Una prueba puede terminar por resultado propio o por cancelación explícita. El tiempo
transcurrido no cambia su estado ni provoca que la infraestructura mate el proceso.

Los campos `timeout_class` y `timeout_is_contract_failure` se eliminan del esquema y del
manifiesto porque ya no describen una conducta ejecutable.

## Validación autorizada

Esta tanda se cerró mediante auditoría estática de rutas, manifiestos y pruebas del gate.
Por instrucción expresa no se compiló NSC ni se ejecutó ninguna prueba.
