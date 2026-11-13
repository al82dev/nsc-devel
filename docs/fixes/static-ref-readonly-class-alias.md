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

## Agregados que contienen referencias

En `542137245`, dos sondeos aceptaron copiar un struct que contenía una clase
desde un retorno estático `ref readonly` y mutar la clase mediante la copia,
tanto con vida del argumento como con vida estática. Un tercer sondeo, sobre
el primer ensayo de corrección, aceptó la copia y escritura de un struct con
campo `ref int`. No se ejecutaron; las
fuentes y logs `static-readonly-aggregate-before-20261113`,
`static-lifetime-aggregate-before-20261113` y
`static-readonly-ref-field-before-20261113` se conservan en
`A:/nsc-fixes-evidencia`.

El análisis existente de campos de structs se aplica ahora a estos retornos y
reconoce campos `ref` y `ref mut` como portadores de alias. Para métodos
estáticos y retornos de vida estática, examina también los campos reconstruidos
de structs NativeSharp binarios; tipos desconocidos y campos importados C se
tratan conservadoramente. Los métodos de instancia con vida no estática
conservan la regla anterior para structs precompilados. Las tres reproducciones
se rechazan después del cambio (`static-readonly-aggregate-after-20261113.cmd`).

El primer ensayo rechazaba también una copia válida de un struct escalar
binario; los cuatro fallos O0/O2 están en
`static-readonly-aggregate-directed-20261113.log`. El ajuste posterior recorrió
los campos del layout publicados y reconstruidos. La regresión final pasó 48
comprobaciones semánticas y 30 ejecuciones: campos privados y anidados de
clase, campo `ref`, lectura, retorno mutable y copia escalar, con fuente y
productores binarios O0/O2 (`static-aggregate-private-after-20261113.log`).
Las comprobaciones semánticas se repiten sin variar optimización; el volcado
`static-aggregate-private-members-20261113.log` confirma la presencia del
campo privado y del calificador `ref` en este productor. Pasaron también los
96 controles de alias de clase y `nsc.item313.reference-returns` 1/1.
No se acreditan metadata antigua arbitraria, todos los layouts ni operadores.
