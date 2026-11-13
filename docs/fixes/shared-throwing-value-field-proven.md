# Construcción compartida con un campo de valor

## Reasignación directa de campos

En `48256dc20`, la fábrica rechazaba una carga cuyo constructor reasignaba
un campo de valor antes de lanzar. La construcción directa equivalente limpió
el valor sustituido y el final en clase y struct, con y sin recurso owned, en
O0/O2. Otro control reasignó un campo antes de inicializar el segundo; las
trazas comprobaron el orden de las tres destrucciones. Evidencia en
`shared-branch-direct-control-20261113.log` y
`shared-reassign-early-direct-20261113.log`, bajo `A:/nsc-fixes-evidencia`.

La comprobación permite ahora varias asignaciones directas al mismo campo si
cada una construye el tipo esperado con un constructor local no lanzador y la
secuencia inicializa todos los campos. Los flags de vida y la sustitución
existentes realizan la limpieza. La regresión dirigida pasó 388 ejecuciones y
16 rechazos esperados, con Rc/Arc, clase/struct, éxito, throw, condición de
error, reasignación temprana y recursos owned en O0/O2. Dos productores
NativeSharp binarios y sus consumidores pasaron 16 cruces de familia y
optimización. Registros: `shared-reassign-fields-final-20261113.log` y
`shared-reassign-fields-binary-20261113.log`. No se midió la liberación física
del bloque ni se comprobó una excepción del destructor durante la sustitución.

## Condición de error al final del constructor

En `ad6d64b57`, la construcción compartida se rechazaba si, después de
inicializar los campos, el constructor usaba `if (Flag) throw new Exception()`.
Las construcciones directas equivalentes ejecutaron las ramas de éxito y error
para clases y estructuras, con y sin recurso owned, en O0/O2
(`shared-branch-direct-control-20261113.log`). El MIR de la reproducción está
en `shared-branch-mir-20261113.log`.

La comprobación estructural admite ahora esa condición final sin `else` cuando
el bool es literal o un campo estático no volátil, y el cuerpo solo construye
y lanza una excepción sin argumentos. En esa etapa seguían rechazadas otras
ramas y reasignaciones no demostradas. La regresión dirigida registró 260 ejecuciones
correctas y 24 rechazos esperados; incluye uno y dos campos, ambas ramas,
literales, propiedad anidada, Rc/Arc y clase/struct en O0/O2. Los productores
binarios y consumidores pasaron 16 cruces de rama, familia y optimización.
Los registros están en `A:/nsc-fixes-evidencia/shared-branch-fields-regression-20261113.log`
y `shared-branch-fields-binary-20261113.log`. No se midió la liberación física
del bloque ni se acreditan condiciones o cuerpos generales.

## Inicialización en el cuerpo del constructor

En `19bde64b3`, un constructor que asigna dos campos en orden inverso dentro
de su cuerpo se rechazaba en Rc y Arc, para clase y struct, O0/O2. La
construcción directa equivalente destruía los campos en orden de declaración
inversa, con traza 12 (`shared-reverse-direct-control-20261113.log`); el MIR
registró cada asignación como `ExpressionStatement` con un único `Store`.

La comprobación estructural reconoce ahora ese wrapper y aplica las mismas
validaciones de identidad, construcción, unicidad y ausencia de efectos
adicionales. Con el compilador incremental, la regresión registró 164
ejecuciones correctas y 16 rechazos esperados en O0/O2. Los casos nuevos
verifican éxito y excepción con y sin recurso owned, en Rc/Arc y clase/struct,
incluida la traza 12. Un productor NativeSharp binario de clase con recurso
owned y sus consumidores O0/O2 pasaron los ocho cruces. Los comandos y
resultados están en `A:/nsc-fixes-evidencia/shared-reverse-fields-final-20261113.log`
y `shared-reverse-fields-binary-20261113.log`. La prueba no mide la liberación
física del bloque ni cubre formas generales del constructor.

## Ampliación a varios campos directamente construidos

En `2cfdfadd9` el caso conservado con dos campos `Field` era rechazado por la
barrera de limpieza parcial. Su MIR contiene una llamada de base opcional,
dos asignaciones de construcción directa y un throw. La comprobación del
constructor admite ahora una secuencia que inicializa cada campo propio una
vez mediante un constructor local no lanzador, con argumentos literales.
En esa etapa comprobaba pertenencia, identidad y ausencia de inicializaciones repetidas;
otras instrucciones conservan el rechazo. Se reutilizan los indicadores de
vida y la limpieza existentes del constructor.

Tras el build incremental, `shared_throwing_value_field_proven.py` registró
100 ejecuciones correctas O0/O2 y 16 rechazos esperados. Incluye uno, dos y
tres campos del mismo tipo, con y sin recurso owned, retorno normal o throw,
clases y estructuras en Rc/Arc, orden inverso de destrucción y fallo de base.
En esa etapa las ramas y reasignaciones seguían rechazadas. El productor NativeSharp binario
de una clase con dos campos y recurso owned y sus consumidores O0/O2 ejecutaron
42 en los ocho cruces de familia y optimización. Comandos y resultados:
`A:/nsc-fixes-evidencia/shared-several-value-fields-final-20261113.log` y
`shared-several-value-fields-binary-20261113.log`. Esa ejecución no midió la
liberación física del bloque ni cubrió campos de distintos tipos, metadata
preexistente o cuerpos generales de constructor.

Una comprobación posterior con dos tipos de campo distintos pasó en clases y
estructuras, Rc y Arc, con y sin recurso owned, éxito y throw, en O0/O2. La
regresión ampliada registró 132 ejecuciones correctas y 24 rechazos esperados
(`A:/nsc-fixes-evidencia/shared-mixed-value-fields-regression-20261113.log`). Una
variante que asignaba los campos en el cuerpo en orden inverso se rechazó
en las ocho combinaciones probadas; entonces no se había demostrado la limpieza de esa
forma (`shared-value-field-reverse-probe-20261113.log`). Estas pruebas no
miden liberación física del bloque ni cubren metadata preexistente.

## Primer caso: un campo

En la base `078ae9ba1`, la reproducción original de `MakeRcThrowing` con una
carga que contiene un campo de estructura con destructor era rechazada para O0
y O2. El diagnóstico indicaba que la limpieza parcial no estaba demostrada.

En esa etapa se admitió un caso limitado: un único campo propio de valor,
de disposición normal, inicializado directamente por un constructor local sin
`throws`, con argumentos literales. El constructor local de la carga puede
invocar primero al constructor de base y después inicializar ese campo; puede
terminar normalmente o lanzar una excepción construida directamente. Los
indicadores existentes del constructor determinan si corresponde destruir el
campo. Las formas que no satisfacen la comprobación conservan la barrera.

La prueba dirigida `tests/integration/shared_throwing_value_field_proven.py`
comprueba `Rc` y `Arc`, cargas de clase y estructura, con y sin un recurso
propiedad del campo, y retorno normal o excepcional, en O0 y O2. También prueba
el fallo de la base y el rechazo de cargas con dos campos o control de flujo
condicional. Los contadores verifican las construcciones y destrucciones, y que
no se invoque el destructor de la carga incompleta. Se probó además el ejemplo
original y un productor NativeSharp binario con consumidores O0/O2.

Las pruebas emplearon el compilador incremental de
`A:\nsc-fixes-evidencia\build` y Core binario de
`A:\nsc-fixes-evidencia\integracion-vistas-sdk`. Los registros se conservan
en `A:\nsc-fixes-evidencia`, incluidos
`shared-value-field-rc-proof-20261113.log`,
`shared-value-field-success-check-20261113.log` y
`shared-throwing-value-field-binary-20261113.log`. La liberación física del
bloque compartido no se midió directamente. Cargas importadas y formas de
constructor más generales quedan fuera de esta demostración.
