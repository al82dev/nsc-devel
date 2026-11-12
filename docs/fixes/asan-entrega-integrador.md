# Entrega para integrar el frente ASan

Rama `development-fixes-limitaciones-asan`, creada desde el WIP compartido
`d00b3a8698d7750c80887d029d64fda481d67211`. La revisión inicial del trabajo de
este chat fue `41f0a0cbb8683c89f21a504e5aa5d9e8676bdc8c` y el compilador
se construyó desde esa revisión sin cambios de producto en `src` ni `lib`.

Commits de trabajo de este chat, en orden, antes del que incluye esta entrega:

- `8e27edc3c`: ejecutores dirigidos del driver y evidencia de opciones ASan.
- `00f712121`: procedencia y consumo del Core binario aislado.
- `1e12fb3a9`: ejecutores y evidencia de runtime, heap y caso 97 original.
- El commit que contiene esta entrega agrega el contraste binario de Collections
  y el handoff del caso 100. Consultar el hash en `git log` de la rama.

Las pruebas dirigidas de driver y selección de runtime, heap integrado y ejemplo
original 97, ocho argumentos incompatibles, limpieza de arreglos, metadata de
Core y consumo Core binario pasaron en las configuraciones y con las limitaciones
de sus informes específicos. El ejemplo 99 pasó con Collections y Core binarios
en O0/O2, con y sin ASan. No se ejecutó ninguna batería completa. Las pruebas
instrumentadas usaron `detect_leaks=0`; Core y Collections no se instrumentaron.
No se acredita ausencia general de fugas ni de errores dentro de bibliotecas.

El ejemplo 100 permanece **reproducido y sin corregir** con Collections binario:
falla inferencia de `SortWith`, aunque pasa con Collections fuente. Con tipo
explícito pasa en ambos. El handoff `asan-sortwith-binario-handoff.md` conserva
controles, hipótesis aún no demostrada, rutas y pasos de diagnóstico. Este
trabajo requiere coordinación con el frente de genéricos antes de cambiar
archivos compartidos. La fuente original no se modificó.

El commit `7e00ebcb4` añade una regresión dirigida parametrizada que conserva
esa fuente. Su baseline falla actualmente en las cuatro combinaciones normal y
ASan en O0/O2; el control separado con tipo explícito pasó. El registro de la
prueba en `CMakeLists.txt` permanece a cargo del integrador. La comprobación
unitaria adicional `nsc_item389_native_sanitizer_tests` pasó 10 checks en O1;
su alcance y procedencia están en `asan-pipeline-unitario.md`.

También pasaron en O0/O2 los controles de crecimiento, reducción y liberación
del adaptador (`asan-realloc-exitoso.md`). La prueba semántica binaria en
`asan-sortwith-binario-semantica.md` verifica orden descendente, duplicados,
vacío y singleton con tipo explícito; su variante inferida sigue fallando.
Sus ejecutores nuevos tampoco se registraron en `CMakeLists.txt`. El fragmento
diagnóstico no aplicado está versionado en `asan-sortwith-diagnostico-no-aplicado.md`.

Los archivos modificados de ejecución de pruebas son
`tests/integration/address_sanitizer_driver.py`,
`asan_runtime_path_validation.py`, `address_sanitizer_heap.py`,
`address_sanitizer_integrated_runtime.py` y `asan_throwing_array_release.py`;
los nombres abreviados están dentro de `tests/integration`. Revisar conflictos
si el integrador o el frente de interoperabilidad cambian esos mismos fixtures.
No se modificaron `CMakeLists.txt`, `src/LLVM/Internal/OopLlvmImplementation.h`
ni documentos globales de campaña. Ningún cambio de código de producto fue
necesario para las rutas ASan observadas en la revisión probada.

Informes: `asan-driver-entorno.md`, `asan-integrated-runtime-cwd.md`,
`asan-sdk-binario-aislado.md` y `asan-sortwith-binario-handoff.md`. Contienen
comandos, hashes, logs en A, resultados y límites. Los ejecutores conservan el
`PATH`; una prueba se omite con código 77 cuando su precondición sobre la DLL
no se cumple, y el control de shadowing requiere un cwd autorizado y libre.

El banner del compilador observado sigue mostrando una versión histórica; se
deja como observación para el integrador porque este frente no es propietario
de ese comportamiento. No se modificaron interoperabilidad, Daikon ni LLVM.
No se hizo push, merge, rebase ni despliegue.
