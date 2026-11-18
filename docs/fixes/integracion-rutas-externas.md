# Integración de fuentes externas de la laptop

El commit de origen `48834e5d1ff16c6c42f63bb35fe3f2c780cf2be1` permite
inclusiones `Compile` explícitas relativas con `..`, incluidos globs, sin
extender la búsqueda implícita fuera del proyecto. El contrato y los límites
están en `rutas-externas-laptop.md`. Se aplicó en
`development-fixes-limitaciones` sin mezclar ramas. Los cambios locales de
otras áreas no forman parte de esta integración.

En `A:/nsc-fixes-evidencia`, `integration-external-sources-build.cmd` compiló
incrementalmente `nsc`, `nsc_project_loader_tests` y
`nsc_compilation_model_tests`; terminó con código 0. El build contiene cambios
locales concurrentes del frontend y SDK, por lo que esta validación no
certifica un árbol limpio de la revisión resultante.

`integration-external-sources-test.cmd` terminó con código 0. El script
`tests/integration/project_external_sources.py` verificó archivo y glob
externos con ejecución 42 en O0/O2, selección, exclusiones, `Remove`,
duplicados, rutas inválidas, límites y grafo. En esta máquina sí se creó la
unión de directorio y no se recorrió; también se creó el enlace simbólico de
archivo y se rechazó el que apuntaba fuera de la raíz de búsqueda. El log
`integration-external-sources-test.log` conserva los comandos y diagnósticos.

`integration-external-sources-unit.cmd` ejecutó dos pruebas, `nsc.project-loader`
y `nsc.compilation-model`, ambas aprobadas; salida en el `.log` homónimo.
También pasó `conditional-unsafe-coherence-after.cmd` para el prototipo local
de Arc. No se ejecutó la batería completa ni el ejecutable separado del
compiler-suite. El proyecto histórico `cases/102-outside/OutsideProject.nsproj`
se conserva, pero `cases/Program.ns` no está presente; las pruebas anteriores
reproducen las mismas formas de ruta con archivos temporales, no demuestran la
ejecución de esa fuente histórica ausente.
