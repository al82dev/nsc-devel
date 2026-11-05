# Tanda 08: clave valida para cache C3

La cache requiere claves SHA-256 hexadecimales de 64 caracteres. Se genera la
clave versionada C3 mediante `templateCacheKey`, en lugar de pasar el prefijo
literal, evitando que todos los accesos se rechacen como clave invalida. Falta
verificar los aciertos y la invalidacion en la validacion agrupada.
