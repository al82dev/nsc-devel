# Tanda de Validacion 98: claves JSON unicas al reabrir historial

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`history_record` reabria `metadata.json` y `results.json` con la politica
last-wins. Una corrida persistida con identidad, estado o conteos duplicados
podia proyectarse en la tabla historica usando solamente la ultima definicion.

## Correccion

- metadata y resultados se analizan con claves unicas;
- una duplicacion convierte la corrida en `MALFORMED`;
- el stream de eventos ya comparte el lector JSONL estricto cerrado en la
  Validacion 94.

## Evidencia ejecutada

- pruebas dirigidas del historial: `5/5` PASS;
- suite Python completa del controlador: `232/232` PASS;
- metadata con `schema` duplicado y resultados con `status` duplicado se
  clasifican `MALFORMED`;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves al reabrir metadata, resultados y
eventos historicos. No acredita una corrida funcional del producto. La meta
permanece activa hasta cero errores inesperados en la matriz acordada.
