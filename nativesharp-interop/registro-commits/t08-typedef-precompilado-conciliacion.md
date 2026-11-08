# Conciliación de typedef C hacia un record precompilado

El control equivalente de `t08-typedef-precompilado-encabezados.md` fallaba:
el alias C incorporaba el nombre del encabezado en su USR y la conciliación de
records requería el mismo archivo físico incluso para definiciones completas iguales.

En `QualifiedTypeNames.cpp`, para aliases C de archivo con igual nombre se admite
la diferencia de USR solo si sus records ya fueron conciliados. En la comparación
de clausura de records C, dos archivos físicos distintos se cotejan por su contrato
completo únicamente si ambas fuentes siguen presentes y sus hashes coinciden. La
comparación original de procedencia y el rechazo de contratos distintos se conservan.
No cambia el formato NSMETA.

`typedefs/precompiled-conflict/validar.cmd` pasó en O0: generó la biblioteca,
compiló y ejecutó el consumidor con el mismo encabezado y con otro equivalente,
y rechazó tanto el record incompatible como la fuente modificada después de
generar la biblioteca. `records-precompiled/validar.cmd` pasó sus
controles de lectura y escritura en O0, O1, O2 y O3. Se compiló incrementalmente
NSC en A:; el compilador reportó dos advertencias en inicializadores existentes
de `CppCRecordMetadataCodec.cpp`. La batería completa de la tanda no se ejecutó.
