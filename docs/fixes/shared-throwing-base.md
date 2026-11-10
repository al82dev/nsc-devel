# Fábricas throwing con bases no triviales

En `95dd725a1`, las reproducciones de `MakeRcThrowing` y `MakeArcThrowing`
con una base que tiene destructor o campo owned superaban el análisis semántico
pero se rechazaban durante la generación: `shared throwing construction cannot
safely clean this payload shape`. Los comandos y diagnósticos están en
`A:/nsc-fixes-evidencia/shared-throwing-base-current-{Rc,Arc}.log`.

La protección de `invokePlacementConstructor` trataba toda base con destructor
o campo owned como no verificable, aunque los constructores de fuente ya emiten
regiones para limpiar campos owned y bases cuya construcción terminó. Ahora,
para las fábricas compartidas throwing, se admite esa jerarquía cuando cada
layout de base es verificable y los campos owned de la base requieren la
destrucción que el backend reconoce. Se sigue rechazando una base con campo de
valor que requiere drop, un campo owned de base sin destrucción reconocida, y la jerarquía
precompilada no acreditada. La comprobación anterior de la fábrica no throwing
permanece intacta. La corrección no destruye incondicionalmente la base ni
desactiva el resto de protecciones de publicación del payload.

`shared_nontrivial_base_throw.py` verifica fallos antes, durante y después de
la construcción de bases, bases implícitas y explícitas, delegación, captura
local, éxito, orden de destructores y campos inicializados mediante helpers,
incluido un helper que lanza. Pasó para Rc y Arc en O0/O2 desde fuentes y como
consumidor de `shared-weakcount-sdk/NativeSharpCore.lib`; registros
`shared-throwing-base-ensayo-{Rc,Arc}.log` y
`shared-throwing-base-binary-{Rc,Arc}.log` en `A:/nsc-fixes-evidencia`.

La prueba de fábrica interna conserva los rechazos de campos de valor no
acreditados y comprueba que los dos ejemplos de base que antes se rechazaban
ahora se emiten; la regresión pública anterior acredita sus formas ejecutables.
Pasó `shared-throwing-factory-controls.cmd`. También pasaron los controles
dirigidos de publicación del receptor, rechazo de constructores throwing en
fábricas ordinarias y limpieza de campos (`shared-throwing-safety-controls.log`),
así como el control de identidad de estructuras.

El compilador se actualizó incrementalmente en A
(`shared-throwing-base-final-build.log`). El árbol de compilación incluye
archivos concurrentes sin registrar y el SDK binario procede de una compilación
anterior que también los incluía; estas pruebas no certifican un árbol limpio.
No se acredita la limpieza de campos de valor con drop, bases precompiladas,
una segunda excepción durante cleanup, el abandono de futures ni la seguridad
completa de los punteros. No se ejecutó la batería completa.
