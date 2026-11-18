# Operadores sobre un campo de receptor calculado

En la reproducción `++Select(holder).Item` y sus variantes, el campo era
direccionable, pero la llamada devolvía una referencia prestada sin ordinal
de dueño en el análisis. El compilador rechazaba los cuatro positivos y no
alcanzaba los diagnósticos de dos negativos. La traza previa está en
`A:/nsc-fixes-evidencia/operadores-traza-raiz.log`.

El análisis reconoce el dueño local RAII cuando una llamada estática de fuente,
con un único parámetro prestado, devuelve directamente ese parámetro y sus
sentencias previas consisten solo en escrituras numéricas estáticas de la forma
comprobada. La variable prestada conserva el dueño y el campo, para impedir
su reemplazo o su escape mientras la vista vive. El lowering existente obtiene
la dirección una sola vez. No se reconoce por el solo hecho de existir un
argumento: una llamada que devuelve otro objeto o ejecuta una llamada previa
sin contrato demostrado permanece rechazada.

La compilación incremental de `nsc` pasó. El ejecutor
`operator_computed_receiver_vistas.py` pasó prefijo y postfijo, incremento y
decremento, O0/O2, contando evaluación y destrucción; rechazó reemplazos y
escape, y mantuvo el rechazo de dos receptores no acreditados. Los negativos
solo se analizaron. `operator_increment_owned_field_throw.py` también pasó
en O0/O2 para las cuatro formas directas y las cuatro con receptor calculado,
verificando una evaluación y conservación del campo cuando el operador lanza.
Logs: `A:/nsc-fixes-evidencia/operadores-identidad-controles-final.log` y
`A:/nsc-fixes-evidencia/operadores-identidad-excepciones.log`. Después de
excluir expresamente cuerpos precompilados se reconstruyó incrementalmente y
se repitieron ambos: `verificado-operator-computed.log` y
`verificado-operator-throw.log`.

Pasaron las pruebas dirigidas existentes de incremento/decremento, propietarios,
campos y alias (`dirigido-operator_increment_*.log` en la misma carpeta).
La repetición con el último ajuste quedó en `verificado-operator-field.log`,
`verificado-operator-alias.log`, `verificado-operator-owned.log` y
`verificado-operator-scalar.log`.
Los controles originales 39 y 70 se repitieron: el 39 se analizó y emitió sin
ejecutarlo; el 70 también se ejecutó. Estas pruebas no acreditan receptores
binarios, cuerpos con control de flujo o llamadas, varios parámetros, alias
arbitrarios ni seguridad de concurrencia. Esas formas siguen pendientes o
conservan el rechazo anterior. No se modificaron los ejemplos originales.
