# Pruebas de integración registradas

Había 162 scripts en `tests/integration` que no figuraban en `CMakeLists.txt` ni en `cmake/*.cmake`.
Se ejecutaron todos con el compilador actual: 161 en la primera pasada y 12 repetidos con los
argumentos corregidos. Se registraron 155, y otros siete quedan fuera por los motivos que se dan
abajo.

## Cómo se registran

- `cmake/RunIntegrationScript.cmake` ejecuta el script desde la raíz de fuentes. Los argumentos van
  en `SCRIPT_ARGUMENTS`, separados por `|`, y `@WORK@` nombra un directorio nuevo por prueba. Ese
  directorio se borra antes de cada ejecución y se vuelve a crear con `PRECREATE`, porque casi todos
  los scripts rechazan un directorio de salida que ya existe.
- `nsc_add_integration_script(<script> [PRECREATE] [LIBRARIES] ARGUMENTS ... [ENVIRONMENT ...])`
  registra `nsc.integration.<script>` con el entorno del runtime de Windows y un límite de 1800 s.
- `LIBRARIES` exige el fixture `nsc.fixture.integration-libraries`
  (`cmake/BuildIntegrationLibraries.cmake`). Ese fixture compila una sola vez, a partir de copias,
  NativeSharp.Core y NativeSharp.Core.Collections.Generic como bibliotecas con su NSMETA.
- ASan usa `NSC_TEST_CLANG_EXECUTABLE` y el `clang_rt.asan_dynamic-x86_64.dll` de su árbol.
- `selfhost_metadata_content.py` valida el compilador autoalojado y se añadió a
  `nsc.selfhost.*`.
- `sort_with_binary_original.py` y `operator_in_temporary_binary.py` comprueban ejemplos originales
  del corpus de referencia (`NSC_REFERENCE_EXAMPLES`, por defecto
  `A:/native-sharp-reference/code-examples`). Solo se registran si ese corpus existe.

## Expectativas desactualizadas corregidas

- `decimal_literal_wide.py`: el analizador rechaza ahora el literal de 2^96 con "invalid decimal
  literal" antes del lowering; la prueba acepta también ese mensaje.
- `shared_binary_owner_empty_constructor.py`: desde NSC-NEW-012
  (`constructor-campo-valor-por-defecto.md`), un campo de valor con destructor que conserva su valor
  por defecto vive desde el inicio, y el constructor que falla lo libera una vez. La prueba esperaba
  cero liberaciones. Ahora espera una.
- `shared_binary_pattern_field.py`, casos `indexed_parameter_array_delete_buffer` y `_alias`: el
  `delete[]` de un arreglo parámetro mientras vive la vista de un `match` sobre uno de sus elementos
  se aceptaba. Es una regresión de `90aeb4e5e`: la regla de vida juzgaba por usos posteriores de su
  nombre al temporal del match (`$__nsc_match_…`), que se lee a través de sus bindings. Los
  temporales del compilador ya no se descartan por esa regla.

## Fuera del registro

- `isolated_a_runner.py`: módulo auxiliar importado por otros scripts.
- `async_yield_object_identity_probe.py`: observación exploratoria, sin carácter de compuerta
  (lo dice su docstring).
- `vistas_binary_tuple_modes_probe.py`: está ligado al compilador de otra campaña
  (`A:/nsc-vistas-evidencia/build/bin/nsc.exe`) y rechaza cualquier otro.
- `operator_in_provenance_vistas.py`: reproducción conservada de un problema abierto
  (`operator-in-provenance-vistas-pendiente.md`). Su propio documento pide no convertirla en éxito.
- Tres scripts reproducen defectos abiertos y quedan como NSC-NEW-024 a NSC-NEW-026:
  `async_finally_exception_replacement.py` (un `try/catch` dentro de `finally` hace fallar a LLVM:
  "Cleanup funclets for the SEH personality cannot contain exceptional actions"),
  `shared_async_finally_owner.py` (una llamada que puede lanzar en el `finally` de un async, mismo
  error) y `async_namespace_generator.py` (`yield break` desde un `catch` con cleanup que lanza:
  "structured exit across an active cleanup funclet").

## Validación

`ctest -R "nsc[.]fixture|nsc[.]integration[.]|selfhost[.]metadata-content"` con el compilador de esta
rama: el fixture, los 155 scripts registrados y `nsc.selfhost.metadata-content` aprueban.
