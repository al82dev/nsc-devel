# Caso 102: `Compile` con `..` y globs en laptop 05

## Resultado

La revisión `50c87bc38bc57a05b0851f43fe43685040245160` ya contiene la corrección
funcional de `ProjectLoader` y `GlobPattern` para fuentes externas seleccionadas
explícitamente. Las pruebas dirigidas no reprodujeron un defecto de producción.
Este lote amplía la regresión y documenta el contrato observado; no modifica el
loader, el modelo de compilación ni CMake.

La fuente histórica del ejemplo original no está completa en la evidencia
disponible. Por ello, esta validación corresponde a fixtures equivalentes y no
se afirma que se haya ejecutado el ejemplo original.

## Contrato verificado

- Una inclusión `Compile` explícita puede empezar con uno o más segmentos `..`.
- Los segmentos `..` solo son válidos antes del primer segmento con comodines.
- Las rutas absolutas, el cruce por encima de la raíz del sistema y un `**`
  incrustado en otro segmento se rechazan con diagnóstico.
- La raíz externa se obtiene del prefijo literal anterior al primer comodín. Si
  no existe, la regla no produce coincidencias; si existe pero no es directorio,
  el proyecto se rechaza.
- La búsqueda externa solo acepta archivos regulares `.ns`, no recorre `bin` u
  `obj`, no atraviesa junctions durante la enumeración y rechaza enlaces cuyo
  destino canónico sale de la raíz de búsqueda.
- Un enlace cuyo destino permanece dentro de la raíz es válido. La identidad
  canónica evita compilar dos veces la misma fuente, incluso entre elementos
  `Compile`, aliases locales, globs y enlaces.
- `Exclude` se aplica a la regla `Include` correspondiente y `Remove` al conjunto
  resultante. Ambos aceptan patrones exactos y listas separadas por `;`, y usan
  las rutas relativas al proyecto también para fuentes externas.
- Las listas vacías, las rutas absolutas y `..` después de comodines son inválidas
  tanto en `Exclude` como en `Remove`. `Remove` no puede combinarse con
  `Exclude` en el mismo elemento.
- El orden publicado de las fuentes es determinista por ruta normalizada.
- Una misma fuente externa compartida por dos proyectos del grafo se asigna una
  sola vez. Su ruta relativa también queda registrada en el bundle diagnóstico.

## Entorno aislado

- Repositorio: `A:\nsc-fixes-laptop-05`.
- Rama: `development-fixes-limitaciones-laptop-05`.
- Build principal: `A:\nsc-fixes-laptop-05-build`.
- SDK: `A:\nsc-fixes-laptop-05-sdk`.
- Temporales y cachés: `A:\nsc-fixes-laptop-05-temp` y
  `A:\nsc-fixes-laptop-05-cache`.
- Compilador probado: `A:\nsc-fixes-laptop-05-build\bin\nsc.exe`, identidad de
  build `5cdc703705cbc7da`.
- Runtime de las ejecuciones: `A:\nsc-fixes-laptop-05-build\generated\NativeSharp.Runtime.Win32.lib`.
- Linker: `A:\nslink-devel\bin\release\nslink.exe`.

`TEMP`, `TMP`, `APPDATA` y `LOCALAPPDATA` se dirigieron a las rutas aisladas de
A:. Para las pruebas que enlazan se fijaron `NSC_WINDOWS_RUNTIME_LIBRARY` y
`NSC_LINKER` a las rutas anteriores. No se modificó ni desplegó
`C:\native-sharp`, LLVM/Clang ni otra instalación funcional.

## Evidencia dirigida

1. Configuración inicial del build principal con CMake/Ninja, Clang 23.1.0,
   LLVM en `C:/clang-llvm/lib/cmake/llvm`, NSLink explícito y tests habilitados:
   completada.
2. Build inicial autorizado de los targets `nsc`,
   `nsc_project_loader_tests` y `nsc_compilation_model_tests`: 634 pasos,
   código 0.
3. `ctest -R ^nsc.project-loader$`: 1/1 aprobado.
4. `ctest -R ^nsc.compilation-model$`: 1/1 aprobado.
5. Fixture existente `tests/integration/project_external_sources.py` con el
   compilador aislado: aprobado; ejecutables O0 y O2 devolvieron 42 tanto para
   el archivo explícito como para el glob externo.
6. Fixture ampliado: aprobado. Incluyó bundle diagnóstico, grafo de dos
   proyectos, múltiples `..`, orden, exclusiones y eliminaciones exactas y en
   lista, duplicados, raíces inexistentes o no directorio, patrones inválidos,
   junction y enlaces simbólicos internos y externos.
7. `TF-COMPILATION-001` a `TF-COMPILATION-008`: 8/8 aprobados mediante un
   runner temporal aislado que compiló literalmente `TestRegistry.cpp` y
   `CompilationCore001To112Module.cpp` y los enlazó con `nsc_compilation.lib` y
   `nsc_basic.lib` del build principal. La salida se preservó en
   `A:\nsc-fixes-laptop-05-evidencia\case102-compiler-suite-directed.txt`.

No se ejecutó la batería completa.

## Compiler-suite independiente

Se intentó configurar exclusivamente el target independiente
`nsc_component_tests` en
`A:\nsc-fixes-laptop-05-build\compiler-suite`, con el compilador, NSLink,
runtime y prefijo de instalación explícitos de A:. La configuración se detuvo
antes de generar Ninja porque el validador obligatorio detectó dos fuentes
unitarias versionadas que no están registradas en el `CMakeLists.txt` principal:

- `tests/unit/view_producer_body_prototype.cpp`;
- `tests/unit/view_provenance_summary_prototype.cpp`.

El diagnóstico exacto fue
`unit-test CMake registration differs; missing=[...]; unexpected=[]`. No hay una
opción oficial para omitir el control y no existe un
`nsc_component_tests.exe` previo en este build. Por pertenecer `CMakeLists.txt`
a otro responsable, no se modificó.

Para separar el comportamiento de los casos del fallo de configuración, se
compiló fuera del árbol un runner mínimo con el registro y el módulo oficiales.
Ese runner seleccionó individualmente `TF-COMPILATION-001` a
`TF-COMPILATION-008`; los ocho aprobaron. Esta evidencia verifica las ocho
variantes del contrato de globs en el consumidor, pero no sustituye ni certifica
la generación del target monolítico `nsc_component_tests`.

La propuesta al integrador sigue siendo registrar ambas fuentes en sus targets
unitarios correspondientes y regenerar `nsc_component_tests`. Por esta causa,
la **configuración del target oficial** queda bloqueada por infraestructura
compartida; el comportamiento dirigido del caso 102 en su módulo sí quedó
verificado.

## Coordinación y límites

Se coordinaron tres revisiones separadas para rutas explícitas,
globs/exclusiones y enlaces/consumidores. Las tres convergieron en que la
corrección previa está presente y que faltaba principalmente evidencia de
regresión.

Archivos compartidos que deliberadamente no se tocaron:

- `src/Frontend/ProjectCompilation.cpp`;
- `CMakeLists.txt`;
- modelos compartidos del proyecto.

Tampoco se modificaron `src/Compilation/ProjectLoader.cpp` ni
`src/Compilation/GlobPattern.cpp`, porque las reproducciones actuales no
justificaron otro cambio funcional. Daikon, interoperabilidad C/C++,
`NativeSharp.Text`, despliegue, push e integración permanecieron fuera del
alcance.

## Incidencias de entorno no atribuibles al caso 102

- El CTest generado conserva una referencia predeterminada a un `nsc.exe` de
  instalación para otras pruebas. No se ejecutó ninguna prueba que consumiera
  esa referencia.
- La primera ejecución del fixture alcanzó correctamente el loader, pero el
  enlace buscó el runtime predeterminado en C: y falló. La repetición fijó
  `NSC_WINDOWS_RUNTIME_LIBRARY` al runtime generado en A: y aprobó. Esto no fue
  un defecto de rutas externas.
