# Tanda de Validacion 93: claves JSON unicas del manifiesto

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`load_manifest` usaba `json.loads` sin un detector de claves duplicadas. La
politica implicita conservaba la ultima definicion y descartaba las anteriores,
por lo que un caso podia tener contratos ambiguos incluso dentro de `inputs` u
`oracle`.

## Correccion

- cada objeto JSON se construye mediante un hook que exige claves unicas;
- la regla se aplica tambien a objetos anidados;
- el diagnostico conserva archivo y numero de linea del JSONL;
- el manifiesto vivo se revalido sin modificar ninguna fila.

## Evidencia ejecutada

- pruebas dirigidas del manifiesto: `15/15` PASS;
- suite Python completa del controlador: `227/227` PASS;
- auditor canonico: `9,587` casos, `5,695` ejecutables y `0` sin clasificar;
- una clave `inputs.value` duplicada es rechazada;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en el manifiesto ejecutable
canonico. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
