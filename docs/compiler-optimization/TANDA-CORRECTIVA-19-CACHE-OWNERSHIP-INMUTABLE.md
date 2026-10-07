# Tanda correctiva 19: cache de ownership inmutable

## SCOPE LOCK

### Objetivo

Eliminar la escritura concurrente sobre la cache de ownership transitivo compartida por los
shards de method bodies. El resultado semantico debe conservarse: un `struct` posee storage si
contiene directamente un campo `owned` o si contiene por valor otro `struct` que lo posee.
Las consultas realizadas durante el analisis paralelo deben ser de solo lectura.

### Archivos autorizados

- `include/nativesharp/Frontend/ProjectMemberModel.h`
- `src/Frontend/ProjectMemberModel.cpp`
- `tests/unit/ProjectMemberOwnershipCacheTests.cpp` (nuevo)
- `CMakeLists.txt`
- `docs/compiler-optimization/TANDA-CORRECTIVA-19-CACHE-OWNERSHIP-INMUTABLE.md`

### Archivos y superficies protegidas

- Todas las pruebas, fixtures y proyectos historicos existentes.
- Parser, semantica de expresiones, backend LLVM, NSMETA, runtime y bibliotecas.
- NSLink, nsdbg, nsasm, bindings, IDE, LSP y VS Code.
- `C:\native-sharp` y cualquier otra ruta de `C:`.
- `_tmp_strip_saru_exceptions.py`.

### Implementacion permitida

1. Construir el cierre transitivo despues de recolectar todos los miembros y antes de validarlo.
2. Usar un worklist iterativo y dependencias inversas; no usar recursion.
3. Publicar la cache completa como datos inmutables para el resto de la compilacion.
4. Agregar una prueba nueva que cubra ownership directo, transitivo, negativo y lecturas
   concurrentes repetidas.

### Implementacion prohibida

- Cambiar diagnosticos, reglas de ownership o expectativas historicas.
- Introducir compatibilidad por version de lenguaje.
- Ejecutar baseline, minibaseline u optimizaciones O1/O2/O3.
- Instalar o copiar artefactos fuera de `A:\nsc-devel`.

### Validacion autorizada

- Compilar exclusivamente el nuevo ejecutable de prueba.
- Ejecutar directamente el nuevo ejecutable de prueba.
- Revisar `git diff --check` y el diff del commit.

## Estado

LISTA.

## Evidencia

- El target nuevo compilo correctamente.
- La primera ejecucion detecto que el fixture consultaba identidades de declaracion sin el prefijo
  canonico `type:` exigido por `typeOrdinalForKey`; la implementacion no se modifico.
- La correccion del fixture se registro antes de repetir la prueba puntual.
- `cmake --build A:\nsc-devel\build-test-nslink --target
  nsc_project_member_ownership_cache_tests --parallel 12`: correcto.
- `A:\nsc-devel\build-test-nslink\tests\nsc_project_member_ownership_cache_tests.exe`:
  `PASS: 8 checks`.
- No se ejecuto CTest, baseline, minibaseline ni una optimizacion distinta de O0 para productos
  NativeSharp.
