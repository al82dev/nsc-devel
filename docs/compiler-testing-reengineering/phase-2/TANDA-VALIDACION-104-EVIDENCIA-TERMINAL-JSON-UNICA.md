# Tanda de Validacion 104: evidencia terminal JSON unica

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

Los 14 gates host y el gate Infrastructure reabrian `results.json`; este
ultimo tambien reabria `events.jsonl`. Esas lecturas aplicaban last-wins, por
lo que una segunda identidad, estado, conteo o carga de evento podia sustituir
silenciosamente la evidencia publicada por el controlador.

## Correccion

- `component_gate_support.read_json_object` exige claves unicas en documentos;
- `component_gate_support.read_jsonl` sigue siendo la unica frontera para
  streams JSONL y exige la misma unicidad;
- los 15 gates consumen ahora esos lectores antes de validar resultados;
- seleccion, orden, estados esperados y ejecucion permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas de soporte e Infrastructure: `15/15` PASS;
- suite Python completa del controlador: `238/238` PASS;
- comprobacion de sintaxis de todos los gates de integracion: PASS;
- no quedan llamadas a `json.loads` en los 14 gates host ni en Infrastructure;
- una segunda definicion anidada de `PASS` es rechazada;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves al reabrir evidencia terminal en
los gates host e Infrastructure. No acredita una corrida funcional de esos
gates ni del producto. La meta permanece activa hasta cero errores inesperados
en la matriz acordada.
