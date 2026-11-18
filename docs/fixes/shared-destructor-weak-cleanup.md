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

Las evidencias históricas descritas arriba tienen límites de procedencia: el
script `implicit-destructor-exception/run-probe.py` inició los procesos de los
registros `*-after.log` en la carpeta temporal y esos registros no son
conformes. La conformidad de los `*-before.log` no está acreditada. El script
histórico `shared_destructor_weak_cleanup.py` también usa un directorio
temporal para sus procesos; su registro CTest no acredita ejecución conforme.

Una comprobación posterior usa las familias públicas y Core binario aislado:
el destructor de una clase o struct lanza implícitamente desde un inicializador
estático. Para Rc y Arc en O0/O2, los ocho programas compilaron y devolvieron
42; verifican captura, una destrucción, `Expired`, un débil explícito y
`Upgrade` vacío. Comandos, fuentes, salidas y hashes antes/después:
`A:/nsc-fixes-agente-2-evidencia/shared-destructor-binary-20261112/run-20261112T042041677058Z/results.json`.
El compilador y Core corresponden a un árbol WIP; el ensayo no observa la
liberación física ni acredita las rutas pendientes descritas arriba.

La prueba permanente `nsc.sdk.shared-destructor-weak-binary` realiza esos ocho
cruces; recibe el compilador de CTest y el Core binario mediante
`NSC_TEST_CORE_BINARY`. Requiere una carpeta temporal existente en A mediante
`NSC_TEST_TEMP_ROOT` o `TEMP`, e informa omisión 77 si faltan los requisitos.
Los procesos arrancan desde el repositorio. La ejecución dirigida con el
compilador y Core WIP terminó con 0; sin Core terminó con 77. Registros:
`A:/nsc-fixes-agente-2-evidencia/shared-destructor-binary-20261112/permanent-configured.log`
y `A:/nsc-fixes-agente-2-evidencia/shared-destructor-binary-20261112/permanent-missing-core.log`.
La versión posterior también registra SHA-256 del compilador, Core y metadata
antes y después, y falla si cambian. Sus registros dirigidos son
`A:/nsc-fixes-agente-2-evidencia/shared_destructor_weak_binary-hashed-configured-20261112.log`
y `A:/nsc-fixes-agente-2-evidencia/shared_destructor_weak_binary-hashed-missing-core-20261112.log`.
