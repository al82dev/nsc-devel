# Acceso estático a una clase interna de otro proyecto

En la revisión `d2431a04b`, un consumidor de fuentes podía ejecutar
`Hidden.Plain()` o `Hidden.Generic<Payload>()` cuando `Hidden` era una clase
`internal` de otro proyecto sin contrato Friend. El uso mediante una
declaración local del mismo tipo se rechazaba, al igual que un método
`internal` de un tipo público. Sin `using Sample`, el acceso estático también
se aceptaba. La reproducción y sus comandos están en
`A:/nsc-fixes-evidencia/internal-visibility-contrast/result.json`; el caso
genérico se compiló y ejecutó con resultado 42 en
`internal-owner-visibility-probe/result.json`.

La búsqueda principal de tipos en expresiones respeta la visibilidad. Cuando
no registra candidato, `resolveWrittenMemberType` usa una búsqueda alternativa
por nombre que no comprobaba la visibilidad del tipo encontrado. Para esa vía,
la corrección comprueba la visibilidad existente de una clase `internal`
respecto al proyecto de la función que la usa, incluido el contrato Friend.
Si no es visible, emite un diagnóstico y rechaza el receptor. No cambia la
búsqueda alternativa compartida ni la selección de nombres públicos sin
importar, y no altera metadata o interoperabilidad C/C++.

La regresión dirigida `nsc.visibility.internal-static-type` comprueba el
rechazo con método ordinario, genérico y sin import, y conserva los controles
de método interno y declaración local. Comprueba que la API pública del
productor puede usar sus propios helpers internos, incluso desde una plantilla,
con ejecución 42 en O0/O2, y que un consumidor declarado Friend puede usar
el tipo. Pasó directamente y por CTest
(`internal-static-type-visibility.log` e `internal-static-visibility-ctest.log`
en `A:/nsc-fixes-evidencia`).

El compilador se reconstruyó incrementalmente en A. La regeneración de CMake
y la identidad de versión implicaron 37 acciones
(`internal-static-visibility-build.log`). El análisis semántico de Core y del
ejemplo original de punteros pasó; la prueba dirigida de uso de `Rc`/`Arc`
ejecutó 42 en O0/O2. El análisis `clang-tidy --checks=clang-analyzer-*` del
archivo modificado terminó con código 0 y cinco advertencias de padding en
encabezados (`internal-static-visibility-analysis.log`).

El cambio se limita al receptor obtenido por esa vía alternativa y a clases
`internal`. No demuestra protección de todos los tipos, rutas de nombres o
escenarios de metadata precompilada. La regresión
`generic_internal_owner_metadata.py` sigue fallando antes de consumir el
binario porque falta el propietario interno en la metadata; no se ha
modificado el productor de metadata compartido.
