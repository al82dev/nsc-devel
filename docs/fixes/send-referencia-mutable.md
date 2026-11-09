# Send de una referencia mutable

`RefMutRaw` contiene `ref mut Raw`; `Raw` contiene `int*`. La regla estructural
existente requiere que una referencia mutable sea `Send` solo si su referente
lo es. Antes de la corrección, `--check-project-method-bodies` aceptaba
`Require<RefMutRaw>() where T : Send`, aunque rechazaba `Raw`; la matriz completa
anterior está en `A:/nsc-fixes-evidencia/thread-payload-current.json`. El modelo
de miembros muestra la clave `refmut:type:Probe::Raw` para ese campo y
`builtin:int*` para su referente (`thread-payload-members.log`).

`ProjectGenericConstraintModel::satisfiesThreadSafetyConstraint` asignaba
`send = true` a la referencia sin recorrer su tipo. Ahora, al evaluar `Send`,
añade el referente al recorrido estructural existente y rechaza un puntero
crudo directo; la comprobación `Sync` y las demás clases de campo se conservan.
El recorrido usa los ordinales ya visitados para evitar ciclos.

La compilación incremental usó `A:/nsc-fixes-evidencia/build/bin/nsc.exe` y
coexistió con cambios ajenos sin registrar. La regresión dirigida
`tests/integration/send_mutable_reference.py` pasó tanto con Core desde fuentes
(`send-mut-ref-check.cmd`) como con el SDK binario aislado
`generic-owner-sdk-current/NativeSharpCore.lib` (`send-mut-ref-binary-check.cmd`).
Comprueba `Safe`, `Raw`, `ref mut int`, `ref mut Raw` y un contenedor anidado,
para Send y Sync. Antes del build, esa misma prueba falló al aceptar
`Send:RefMutRaw`. También pasaron `nsc.cc1.generic-constraints` y las dos
pruebas dirigidas de campos anidados y préstamo (`nested-thread-safety-tests.log`).
Los registros están en `A:/nsc-fixes-evidencia`; la prueba nueva se ejecuta
directamente mientras el archivo CMake compartido tiene cambios ajenos.

Esto corrige el caso comprobado de las restricciones genéricas, no acredita
todas las clases de referencias ni el análisis de capturas. La transferencia
de `Arc` y `WeakArc` entre hilos sigue pendiente de identidad del proveedor,
seguridad del payload y demás garantías; no se habilitó en este cambio.
