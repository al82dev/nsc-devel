# Receptor compartido entregado a helper binario

Un productor NativeSharp estático define `Base` y un helper `Save(Base)` que
guarda su argumento en un campo estático. El consumidor define `Payload : Base`
y llama a `Save(this)` desde constructor o destructor antes de usar la carga
con Rc o Arc. La semántica y la emisión O0 aceptaron los cuatro ejemplos con el
compilador de trabajo; ningún consumidor se ejecutó. Los controles sin llamada
también emitieron. Fuente, comandos y hashes:
`A:/nsc-fixes-agente-2-evidencia/receiver-publication-external-helper-20261112/run-20261112T051126230771Z/results.json`.

La corrección comprueba solo argumentos que el análisis MIR reconoce
como derivados del receptor en una llamada directa durante la construcción o
destrucción de una carga de clase compartida. Si no encuentra función MIR o su
cuerpo falta, rechaza por cuerpo no verificable en lugar de asumir que no
publica. Este rechazo es conservador: también rechaza un helper binario benigno
que reciba el receptor cuando no se puede verificar su cuerpo. No modifica las
llamadas que no reciben dicho receptor ni las clases ordinarias fuera de esa
ruta. No cubre por sí mismo llamadas cuyo receptor es `this`, despacho virtual
o de interfaz, retornos opacos ni aliases no reconocidos, y no se presenta como garantía
general. La variante con función presente y cuerpo nulo no se reprodujo.

La matriz de base cubrió Rc/Arc y ctor/destructor: el helper binario que publica
y el benigno que recibe el receptor emitieron, mientras que el helper fuente
publicador se rechazó con el diagnóstico de publicación y los controles fuente
seguro y binario escalar emitieron. Registro:
`A:/nsc-fixes-agente-2-evidencia/receiver-helper-guard-20261112/run-20261112T051442051688Z/results.json`.
Un control distinto con campo `owned Base other = new Base()` y llamada al
helper binario con ese campo independiente pasó en las cuatro combinaciones:
`A:/nsc-fixes-agente-2-evidencia/receiver-helper-guard-20261112/run-20261112T051845336955Z/results.json`.
Una construcción local dentro del destructor fue rechazada antes de la guarda
por el contexto de declaración, por lo que no constituye un control de esta.

La regresión preparada compila un productor binario y comprueba semántica,
diagnóstico preciso y emisión de 24 casos O0, sin ejecutar consumidores.
Falla si el productor, metadata o un Core configurado están ausentes; sin Core
configurado o sin temporal adecuado en A se omite. Registra hashes de entrada y
falla si cambian. Los resultados anteriores corresponden a la base previa.

La compilación incremental posterior al parche terminó correctamente
(`A:/nsc-fixes-evidencia/receiver-helper-build-20261112.log`). La regresión
dirigida pasó 24 análisis y 24 emisiones O0: ocho helpers binarios con receptor
se rechazaron por cuerpo no verificable, cuatro helpers fuente publicadores por
publicación, y doce controles emitieron correctamente. Ningún consumidor se
ejecutó. La regresión previa de publicación directa también pasó; Core ausente
devolvió 77 y Core configurado incorrectamente devolvió 1. Comandos, hashes y
resultados en `A:/nsc-fixes-evidencia/receiver-helper-validation-20261112.json`
y los logs `receiver-helper-validation-*-20261112.log` de la misma carpeta.
El SHA-256 de nsc fue
`4726a3c676d17e1cf8d4d758230a526bdbcfa52aac61151b2d47f32c65e040d9` antes y
después; el Core binario era preexistente. El árbol contenía otros cambios y el
build reutilizó objetos incrementales; no certifica una revisión limpia ni
amplía los límites de cobertura indicados arriba.
