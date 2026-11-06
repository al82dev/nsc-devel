# Controles del productor 1.43

La primera ejecución agrupada compiló, pero tres controles intentaban decodificar
como 1.42 el catálogo que el productor ya emite como 1.43. Los controles usan
ahora el lector versionado y conservan la versión del productor al verificar el
cierre. Falta reconstruir incrementalmente y repetir la suite y el consumidor.
