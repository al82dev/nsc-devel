# Tipos enum C++ en namespaces

Una declaración local `Space.Mode value` no reconocía el enum C++ dentro del
namespace. Reproducción: `A:/temp/t10-enum-namespace-sonda/tipo.log`.
El inventario ahora construye la identidad con sus segmentos de namespace,
conserva el USR de Clang y separa los candidatos con nombres calificados
distintos. Un ámbito que no forma una identidad válida no se publica.

`pruebas-tanda-10-enum-cpp-namespace/validar.py` compara Clang 23.1.0 y
NativeSharp en O0–O3 con `First::Mode` y `Second::Mode`, ambos no scoped y con
base `int`. La conversión explícita y ejecución pasan, mientras que la
asignación entre los dos tipos se rechaza. Un namespace anónimo adicional en
el encabezado no impide compilar la muestra. Registros:
`A:/temp/nsc-t10-enum-cpp-namespace-tdmdgg6o`. La regresión de enum global y
C también pasó: `A:/temp/nsc-t10-enum-cpp-global-cl2_c82a`.

La prueba no acredita enumeradores calificados, campos, paso por copia o
precompilación de enums en namespace. T10-002 y T09-003 permanecen abiertos.
