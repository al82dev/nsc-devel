# T08-003: validar objetos nativos al aceptar un sidecar

Un sidecar con huella coherente puede contener un objeto inválido. Antes de
aceptarlo, el driver pide al backend ThinLTO que lo lea con LLVM y verifique
que sea un objeto COFF AMD64 con secciones. Si no lo es, el sidecar se rechaza
y se conserva la biblioteca nativa mediante el fallback existente.

La ruta sin el backend ThinLTO informa que no está disponible. Se validará el
control de corrupción autenticada y las regresiones agrupadas; esta comprobación
no demuestra equivalencia semántica entre el objeto y su biblioteca.
