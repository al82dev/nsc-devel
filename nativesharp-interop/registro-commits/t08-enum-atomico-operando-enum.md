# Tanda 8: operando enum en actualización atómica

Una asignación compuesta sobre una global atómica de enum C puede recibir otro
enum C como operando derecho. El análisis comprueba su origen importado y el
backend usa su tipo entero subyacente para las conversiones; la actualización
del destino sigue siendo `cmpxchg seq_cst`.

La prueba `state += state` se comparó con C y pasó O0–O3 con global y TLS,
junto con los casos previos del fixture. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-enum-global-32527-7616` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-32527-7616`. No acredita todas
las combinaciones de enums y operadores ni cierra la tanda.
