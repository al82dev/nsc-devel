# T08-015: identificación MIR de enum volatile

Un elemento indexado conserva el ancho candidato a atomicidad solo si el modo
MSVolatile está acreditado y su identidad corresponde a un enum importado con
layout coincidente. La alineación real de la dirección sigue pendiente de
comprobación en el emisor. No se modifica todavía la instrucción generada.
