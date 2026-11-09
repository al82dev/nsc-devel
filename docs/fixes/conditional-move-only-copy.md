# Copia move-only en expresiones condicionales

`true ? owner : owner` aceptaba leer un `Rc` desde cualquiera de las dos ramas
y producir un valor sin dirección. Por ello se podía copiar a otro propietario
o introducirlo en una tupla sin incrementar su contador. Las reproducciones se
limitaron al análisis semántico (`public-tuple-borrow-conditional_owner_copy.log`
y `public-tuple-borrow-tuple_owner_conditional_copy.log` en
`A:/nsc-fixes-evidencia`); no se ejecutó el código potencialmente inseguro.

El análisis del condicional rechaza ramas que son ubicaciones move-only cuando
el resultado tiene ese tipo. Se permiten ramas que producen clones nuevos y se
conserva el tratamiento de valores copiables y de `never`. No habilita
transferencias `move` que ya eran inválidas antes del cambio.

Los controles internos local y atómico verificaron el rechazo tanto en una
asignación como dentro de una tupla y ejecutaron clones en O0/O2, incluso con
una rama `never` (`readonly-array-ctest-conditional_owner_never.log`). Las pruebas
públicas comprobaron `Rc` y `Arc`, sus débiles, una rama mezclada de clon y
ubicación, y la ejecución de clones; todas pasaron
(`public-tuple-borrow-conditional_owner_copy,tuple_owner_conditional_copy,conditional_owner_mixed,conditional_weak_copy,conditional_owner_clone.log`).
Los tres controles existentes de tuplas y agregados pasaron 3/3 tras el cambio
(`tuple-owner-existing-controls.log`). La información de esta prueba desde
fuentes no certifica el SDK binario ni cubre todas las formas de expresión.
