# Investigación: FnOnce en campos estáticos

Estado: prototipo descartado para integración; no resuelve las mutaciones indirectas y rechaza
programas válidos. La rama principal no fue modificada. El parche experimental se aplicó y
probó en una combinación independiente de forwarding, pero no debe integrarse.

La reproducción original `saved = action; action(); saved();` se acepta con el compilador WIP
anterior. El prototipo identifica el campo por su binding semántico y conserva la procedencia de
entornos en el estado de flujo. Rechaza esta ruta, el orden inverso, dos llamadas por el campo,
alias campo→local, campo→campo, autoasignación y una rama con posible consumo. Acepta controles
seguros de una sola llamada, de reemplazo directo por otra closure y de varias llamadas `Fn`;
estos controles se ejecutaron O0/O2. Los negativos se limitaron a análisis.

El código acotado probó la independencia del forwarding descartado: se aplicó sobre la
instantánea anterior a ese parche y se recompilaron las tres unidades afectadas. El ejecutable
WIP resultante tiene SHA-256
`b5fea19447a221403674d893a87cc68ebba3e59f27ee40bee2b72ecedefb403a`. La prueba está en
`tests/integration/fnonce_static_field.py`; el log de los controles iniciales exitosos está en
`A:\nsc-fixes-agent1-temp\fnonce-param-tests\field-independent.log`.

Al ampliar los controles aparecieron dos falsos positivos concretos:

1. Tras `saved=action; action(); Replace(other);`, el método `Replace` cambia el campo a otra
   closure, pero el prototipo conserva la identidad anterior y rechaza una llamada válida.
   Evidencia: `A:\nsc-fixes-agent1-temp\fnonce-param-tests\field-indirect.log`.
2. La primera llamada a `saved()` ejecuta la propia closure, que reasigna `saved` a un método
   distinto. La siguiente llamada corresponde a ese método. El compilador WIP previo acepta el
   ejemplo y el prototipo lo rechaza. Fuentes y logs en
   `A:\nsc-fixes-agent1-temp\fnonce-field-mutation`.

Una frontera mínima es `[move box] () => box`: la doble llamada por el local se diagnostica
como FnOnce y su cuerpo lee la captura sin llamadas ni escritura. Para la secuencia inmediata
`saved=action; action(); saved()` el compilador previo acepta y el prototipo rechaza. La
reproducción se analizó sin ejecutarse; las salidas se conservaron separadas en
`A:\nsc-fixes-agent1-temp\fnonce-field-pure-before` y `A:\nsc-fixes-agent1-temp\fnonce-field-pure-after`.
Esto solo justifica estudiar una regla cuando la identidad de captura, la ausencia de efectos
intermedios y el tratamiento de escrituras concurrentes sean demostrables; el parche actual no
comprueba estas premisas. Invalidar el dato ante todas las llamadas evitaría algunos falsos
positivos, pero también dejaría sin detectar usos repetidos a través del campo.

Una solución útil requiere información semántica fiable sobre escrituras de campos de cada
llamada, incluidos callbacks, dispatch y código externo, además de un tratamiento explícito de
concurrencia y del flujo excepcional. También faltan campos de instancia y accesos entre métodos.
El prototipo conservado `A:\nsc-fixes-agent1-temp\fnonce-field-prototype.patch` es evidencia
de investigación, no un arreglo listo para integrar. La prueba actual incluye deliberadamente
el control válido `reassigned_indirect`, que muestra el fallo del prototipo.
