# Referencias de proyecto y encabezados C con rutas Unicode

Las rutas de `ProjectReference`, el encabezado de `CppLibraryReference` y sus
directorios de inclusión se interpretan ahora como UTF-8, igual que las rutas
de bibliotecas. El diagnóstico del directorio conserva ese texto. Antes, una
referencia a `child-Δ.nsproj` se rechazaba como inexistente en esta máquina.

La prueba `rutas-unicode/validar.py` contrastó con `os.path.samefile` las
parejas Δ/δ, Ж/ж y Ā/ā. Para cada una verificó bibliotecas duplicadas, fuentes
que se cargan una vez, proyectos duplicados y proyecto individual. También
compiló y ejecutó en O0 una función C de un encabezado y un directorio de
inclusión con esas rutas. La prueba terminó con código 0, igual que la regresión
de caché C directa en O0; la construcción fue incremental.

No se han certificado todas las equivalencias Unicode ni todas las formas de
ruta; el control de la tanda continúa abierto.
