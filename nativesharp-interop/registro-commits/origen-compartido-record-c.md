# Evidencia del record C raíz

La comparación reúne el contrato capturado con el archivo equivalente existente,
el offset y la huella del buffer. Recalcula el archivo para no considerar vigente
un encabezado modificado después de analizarlo. Una ausencia o archivo virtual
produce evidencia insuficiente; diferencias observables se distinguen de ausencia.
El resultado solo cubre el record raíz, no sus dependencias ni su identidad
completa. No habilita conversiones. La construcción incremental y la suite de
sesión pasaron con código 0; falta la validación completa de la tanda.
