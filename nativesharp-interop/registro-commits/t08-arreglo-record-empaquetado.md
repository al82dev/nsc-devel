# T08-010 y T08-016: arreglo C de records empaquetado

SCOPE LOCK: habilitar únicamente la proyección C de arreglos fijos de records
subalineados, sin modificar LLVM, NSLink ni el comportamiento de C++. El
proyector ya verifica identidad, tamaño y alineación del record en
`projectCArrayField`; la barrera anterior rechazaba esos arreglos antes de
alcanzar dicha comprobación. Ahora permite llegar a ella. La alineación y la
dirección del elemento siguen dependiendo de las reglas de acceso existentes.
La validación mixta, de IR y de direcciones queda pendiente.
