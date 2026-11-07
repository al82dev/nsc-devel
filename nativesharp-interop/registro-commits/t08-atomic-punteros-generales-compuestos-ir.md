# T08-004: verificar operaciones compuestas en IR

El control admite escrituras atómicas o actualizaciones atomicrmw de puntero,
con orden secuencialmente consistente y alineación ocho. C usa asignación y
NativeSharp usa aritmética compuesta; ambos conservan la lectura atómica.
