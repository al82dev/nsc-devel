# Enumeradores C++ calificados por namespace

La expresión `Space.Chosen` no se resolvía aunque el tipo enum ya estuviera
disponible. La proyección busca cada enumerador en el namespace solicitado,
conserva la identidad del tipo que Clang observó y evita colisiones de
almacenamiento entre namespaces. Al usar solo el valor, también se selecciona
su tipo como dependencia sin exigir una declaración local artificial.

`pruebas-tanda-10-enum-cpp-namespace/validar.py` ejecutó en O0–O3 los valores
homónimos `First.Value = 10` y `Second.Value = 20`, con y sin declaraciones
explícitas de sus tipos, contrastados con Clang 23.1.0. Se rechaza asignar
`Second.Value` a `First.Mode`. Registros:
`A:/temp/nsc-t10-enum-cpp-namespace-0dwnhrwg`. También pasaron la regresión de
enum global y C (`A:/temp/nsc-t10-enum-cpp-global-jh534_a1`) y las 16
combinaciones precompiladas del enum global
(`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-51ggwvt5`).

Solo se acreditan enums no scoped, nombrados, de base `int`, en esos
namespaces. Sus campos, ABI y uso precompilado no están acreditados. T10-002
y T09-003 siguen abiertos.
