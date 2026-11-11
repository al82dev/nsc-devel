# Punteros compartidos en tuplas precompiladas

Al reconstruir la superficie de una biblioteca, los elementos de tupla con
`Rc`, `WeakRc`, `Arc` o `WeakArc` no se reconocían. Un productor con un método
que devuelve una tupla con clon compilaba, pero el consumidor no encontraba el
método para Rc y WeakRc. Un estado público de namespace con una tupla Arc o
WeakArc tampoco permitía generar la biblioteca; sus controles directos sí.

El mapeo reconstruye exclusivamente especializaciones de las cuatro familias
del módulo NativeSharpCore cuando la identidad completa coincide con un
registro de especialización de la metadata. Se reutiliza la conversión de
especializaciones existente, sin cambiar el formato. Tipos homónimos de otro
módulo no se reconocen como las familias del SDK.

Las pruebas dirigidas `nsc.sdk.namespace-weakarc-binary` y
`nsc.sdk.shared-tuple-return-binary` pasaron cada una 1/1 en O0/O2; verifican
contadores tras reemplazo o salida de ámbito y controles homónimos. También
pasó el control de tuplas numéricas. Para separar este conjunto del ensayo
pendiente de vistas, se retiró temporalmente la condición de SharedView, se
compiló incrementalmente y se ejecutaron esas tres pruebas; se restauró el
archivo byte por byte y se recompiló incrementalmente. Los comandos y logs
están en `A:/nsc-fixes-evidencia/shared-tuple-commit-20261111/`.

Las pruebas usan el árbol de trabajo con otros cambios locales y el Core
binario aislado preexistente, generado por un compilador anterior; no son una
validación de una revisión limpia. No prueban metadata histórica, DLL,
descarga de módulo, vistas prestadas ni todos los tipos anidados. El análisis
independiente de vistas y sus barreras sigue pendiente y no forma parte de
este cambio. Las pruebas opcionales se omiten si no se configura
`NSC_TEST_CORE_BINARY`.
