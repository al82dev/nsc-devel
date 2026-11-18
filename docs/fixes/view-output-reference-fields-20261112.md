# Salidas de vistas a través de campos ref

En `f194b1c55`, una llamada con salida `ref` podía guardar una vista de un
dueño local interior en un struct exterior mediante un campo `ref` intermedio.
El compilador aceptaba la reproducción semántica; el programa inseguro no se
ejecutó. Evidencia: `A:/nsc-fixes-evidencia/view-output-reference-field-borrowed-20261112.log`.
Tras la primera guarda, un alias local del mismo campo aún eludía el rechazo:
`A:/nsc-fixes-evidencia/view-ref-field-alias-before-20261112.log`.

El registro existente de orígenes de campos ref utiliza el ordinal del miembro,
no la identidad de la instancia, y por tanto no permite identificar de forma
fiable el almacenamiento referenciado. `recordViewOutputOrigins` rechaza el
paso de una vista con dueño local conocido a través de un campo ref de destino
desconocido, incluso mediante un alias local o su représtamo conocido. Esta
decisión conservadora fue autorizada por el usuario y rechaza también algunos
usos seguros, incluido el control de dueño y destino en el mismo ámbito. No se
afirma que identifique todas las referencias, orígenes desconocidos o destinos
indirectos; falta un análisis preciso por instancia y flujo para admitirlos.

Build incremental del target `nsc`: 0; compilador aislado SHA-256
`8ab99ef04d6e1c90f8300726524d121b9a35bece7844c856d09635eaf74c6e3b`.
`view-ref-field-final-20261112.log` verifica análisis dirigido para salida ref,
out, alias, représtamo, el rechazo conservador y dos controles aceptados. Los
controles existentes de campos por valor, alias de parámetros y llamadas de
delegado, evento y funciones de namespace pasaron; logs `view-ref-field-struct-final`,
`view-ref-field-parameter-alias-final` y `view-ref-field-call-{0,1,2}` en
`A:/nsc-fixes-evidencia`, todos con sufijo `-20261112.log`. Un intento con
nombre de runner inexistente terminó 2; se corrigió el nombre y pasó el runner
de alias de parámetro. Estas pruebas son semánticas, no ejecución nativa ni
demostración completa del contrato de vistas. No se ejecutó batería completa.
