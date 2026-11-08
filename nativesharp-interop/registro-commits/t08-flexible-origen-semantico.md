# T08: conservar el origen del miembro flexible importado

El tipo `T[*]` también existe para almacenamiento NativeSharp. Distinguir una
conversión de miembro importado solo por ese tipo cambiaría reglas de ambos.

El análisis de miembro identifica un arreglo flexible C/C++ importado y copia
ese hecho al binding de la expresión. Aún no permite la conversión ni modifica
el lowering; falta usar y validar el dato en todos los contextos necesarios.
