# Construcción compartida con un campo de valor

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
