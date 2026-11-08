# T08: validación agrupada de procedencia y controles

Revisión de código: `b7e02c6c4`. Compilación incremental de `nsc` y
`nsc_cpp_c_identity_tests`: exit 0. Herramientas bajo `A:/al82dev-clang`.

- `pruebas-tanda-08-identidad-c/validar.cmd`: exit 0. Incluye restauración,
  driver, matriz de typedefs, records precompilados y catálogos históricos.
- `pruebas-tanda-08-nombres-metadata/validar-referenciados.cmd`: exit 0,
  consumidor con y sin encabezado en O0–O3 y dos controles negativos.
- `pruebas-tanda-08-anonimos-c/validar.cmd`: exit 0 en la construcción previa
  a los ajustes de los controles; no equivale a una validación integral.

El caso negativo de enum conserva una limitación: su rechazo identifica `NMood`,
pero no aísla el valor del enumerador como causa. Falta una prueba independiente
de procedencia y contenido, además del gate completo de la tanda 8. No se cierra
T08-002 ni se certifica la compatibilidad general por estos resultados.
