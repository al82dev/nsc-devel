# Débil implícito cuando la destrucción lanza

Los destructores no admiten una cláusula `throws` explícita en la sintaxis
probada, pero pueden propagar una excepción implícita: el destructor de la
reproducción llama a un método cuyo inicializador estático lanza. Tras soltar
el último fuerte, ambos controles destruían el payload y solo después liberaban
el débil implícito. Si la destrucción salía por excepción, esa última operación
no se ejecutaba y un débil explícito restante observaba dos referencias débiles
en lugar de una. Las reproducciones local y atómica devolvieron 14 en O0/O2;
antes habían comprobado excepción capturada, una entrada al destructor y cero
fuertes (`A:/nsc-fixes-evidencia/implicit-destructor-exception/*-before.log`).

`DropStrong` libera ahora el débil implícito en un `finally` alrededor de la
destrucción. La excepción se propaga; el débil explícito conserva el bloque y
su última liberación puede destruirlo. No se cambia la política de excepciones
del destructor ni se llama dos veces a la destrucción del payload.

La nueva prueba `nsc.sdk.shared-destructor-weak-cleanup` ejecuta clase y struct
en los controles local y atómico, O0 y O2. Verifica la captura, una ejecución
del destructor, fuerte cero, imposibilidad de retención, conteo débil uno y
liberación final. La versión anterior de los controles falla la regresión
(`A:/nsc-fixes-evidencia/destructor-weak-baseline/result.log`); las
reproducciones con clase pasaron O0/O2 (`implicit-destructor-exception/*-after.log`).
El build incremental y cuatro pruebas dirigidas, incluidas las dos de contienda
atómica y la de manejadores, pasaron 4/4
(`A:/nsc-fixes-evidencia/shared-destructor-weak-cleanup-ctest.log`).

Esta prueba no observa directamente la liberación de memoria sin débil explícito
ni verifica que se destruyan los campos del payload si su destructor se
interrumpe. Tampoco acredita consumo binario ni transferencia de las familias
públicas entre hilos.
