# Validación 141 — carga de historial en Builder 3

`Pruebas` permite seleccionar y cargar una ejecución terminal guardada. La
carga reconstruye cada variante exacta, sus estados, duraciones, detalles, log
completo y fallos reejecutables. El historial también ofrece `Cargar ejecución
seleccionada` y el doble clic realiza la misma operación.

La evidencia cargada es de sólo lectura. `Reejecutar fallos` crea un nuevo
`run_id` y otra carpeta; nunca sobrescribe el log anterior. Se rechazan
ejecuciones incompletas, malformadas, ajenas a `phase-2-suite` o incompatibles
con el catálogo vigente.

Evidencia local: sintaxis Python `PASS`, contratos dirigidos `33/33`, smoke de
GUI `PASS` y `git diff --check` `PASS`. No se compiló ni ejecutó producto, no se
interfirió con la corrida activa del usuario, no se ejecutó baseline y no se
tocó `C:`.
