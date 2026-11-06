# Alcance: funciones C/C++ globales dentro de namespaces

Se conserva la prioridad del namespace léxico y de los namespaces importados.
Si ninguno aporta candidatos, una función C/C++ importada del ámbito global
puede atender la llamada. Se comprueban la matriz y las sombras; no se cambia
el tratamiento de funciones globales NativeSharp.
