# Tanda 8: enum atómico importado de DLL C

El fixture de DLL C exporta una global `_Atomic(enum DllMode)`. Tanto el
control C como NativeSharp comprueban su valor, ejecutan `+= 2` y verifican
el resultado mediante una función de la propia DLL. El IR NativeSharp debe
contener una lectura atómica y un `cmpxchg` para esa global con `seq_cst`.

El caso pasó de O0 a O3. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-global-dll-31927-28758`. No cubre
TLS exportado por DLL ni todos los tamaños y operaciones.
