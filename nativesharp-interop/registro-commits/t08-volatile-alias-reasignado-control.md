# T08-010/015: control de alias volatile reasignado

Se amplía el control de IR contra Clang con lectura y escritura por un alias
local que se inicializa desde un miembro flexible y después se reasigna a otro.
La comparación conserva ambos modos de Clang, referencias mixtas y O0–O3. Se
comprobó con `CALL diagnosticar-var-volatile.cmd`, NSC build
`1965527e723a8dbb` y Clang 23.1.0: terminó con código 1 por las diferencias
esperadas. En O0–O3, Microsoft requiere carga acquire y escritura release para
el alias reasignado, mientras NSC genera volatile simple; ISO coincide. Con
referencias mixtas el IR coincide con ISO y difiere de Microsoft.

El resultado demuestra que la corrección debe cubrir reasignación, pero no
resuelve qué modo corresponde al consumidor mixto ni prueba concurrencia. El
control es diagnóstico y la tanda permanece abierta.
