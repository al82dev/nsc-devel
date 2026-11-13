# Enumeradores de enum class C++

Aunque el tipo `Space.Mode` ya se importaba, `Space.Mode.Value` fallaba como
miembro inexistente: `A:/temp/t10-enum-scoped-valor-sonda/resultado.log`.
El inventario proyecta los valores y sus identidades de Clang como miembros
constantes del tipo enum. El modelo semántico los incorpora al tipo importado
sintético, no al namespace exterior.

`pruebas-tanda-10-enum-cpp-scoped/validar.py` comparó con Clang 23.1.0 y
ejecutó en O0–O3 enumeradores positivos y negativos, incluidos dos enums con
un miembro homónimo. También ejecutó referencias sin declarar una variable del
tipo y comprobó rechazos de ámbito incorrecto, conversión implícita a `int` y
asignación entre enums distintos. Registros:
`A:/temp/nsc-t10-enum-cpp-scoped-c3hsynci`.

Pasaron las regresiones C y de enum global
(`A:/temp/nsc-t10-enum-cpp-global-i588ecul`) y los enums de namespace no
scoped (`A:/temp/nsc-t10-enum-cpp-namespace-vo3zqwfs`). Esta evidencia cubre
`enum class` de base `int`; campos, ABI y precompilación siguen pendientes.
T10-002 y T09-003 permanecen abiertos.
