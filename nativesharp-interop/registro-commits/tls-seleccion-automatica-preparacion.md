# Selección conservadora de LLD por TLS en bibliotecas

La reproducción sin `--linker` ni `NSC_LINKER` de `CTlsPrerequisite` falló
con NSLink en `NSL1023`; con LLD explícito pasa. El header solo declara
`verify_tls_isolation`, mientras `llvm-readobj` muestra una sección `.tls$`
y `llvm-nm` muestra `_tls_index` indefinido en el objeto de su `api.lib`.
Por eso inspeccionar solo el header perdería este caso.

Se prepara una detección positiva conservadora basada en el lector LLVM ya
existente para símbolos COFF indefinidos. Se respetarán una opción de línea de
comandos, `NSC_LINKER` y un sidecar de proyecto; no se modificará NSLink. Una
coincidencia en un miembro no extraído puede seleccionar LLD de más y una
lectura negativa o no compatible no certifica ausencia de TLS. La regla no
reemplaza el análisis de alcanzabilidad ni cierra la tanda. El driver consulta
las referencias estáticas C/C++ y NativeSharp del grafo y selecciona LLD en
ese caso positivo, únicamente para ejecutables o DLL sin opción, variable ni
sidecar explícitos. Si la inspección falla o no encuentra el símbolo,
conserva la selección anterior. La construcción incremental y la matriz
descrita en `tls-seleccion-automatica-matriz.md` pasaron. No se afirma
cobertura completa de TLS ni cierre de la tanda.
