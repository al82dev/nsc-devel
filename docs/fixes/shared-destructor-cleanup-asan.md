# Limpieza excepcional de controles compartidos con ASan

La prueba dirigida `shared_destructor_weak_cleanup.py` conserva el escenario en que el destructor del último fuerte lanza: para controles local y atómico y cargas clase y struct, verifica la excepción, un destructor, conteo fuerte cero y liberación del último débil. La variante opcional `nsc.sdk.shared-destructor-weak-cleanup-asan` usa el mismo escenario y además rechaza diagnósticos AddressSanitizer. Compila cinco archivos mínimos del SDK desde fuentes en un proyecto temporal; no instrumenta ni valida Core binario.

ASan se solicita mediante `NSC_TEST_ASAN_CLANG` y `NSC_TEST_ASAN_RUNTIME`; si falta alguno, el ejecutor devuelve 77 y CTest informa omisión. La prueba fuerza `detect_leaks=0`, por lo que no demuestra ausencia de fugas. Tampoco demuestra la seguridad general de todas las familias públicas ni de rutas distintas de las ensayadas. El enlace usa las dependencias ASan configuradas por el entorno.

El 11 de noviembre, el ejecutor actualizado pasó ocho combinaciones ASan y las mismas ocho en modo normal: controles local/atómico, clase/struct y O0/O2, todos con salida 42. Los logs son `A:/nsc-agent3-evidencia/hybrid/shared-destructor-permanent-asan.log` y `shared-destructor-permanent-normal.log`; los tiempos de las invocaciones fueron aproximadamente 8,35 y 2,07 segundos, respectivamente. El SHA-256 del compilador fue `15b3e955f7eab1b471bdd2ecdfc82a35b3b1bd65239ffbd5ddd4d91f2cc36c26` antes y después de ambas. Sin las variables ASan, el control devolvió 77 y explicó la omisión (`shared-destructor-permanent-skip.log`), sin invocar el compilador. Los resultados usan un árbol con cambios locales y no sustituyen otras pruebas de la campaña.

El guion de esa ejecución consultó la ubicación física de `nslink` mediante
`where`, una operación que los lineamientos no permiten; la ejecución de la
prueba configuró el enlazador por nombre. Posteriormente CTest informó 1/1 y
las ocho salidas 42 (`A:/nsc-fixes-evidencia/shared-cleanup-asan-registered-20261111.log`),
pero invocó CMake y Python por rutas absolutas de su configuración sin una
autorización específica. Esa ejecución CTest se conserva como evidencia no
conforme y no se cuenta como validación conforme. Para las pruebas siguientes
se invocarán las herramientas por nombre, con las excepciones expresamente
autorizadas por el usuario.

La prueba se repitió después directamente desde el árbol principal con Python
por nombre, `nslink.exe` por nombre y las excepciones de compilador y LLVM
autorizadas. Las ocho combinaciones devolvieron 42; comando y salida:
`A:/nsc-fixes-evidencia/shared-cleanup-asan-direct-20261111.cmd` y `.log`.
Se mantienen los límites de instrumentación y de `detect_leaks=0` anteriores.

Corrección: los ejecutores anteriores iniciaban sus procesos hijos desde el
temporal y las primeras invocaciones del agente desde `A:/`. Esas mediciones,
incluida la repetición directa arriba, se conservan como no conformes. Tras
corregir el ejecutor para usar la raíz de su repositorio, se repitieron los
ocho controles ASan y los ocho normales con salida 42; comandos y resultados
en `A:/nsc-fixes-evidencia/repo-cwd-corrections-20261112.json` y logs
`repo-cwd-correction-weak-{0,1}-20261112.log`.
