# Tanda de Validacion 94: claves JSON unicas en disposiciones

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

El lector JSONL local de Builder 3 aceptaba claves duplicadas en catalogos de
disposicion. La ultima definicion podia sustituir silenciosamente identidad,
estado o datos anidados antes de que `parse_disposition_row` aplicara sus
invariantes.

## Correccion

- `read_jsonl` construye cada objeto mediante un hook de claves unicas;
- la validacion cubre tambien objetos anidados;
- el diagnostico conserva archivo y numero de linea;
- los catalogos vivos se revalidaron sin modificar sus filas.

## Evidencia ejecutada

- pruebas dirigidas de disposiciones: `6/6` PASS;
- suite Python completa del controlador: `228/228` PASS;
- auditor canonico: `9,587` casos, `5,695` ejecutables y `0` sin clasificar;
- una clave anidada duplicada es rechazada;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en los catalogos JSONL consumidos
por Builder 3. No acredita una corrida funcional del producto. La meta
permanece activa hasta cero errores inesperados en la matriz acordada.
