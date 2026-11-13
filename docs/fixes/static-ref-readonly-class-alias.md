# Alias de clase desde un retorno estático ref readonly

En `3147f5cfb`, el análisis aceptó el programa de
`A:/nsc-fixes-evidencia/static-readonly-alias-probe-20261113/Main.ns`: un
método estático devolvía `ref readonly` a un campo `owned`, el llamador copiaba
la clase a un local sin calificador y escribía mediante ese alias. No se ejecutó
ese programa. El resultado anterior fue observado en la ejecución de
`static-readonly-alias-probe-20261113.cmd`, pero su log se sobrescribió al
repetir el mismo comando después del cambio; no hay un log previo independiente.

`configureSelectedCallBinding` marca ahora como clase prestada de solo lectura
el resultado estático `ref readonly` con vida no estática, como ya hacía con
el retorno equivalente de instancia. No cambia el tratamiento de retornos
mutables ni habilita retornar referencias a parámetros `in` u operadores.
La reproducción original se rechaza después del cambio; el diagnóstico está
en `static-readonly-alias-probe-after-20261113.log`.

La prueba `static_ref_readonly_class_alias.py` comprueba rechazo de copia sin
calificador, mutación directa, llamada mutante, paso a método mutable y escape
por retorno o campo global. También compila y ejecuta lecturas y un control
de retorno mutable en O0/O2. Se probó con el productor fuente y con productor
NativeSharp binario O0/O2: 42 comprobaciones semánticas de rechazo y 12
ejecuciones correctas; las comprobaciones semánticas se repitieron sin variar
un ajuste de optimización. Comandos y resultados en
`A:/nsc-fixes-evidencia/static-readonly-alias-final-20261113.log`.
El objetivo de referencias se enlazó incrementalmente y
`nsc.item313.reference-returns` pasó 1/1; comandos y logs
`static-readonly-ref-build-20261113` y `static-readonly-ref-controls-20261113`.

Una declaración de local `borrowed Payload` a partir de este resultado sigue
rechazada por falta de un origen directo admitido; no se presenta como lectura
local soportada. No se comprobaron referencias readonly a agregados con alias,
retornos declarados `static`, operadores ni todas las formas de procedencia.
