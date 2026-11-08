# Campaña de limitaciones del compilador y SDK

Base: `152654e5a0e2d84d4c2f245ea7590488f0497430`, rama
`development-fixes-limitaciones`. Fuentes: reporte y registro detallado de
`A:/native-sharp-reference`. Compilador y runtime compilados desde la copia en
`A:/nsc-fixes-evidencia/build`; SDK de la copia en `A:/nsc-fixes-evidencia/sdk`.
Windows x64, LLVM/Clang 23.1.0, host estático y Release. Se usa el enlazador
externo `C:/native-sharp/bin/nslink.exe` como dependencia de referencia y el
runtime generado en A. El ejecutable final informa NativeSharp 3.0.0 y build
hash `c237186aba0246a4` (`final-nsc-version.log`). No se instala ni modifica
`C:/native-sharp`.

Las reproducciones son copias en `A:/nsc-fixes-evidencia/cases`; las fuentes
originales se conservan. Las copias de proyectos que usaban el SDK instalado
apuntan a las bibliotecas de la copia. Cada `result.json` registra el comando y
resultado inicial; `build.log`, `run.log` y demás salidas del directorio son
evidencia. Los resultados iniciales corresponden al compilador construido antes
de los cambios, no a la última ejecución. Las pruebas posteriores y sus límites
se detallan en los documentos de `docs/fixes` y registros de la misma carpeta.

| Caso | Resultado inicial y situación |
| --- | --- |
| Rc / WeakRc; Arc / WeakArc | `sdk-01`: nombres no encontrados. El contrato impreso por el compilador no demuestra una implementación. Pendiente decisión de API y propiedad. |
| Dictionary string AddOrUpdate | `sdk-02`: termina 3221226356; doble liberación. Corregido en `5127e4d07`, con ejecución de regresión. |
| SortWith | `sdk-03`: miembro no encontrado. Pendiente decisión de API. |
| Saturación: suma y conversiones a short y byte | `comp-01`: `-2147483648`, `-25536`, `34`; corregido para esas operaciones en `a7436961d` y `bb4c18900`. No se afirma cobertura de todas las operaciones saturadas. |
| Literales y cálculo decimal | `comp-02`: factura 101, comparación falsa, redondeo 100/100/101. `a63333883` conserva el lexema de literales y corrige la reproducción; no demuestra precisión completa de decimal ni inicializadores estáticos. |
| checked i128 | `comp-03`: desborda y continúa con wrapping. Pendiente decisión sobre el mecanismo observable de fallo. |
| División i128 | `comp-04`: enlace falla por `__divti3`. `2f1ae8d47` corrige división firmada; pruebas diferenciales, extremos y divisor cero. Resto y unsigned no están cubiertos. |
| sealed O2 | `43-dispatch`: IR optimizado conserva llamada indirecta. Pendiente decidir el objetivo de optimización. |
| Cancelación async | `comp-06`: traza 0 y salida 5. Con un avance adicional de `MoveNextAsync`, `comp-06-deferred` da traza 7 y salida 0. Pendiente decidir limpieza inmediata o diferida. |
| impl externo para struct | `68-struct`: parser rechaza el destino struct. Pendiente decisión de lenguaje. |
| Delegado genérico con retorno T | `69-delegate`: rechazo de T. `3c03a4440` corrige el patrón reportado; otras formas de retorno genérico no están demostradas. |
| Restricción entre parámetros genéricos | `69-constraint`: resolución no encuentra T. Pendiente investigación y decisión del contrato de restricciones. |
| Lambda sin captura en Main global | `69-lambda`: falla en HIR por ordinal de owner. `f8a05890f` corrige función global sin captura; cierres con captura no están cubiertos. |
| Matriz dinámica genérica decimal | `70-decimal`: falta layout del elemento. `de6415700` añade el layout en operaciones de arreglo; reproducción y regresión pasan. |
| Método genérico en matriz genérica | `70-method`: se rechaza deliberadamente como no implementado. Pendiente diseño. |
| Función genérica con dimensiones const reenviadas | `70-static`: rechazaba Rows. `690a8ffd3` corrige el caso y verifica multiplicación y argumentos inválidos. No cubre todos los ámbitos o paquetes. |
| Temporal como argumento in de operador | `70-temporary`: rechazo de argumento. Pendiente decisión de lenguaje y duración. |
| Llamada a función desde #run | `73-run`: rechazo del evaluador limitado. Pendiente decisión de alcance del evaluador. |
| Inspección general de campos | `74-fields`: ejecuta un ejemplo explícito, no un inspector universal. No se localizó API general; pendiente diseño. |
| derive Print | `75-derive`: texto `GridPoint`, sin campos. Pendiente decisión de formato. La igualdad falsa observada en la misma ejecución se corrigió de forma acotada; véase `derive-ignore-equality.md`. |
| SIMD máscara a vector, indexación y condicional | `89-mask-convert`, `89-mask-index`, `89-mask-select`: rechazos reproducidos. Pendiente definir las operaciones y representación deseadas. |
| AddressSanitizer | No se localizó opción en `nsc --help`; no se ejecutó la reproducción fuera de límites bajo ASan. Bloqueado por falta de soporte/configuración demostrada; pendiente diseño de herramienta. |
| Operadores ++ / -- | `39-increment`, `39-decrement`: parser rechaza la sobrecarga. Pendiente decisión sobre semántica y evaluación. |
| NativeSharp.Text | `text-namespace`: namespace no encontrado. Pendiente decisión de API. |
| Compile con .. | `102-outside`: proyecto rechaza rutas fuera de su raíz. Pendiente decisión de resolución de proyectos. |
| Ejemplo 66 | Antecedente ya resuelto en el ejemplo; no se atribuye un defecto vigente al compilador. |

La prueba `nsc.stage41.arrays-strings-core` inicialmente no enlazó por el codec
de snapshots; `2d6dd44ac` corrige su dependencia, y `ae22f5cdf` hace lo mismo
para siete objetivos relacionados. Una batería dirigida registró 30/33: los
fallos de constantes genéricas, paquetes y delegados se reprodujeron también
en la base independiente, con los mismos diagnósticos. Para ejecutar los
objetivos comparables se aplicaron ajustes de enlace a los tests en el CMake
del árbol base; los fuentes del compilador permanecieron en la revisión
indicada. La selección comparable de 26 pruebas existentes obtuvo 23/26.
Comandos y resultados: `baseline-suite-build.log`, `baseline-suite.log`,
`baseline-comparison-build.log` y `baseline-comparison-tests.log` en
`A:/nsc-fixes-evidencia`.

`6f3182664` corrige el reconocimiento de paquetes y valores enum; `c35229ed5`
mantiene como negativo el índice de matriz que la prueba anterior esperaba
aceptar; `c9a48e2e1` actualiza la comprobación de IR de delegados sin quitar
la verificación de sus argumentos. La batería final seleccionada pasó 36/36,
incluidas las diez regresiones nuevas (`related-tests.log` y
`related-tests.json`). Además pasaron 12/12 pruebas de análisis e IR de las
etapas 42–44 (`cli-suite.log`), dos de ellas también contadas en las 36, y
tres emisiones de objeto con salida explícita en A (`cli-objects.log`). La
primera tentativa de emisión usó una opción de caché que ese comando no admite;
se corrigió la invocación y las tres pasaron.

El análisis `clang-tidy --checks=clang-analyzer-*` de los cuatro últimos
archivos de frontend terminó con código 0; `analyze-final-*.log` incluye
advertencias de padding en encabezados. No se afirma ausencia de advertencias.
El inventario general de CTest contiene otros objetivos no ejecutados; no se
incluyen pruebas ni builds de interoperabilidad en la batería seleccionada.
No se ha ejecutado una batería general completa.

La comparación de nombres contra `development-cpp-clang` en el repositorio
original, solo en lectura, encontró estos archivos modificados en ambas ramas:

- `CMakeLists.txt`
- `include/nativesharp/Frontend/ProjectGenericModel.h`
- `src/Frontend/HIR/HirBuildCoordinator.cpp`
- `src/Frontend/HIR/HirExpressionMachine.cpp`
- `src/Frontend/MIR/Internal/MirNodeMachine.cpp`
- `src/LLVM/Internal/OopLlvmImplementation.h`
- `src/LLVM/Lowering/OopLlvmExpressionLeaves.cpp`
- `src/LLVM/Lowering/OopLlvmFieldsAndConversions.cpp`

Es coincidencia por archivo, no una predicción de conflictos de líneas. La
comparación de cambios registrados sin commit en archivos rastreados no halló
coincidencias; no evalúa archivos no rastreados. Registro `shared-files.log`.
