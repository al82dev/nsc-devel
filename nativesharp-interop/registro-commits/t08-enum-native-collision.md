# Tanda 08: colisión diferida de enum con otro tipo

La observación conserva las identidades de enums que colisionan con un tipo
NativeSharp o record. Si la búsqueda posterior usa una de esas identidades,
se devuelve un error en lugar de elegir silenciosamente un tipo; colisiones no
usadas no detienen la observación. La implementación requiere validación
agrupada, incluida la interacción con aliases y expresiones.
