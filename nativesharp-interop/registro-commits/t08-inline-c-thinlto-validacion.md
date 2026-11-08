# T08-003: validación de inline C y ThinLTO

La construcción incremental de `nsc` terminó correctamente con build
`be32a31d134f5d52`. Ejecutado mediante un llamador CMD con `call`, el control
ThinLTO pasó O0–O3 para el ejecutable directo y dos productores precompilados;
el consumidor comprobó los estados independientes de las funciones y globales
internas de cada productor.

En O1–O3 el control exige un índice ThinLTO y que el consumidor conserve las
bibliotecas nativas. Antes de cada producción crea sidecars obsoletos y verifica
que no sobrevivan. En O0 NSC advierte que no ejecuta la optimización ThinLTO.
La matriz sin ThinLTO también llegó al resultado correcto en O0–O3.

Este resultado cubre el fallback de archivo nativo. No acredita la optimización
entre bibliotecas que contienen objetos C generados, ni todas las referencias
de objetos nativos hacia símbolos NativeSharp. La tanda 8 sigue abierta.
