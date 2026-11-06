# Diagnóstico de campo C importado

El consumidor que combina el header C y una biblioteca precompilada recibió
`C++ instance field owner, layout or name is invalid`. La validación reúne
varias condiciones y el mensaje anterior no permite identificar cuál falló.
Se conserva la misma comprobación y se agrega la identidad, el nombre y el
primer motivo rechazado para localizar la causa sin omitir validaciones.
Pendiente de construcción incremental y reproducción.

La reproducción con build `b18c816e29a15702` identifica `MoodData.value` y
el motivo `field type not visible`. Se agrega al diagnóstico el identificador
de tipo y el ordinal del proyecto para comprobar la identidad involucrada.
El consumidor sin referencia explícita al header compiló en O0 con el header
presente en disco; esto no demuestra aislamiento del header.
