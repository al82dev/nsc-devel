# T08: incorporar la dirección flexible subalineada al control

El validador copia `RestrictedPackedAddress` y exige un rechazo semántico al
tomar la dirección. La comprobación individual rechazó la operación; el texto
actual menciona además campos de bits y es impreciso para este caso. La prueba
no cambia ese diagnóstico general. Falta repetir el control completo flexible.
