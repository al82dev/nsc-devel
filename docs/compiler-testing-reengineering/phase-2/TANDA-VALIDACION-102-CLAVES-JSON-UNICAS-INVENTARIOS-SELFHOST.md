# Tanda de Validacion 102: claves JSON unicas en inventarios self-host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

Los 18 controladores self-host comparten `selfhost_inventory.read_jsonl`. Ese
lector seguia aplicando last-wins: una segunda definicion dentro de `oracle`,
`inputs`, estado o identidad podia sustituir silenciosamente al contrato
declarado, aunque el cargador canonico ya rechazaba esa ambiguedad.

## Correccion

- el lector compartido exige claves unicas en todos los objetos JSON;
- la violacion conserva archivo, linea y clave duplicada en el diagnostico;
- la construccion del objeto es iterativa y no introduce recursion;
- catalogos, manifiestos y reglas de los controladores permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas del contrato self-host: `5/5` PASS;
- suite Python completa del controlador: `236/236` PASS;
- una segunda definicion anidada de `exit_code` es rechazada;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves al leer inventarios JSONL por los
controladores self-host. No acredita una corrida funcional de esos
controladores ni del producto. La meta permanece activa hasta cero errores
inesperados en la matriz acordada.
