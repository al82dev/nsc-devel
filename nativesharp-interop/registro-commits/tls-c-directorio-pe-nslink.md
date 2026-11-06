# TLS C estático: directorio PE emitido por NSLink

## Evidencia

El fixture `pruebas-tanda-08-tls-c` no enlazó: NSLink reportó `NSL1020`
para `_tls_index`, referenciado desde `api.obj`. El resultado está en
`A:/nsc-interop-build/tanda08-tls-c/link.log` y ya se había registrado en
`TANDA-08-TIPOS-C.md`.

La inspección de solo lectura de `A:/nslink-devel/src/pe/PeConstants.h`
identifica los directorios PE que NSLink enumera; no incluye TLS. En
`A:/nslink-devel/src/pe/PeImageWriter.cpp`, líneas 192–235, el escritor
asigna RVA y tamaño a importación, exportación, recursos, excepciones,
relocaciones base, depuración e IAT. Para los demás índices escribe ambos
campos a cero. En consecuencia, la implementación inspeccionada escribe en
cero la entrada del directorio TLS de la imagen.

`A:/nsc-devel/src/Linker/LldLinker.cpp`, líneas 220–255 y 518–533,
exige que `NSC_LINKER` seleccione `nslink.exe`: el nombre de la clase no
significa que se pueda seleccionar LLD como enlazador de ejecutables mediante
esa opción.

## Alcance y consecuencia

Resolver únicamente `_tls_index` no demostraría TLS estático funcional: hace
falta además que la imagen exponga un directorio TLS válido para Windows. Esta
inspección no demuestra que sea imposible resolverlo desde NSC sin cambiar
NSLink; una alternativa tendría que preservar el formato PE, relocaciones,
inicialización y comportamiento por hilo y demostrarse con pruebas.

La DLL C generada con Clang/LLD y consumida desde NativeSharp ya pasó su caso
de aislamiento entre hilos, como consta en `tls-c-dll.md`. Esa prueba no
resuelve el TLS del ejecutable enlazado por NSLink ni la importación directa de
globales TLS. No se modificaron NSLink ni LLVM y la tanda 08 continúa abierta.
