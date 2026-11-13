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

## Retornos con vida estática

En `cf8b8d4c3`, dos sondeos semánticos adicionales aceptaron copiar y mutar
una clase obtenida mediante `ref readonly[static]`, tanto desde un método
estático como desde uno de instancia. No se ejecutaron las muestras. Sus
fuentes y resultados anteriores están en las carpetas y logs
`static-lifetime-readonly-before-20261113` e
`instance-lifetime-readonly-before-20261113` de `A:/nsc-fixes-evidencia`.

El resultado de clase de un método `ref readonly` conserva ahora el préstamo
y la restricción de solo lectura también cuando la vida declarada es estática.
Las dos reproducciones se rechazan después del cambio; comandos y diagnósticos
en `static-lifetime-readonly-after-20261113.cmd` y los logs homónimos por
receptor. La prueba dirigida ampliada pasó 66 comprobaciones semánticas y 30
ejecuciones: fuente y productores binarios O0/O2, alias y métodos mutantes
rechazados, lecturas válidas y controles de referencias mutables. Las
comprobaciones semánticas se repiten sin variar optimización. Evidencia en
`static-lifetime-readonly-regression-20261113.log`; el objetivo de referencias
se compiló incrementalmente y `nsc.item313.reference-returns` pasó 1/1.
Siguen sin acreditarse las formas de agregados y operadores mencionadas arriba.
