# Imports en cuerpos de métodos genéricos

La regresión dirigida `nsc.generics.declaration-import-cache` usa una caché
aislada en A y un alias de namespace en el archivo donde se declara el método
genérico. El ejecutable eligió `ProducerA` (salida 21), luego se cambió el alias
a `ProducerB` y eligió ese destino (salida 33). Ambas repeticiones sin cambios
registraron `compilation-bundle-cache: hit` y conservaron sus resultados.
Pasó 1/1 en O0 (`A:/nsc-fixes-evidencia/generic-declaration-import-cache-ctest.log`).
El resultado tras cambiar el alias demuestra la selección correcta, pero no se
afirma qué entradas parciales de caché se reutilizaron en esa compilación.
La prueba usa fuentes del proyecto y no resuelve la publicación de ámbitos o
referencias en bibliotecas precompiladas.

En el prototipo de la fábrica pública `RcFactory.MakeRc<T>`, una llamada a
`SharedLocalFactory.Create<T>()` fallaba con `no visible Stage 44 generic
method template` aunque la clase era visible mediante un `using` de su archivo.
La reproducción mínima `A:/nsc-fixes-evidencia/public-factory-forwarding-probe`
mostró que el caso sin namespace y el nombre totalmente calificado pasaban,
mientras que el nombre importado fallaba. Los ejemplos originales del reporte
no se modificaron.

La causa estaba en `completeMethodSpecialization`: el cuerpo clonado se
procesaba con `GenericLookupScope{}` y perdía los imports y alias léxicos de la
declaración. La pasada existente de dependencias ya recorría el árbol con ese
contexto; ahora lo conserva por plantilla de método y el procesador utiliza
ese contexto al analizar el cuerpo especializado. Los imports del llamador no
se utilizan como sustituto.

`nsc.generics.declaration-imports` pasó con import, alias de namespace y nombre
completo en archivos separados, con ejecuciones O0/O2 de salida 21; el control
sin import se rechazó aunque el archivo consumidor importaba otro namespace.
Cada comprobación semántica válida se repitió; no se afirma que la repetición
demuestre un acierto de caché. El prototipo de `Rc` pasó O0/O2 con salida 42 y
rechazó el escape probado. Pasaron también `nsc.generics.selected-body-constraints`,
las dos pruebas unitarias dirigidas de genéricos y las cinco de controles
internos de punteros. Los comandos y salidas están en
`A:/nsc-fixes-evidencia/declaration-imports-test.log`, `generic-unit-tests.log`,
`shared-control-suite.log` y `public-rc-wrapper-probe/results-after-import-fix.json`.

`clang-tidy --checks=clang-analyzer-*` en los dos archivos C++ modificados retornó
0; sus logs registran avisos de padding en encabezados. No se acreditan todas
las formas de alias, metadata precompilada ni los demás caminos que procesan
especializaciones de tipos. No se ejecutó batería completa. NSLink fue
invocado inicialmente mediante su ruta absoluta por un wrapper heredado; se
cambió ese wrapper para resolver `nslink.exe` por PATH y se repitieron las
pruebas afectadas. No se modificó la instalación de C:.
