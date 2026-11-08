# Tanda 8: control de IR de enum atómico

El fixture compara las cargas y escrituras de la misma global C entre el IR de
Clang y el de NSC. Comprueba que ambas sean atómicas secuencialmente consistentes
y estén alineadas a cuatro bytes. Además ejecuta el consumidor en O0–O3.
La evidencia de esta ejecución está en
`A:\nsc-interop-build\tanda08-atomic-enum-25843-25725`.
