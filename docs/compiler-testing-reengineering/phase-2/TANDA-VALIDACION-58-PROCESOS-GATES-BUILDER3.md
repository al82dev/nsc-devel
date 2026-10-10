# Tanda de Validación 58: procesos cancelables de gates Python en Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Los `36` casos BuildSystem y `12` Packaging invocaban `run_case` dentro del
proceso de Builder 3, pero sus hijos nacían mediante el `subprocess.run` privado
del módulo importado. `ProcessController` no poseía esos procesos: Cancelar no
podía terminar su árbol y su timeout no participaba en la clasificación central.

## Corrección mínima

`ProcessController.run_completed` conserva stdout y stderr separados mientras
transmite ambos a la consola progresiva, controla el árbol y registra
cancelación o timeout. `_run_python_gate_case` sustituye temporalmente el
`run_process` del gate por este runner y lo restaura siempre. El resultado sigue
siendo el `CompletedProcess` que esperan los oráculos existentes.

## Evidencia

- Captura separada: stdout y stderr preservados.
- Cancelación previa: no lanza proceso y retorna `130`.
- Cancelación activa: termina el hijo conocido y conserva causa.
- Timeout activo: termina el hijo conocido y conserva causa.
- Runner de gate: inyectado con el timeout solicitado y restaurado.
- Excepción del oráculo: restaura el módulo y conserva evidencia.
- Pruebas dirigidas: `6/6` PASS.
- Suite Python completa del controlador: `189/189` PASS.

Todas las pruebas usaron dobles; no se creó ni terminó un proceso real. No se
modificaron gates, comandos u oráculos. No se abrió la GUI y no se configuró,
compiló o ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
