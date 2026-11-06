# Puntero global C a struct const

La proyección de globales permite un nivel de puntero a struct C const con
identidad y layout verificados. La inmutabilidad del apuntado se conserva en el
contrato de estado existente. Las pruebas de escritura y conversión deben
confirmarlo; no se habilitan punteros a volatile ni const en niveles anidados.
