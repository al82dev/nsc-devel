# Comprobación de símbolos LLVM en accesos TLS volatile

Los ejecutables TLS pasaron O0–O3, pero el verificador exigía una coma justo
después del símbolo. LLVM coloca el ordenamiento `acquire` o `release` antes
de la coma en accesos atómicos. El verificador acepta espacio o coma como fin
de símbolo, conservando la comprobación exacta del nombre y de `volatile`.
Pendiente repetir la validación dirigida.
