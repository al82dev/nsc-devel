# Send y Sync condicionados para Arc

`Arc<T>` y `WeakArc<T>` solo pueden cruzar o compartirse entre hilos cuando `T` satisface tanto `Send` como `Sync`. El SDK expresa esa obligación mediante implementaciones `unsafe` condicionadas. `Rc<T>` y `WeakRc<T>` conservan su carácter local. La declaración insegura expresa una responsabilidad del autor: por sí sola no prueba la corrección de cualquier implementación.

El compilador conserva la forma condicionada por separado de las bases incondicionales de la plantilla. La coherencia compara identidades resueltas en el ámbito de cada implementación; exige que los marcadores sean interfaces y rechaza identidades duplicadas o no resolubles. Al verificar restricciones y capturas se reconoce únicamente la declaración y las identidades de marcadores que corresponden a la especialización; la carga se recorre iterativamente bajo ambas propiedades. Una identidad no resuelta se rechaza de forma conservadora. No se introdujo una versión nueva de metadata.

`A:/nsc-fixes-evidencia/arc-conditional-ctest.cmd` compiló incrementalmente y pasó seis regresiones dirigidas registradas: restricciones con impl en uno y varios archivos, coherencia, capturas y comprobaciones de `Arc` desde fuentes. `arc-conditional-final.cmd` pasó once matrices y pruebas dirigidas, incluidas variantes con Core binario aislado, cargas incompatibles, traits homónimos y los controles de captura. Las pruebas de uso concurrente y de promoción frente a liberación devolvieron 42 en O0 y O2; el detalle está en `arc-conditional-final-*.log`.

El compilador de prueba en `A:/nsc-fixes-evidencia/build/bin/nsc.exe` tuvo SHA-256 `47ed9a9150bf094666d822e68cb90e7d76ab09f613a8315f000a36f1fd8b4208` al realizarse la medición. El Core binario de `A:/nsc-fixes-evidencia/arc-public-sdk/NativeSharpCore.lib` tuvo SHA-256 `f91d95c248f477531d38a093605f45343f6be9736d15e2097acd452db4bb54e0`; su metadata, `f8e4189f7f35f457c61550b015ba001815e76ec0337973add8cc810f9aad669c`. Ambos proceden del árbol aislado con otros cambios locales; el Core no certifica un build limpio de este commit.

Estas pruebas no cubren todas las intercalaciones, cargas o implementaciones inseguras. La prueba de carrera no fuerza todas las planificaciones. La seguridad al descargar una biblioteca que aún contiene objetos compartidos y las vistas dentro de tuplas binarias siguen pendientes. Los controles binarios requieren una biblioteca Core aislada previamente construida y se ejecutan por separado; las pruebas registradas no dependen de una instalación en C.

Tras las integraciones recientes, `arc-upgrade-after-integrations-20261111.cmd`
repitió la carrera de promoción y el control coordinado contra
`A:/nsc-fixes-evidencia/pointers-readonly-sdk/NativeSharpCore.lib`. La
compilación terminó con código 0 y las ejecuciones O0 y O2 devolvieron 42;
el detalle está en el log homónimo. El Core se había construido antes de esta
tanda y el compilador contiene otros cambios locales. La carrera no cubre
todas las intercalaciones ni demuestra seguridad de descarga dinámica.

Al repetir esa prueba, los SHA-256 observados fueron `78aef06df829e03e23beb15a8c2f3d8d15dbb3d9e252e6231789d2f58ac5370d`
para el compilador, `565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`
para el Core y `1954dd440d33a3aba45f23cb4fc4611a6a56dbca7aac5e8bed8770add0988968`
para su metadata. Los hashes identifican esos artefactos, no certifican la
revisión de sus fuentes.
