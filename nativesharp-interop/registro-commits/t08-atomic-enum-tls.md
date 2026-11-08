# Tanda 8: global C enum atómica TLS

El fixture se ejecuta tanto como global normal como `_Thread_local`, con los
mismos accesos cruzados C/NativeSharp. Para cada modo comprueba en el IR de
Clang y NSC las cargas y escrituras `atomic i32 seq_cst` alineadas a 4; en TLS
también exige la declaración `thread_local`. Ambos modos pasaron O0–O3.

Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-26166-8473` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-26251-25787`. El control no
prueba aislamiento entre hilos ni los otros tamaños de enum.
