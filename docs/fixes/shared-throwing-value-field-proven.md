# Construcción compartida con un campo de valor

## Ampliación a varios campos directamente construidos

En `2cfdfadd9` el caso conservado con dos campos `Field` era rechazado por la
barrera de limpieza parcial. Su MIR contiene una llamada de base opcional,
dos asignaciones de construcción directa y un throw. La comprobación del
constructor admite ahora una secuencia que inicializa cada campo propio una
vez mediante un constructor local no lanzador, con argumentos literales.
Comprueba pertenencia, identidad y ausencia de inicializaciones repetidas;
otras instrucciones conservan el rechazo. Se reutilizan los indicadores de
vida y la limpieza existentes del constructor.

Tras el build incremental, `shared_throwing_value_field_proven.py` registró
100 ejecuciones correctas O0/O2 y 16 rechazos esperados. Incluye uno, dos y
tres campos del mismo tipo, con y sin recurso owned, retorno normal o throw,
clases y estructuras en Rc/Arc, orden inverso de destrucción y fallo de base.
Las ramas y reasignaciones siguen rechazadas. El productor NativeSharp binario
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
variante que asigna los campos en el cuerpo en orden inverso sigue rechazada
en las ocho combinaciones probadas; no se ha demostrado la limpieza de esa
forma (`shared-value-field-reverse-probe-20261113.log`). Estas pruebas no
miden liberación física del bloque ni cubren metadata preexistente.

## Primer caso: un campo

En la base `078ae9ba1`, la reproducción original de `MakeRcThrowing` con una
carga que contiene un campo de estructura con destructor era rechazada para O0
y O2. El diagnóstico indicaba que la limpieza parcial no estaba demostrada.

El compilador admite ahora un caso limitado: un único campo propio de valor,
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
