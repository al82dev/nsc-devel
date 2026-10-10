# Tanda de Validación 42: diagnósticos de procesos de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Los mensajes producidos por el controlador ante cancelación o timeout sólo se
agregaban al texto devuelto por `run`. `CommandWorker` descarta ese texto, por lo
que `full.log` podía omitir la razón terminal aun cuando el proceso había sido
detenido por Builder 3.

## Corrección mínima

La cancelación anterior al lanzamiento, la cancelación activa y el timeout pasan
ahora por `self.output` además de conservarse en el texto de retorno. Así usan la
misma ruta progresiva y persistente que la salida del proceso.

## Evidencia

- Cancelación anterior al lanzamiento: mensaje emitido y código `130`.
- Timeout sintético: mensaje emitido, capturado y código `130`.
- Pruebas dirigidas del controlador de procesos: `6/6` PASS.
- Suite Python completa del controlador: `125/125` PASS.

Se usaron dobles; no se creó ningún proceso real, no se abrió la GUI y no se
configuró, compiló o ejecutó producto. No hubo baseline, despliegue ni escritura
en `C:`.
