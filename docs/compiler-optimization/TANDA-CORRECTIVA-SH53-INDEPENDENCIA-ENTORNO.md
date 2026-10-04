# Correctiva SH53: independencia del entorno de construcción

## Problema

`nsc.selfhost.distribution-final` comparaba rutas, versiones y SHA-256 exactos de Python,
CMake y Ninja contra una máquina anterior. Un checkout limpio podía construir correctamente
el compilador y aun así fallar SH53 por usar otra instalación válida de esas herramientas.

## Cambio

- SH53 dejó de ejecutar la validación del `bootstrap.lock`.
- Python, CMake, Ninja, clang-cl y lld-link dejaron de formar parte de los artefactos exigidos
  por la prueba de distribución.
- El reporte SH53 ya no publica hashes ni estados de esas herramientas.
- La recuperación conserva la verificación de los binarios NativeSharp, instalación,
  manifiesto, coexistencia, manipulación/rollback y pruebas funcionales del compilador.
- La documentación declara expresamente que el entorno usado para construir es decisión del
  operador y no forma parte del producto certificado.

## Archivos

- `CMakeLists.txt`
- `cmake/VerifyNativeSharpDistributionFinal.cmake`
- `selfhost/bootstrap/verify_distribution_final.py`
- `selfhost/contracts/distribution-final.contract.json`
- `selfhost/bootstrap/recovery.md`
- `selfhost/bootstrap/path-configuration.md`
- `tests/selfhost/test_bootstrap_infrastructure.py`

## Mejora

El mismo código fuente y los mismos binarios NativeSharp pueden aprobar las pruebas en otra
PC sin reproducir versiones o hashes privados de Python, CMake o Ninja. La prueba vuelve a
medir el compilador y su distribución, no la máquina que los construyó.

## Validación puntual

Se valida la sintaxis del verificador y se comprueba que el registro SH53 ya no transmite ni
exige `BOOTSTRAP_LOCK`, `NINJA_EXECUTABLE`, `CLANG_CL_EXECUTABLE` o `LLD_LINK_EXECUTABLE`.
No se ejecuta baseline ni matriz general.
