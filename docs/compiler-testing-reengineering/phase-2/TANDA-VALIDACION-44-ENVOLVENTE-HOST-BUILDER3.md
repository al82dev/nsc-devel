# Tanda de Validación 44: envolvente host de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados

La ruta host sólo cotejaba `case_id`. Un contrato o componente ajeno, un estado
desconocido, una lista de fallos mal tipada o una contradicción entre estado y
exit code podían cruzar la frontera. Para un defecto conocido, un estado
inventado podía incluso traducirse a `KNOWN_FAILURE`.

## Corrección mínima

`parse_host_envelope` exige:

- exactamente un objeto JSON;
- identidad exacta de caso, contrato y componente;
- `duration_us` entero no negativo;
- `failures` como lista de cadenas;
- estados exclusivos `PASS` o `FAIL`;
- correspondencia `PASS -> 0` y `FAIL -> 1`.

`_run_host` conserva el log y convierte cualquier violación en
`INFRASTRUCTURE_ERROR` antes de aplicar el resultado esperado del caso.

## Evidencia

- Envolventes PASS/FAIL válidas: aceptadas.
- Tres dimensiones de identidad incorrecta: rechazadas.
- Estado y fallos mal tipados: rechazados.
- Dos contradicciones de exit code: rechazadas.
- Duración y cardinalidad inválidas: rechazadas.
- Pruebas dirigidas: `5/5` PASS.
- Suite Python completa del controlador: `134/134` PASS.

No se ejecutó el host, no se abrió la GUI y no se configuró, compiló o ejecutó
producto. No hubo baseline, despliegue ni escritura en `C:`.
