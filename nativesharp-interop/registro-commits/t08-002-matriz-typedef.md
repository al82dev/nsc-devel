# T08-002: matriz de typedef C equivalentes y distintos

Se añade el modo `--typedef-matrix` a `nsc_cpp_c_identity_tests` y el fixture
`pruebas-tanda-08-identidad-c/typedefs`, con doce casos (`casos.txt`). Cada caso
tiene tres proyectos: un solo encabezado, dos encabezados de nombre distinto y
dos encabezados homónimos en directorios distintos.

La sonda aplica a los USR reales de Clang una réplica de la regla actual de
alias y otra de la regla propuesta en `T08-002-PROPUESTA-TYPEDEF.md`, y las
contrasta con lo que da `nsc`. El validador compila los 36 proyectos y exige el
resultado ACTUAL de la tabla; con `NSC_T08_PROPUESTAS=1` exige el resultado PROP.
Se añaden a `validar.cmd` las subrutinas genéricas `:case` y `:case_message`.

La propuesta incorpora la matriz y la revisión contra los contratos existentes.
No se cambia la resolución compartida de nombres. Se registra H5: un struct
idéntico en dos archivos deja sus campos inaccesibles. Es un contrato del carril
de layout de records y no se modifica.
