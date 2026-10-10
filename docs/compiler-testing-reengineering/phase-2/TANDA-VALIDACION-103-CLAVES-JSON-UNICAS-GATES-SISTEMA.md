# Tanda de Validacion 103: claves JSON unicas en gates de sistema

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

BuildSystem y Packaging conservaban lectores JSONL locales con politica
last-wins. Una clave repetida en sus catalogos, clasificaciones, conteos u
oraculos podia cambiar silenciosamente el contrato que esos gates validaban.

## Correccion

- ambos gates consumen ahora `component_gate_support.read_jsonl`;
- el lector comun exige claves unicas en todos los objetos JSON;
- archivo, linea y clave duplicada permanecen en el diagnostico;
- la construccion del objeto es iterativa y no introduce recursion;
- las reglas particulares de BuildSystem y Packaging no cambiaron.

## Evidencia ejecutada

- pruebas dirigidas del soporte de gates: `9/9` PASS;
- suite Python completa del controlador: `237/237` PASS;
- una segunda definicion anidada de `required` es rechazada;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en los documentos JSONL consumidos
por los gates BuildSystem y Packaging. No acredita una corrida funcional de
esos gates ni del producto. La meta permanece activa hasta cero errores
inesperados en la matriz acordada.
