# Prueba de llamadas de delegado con excepciones

`nsc.stage42.delegates-events` fallaba en dos comprobaciones en la revisión
base y en la rama de correcciones. El IR de la reproducción usa `invoke` con
destinos de excepción en lugar de `call`; conserva la llamada estática con
el argumento de usuario y la llamada de instancia con receptor y argumento.
La inspección se registró en `A:/nsc-fixes-evidencia/delegate-ir/dump.log`.

La prueba acepta ambas instrucciones y comprueba explícitamente los
argumentos de cada llamada. `nsc.stage42.delegates-events` pasó después del
ajuste (`delegate-test-final.log`). No se cambió la generación del compilador.
