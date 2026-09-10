# PDB-P6: handoff CodeView al linker / CodeView linker handoff

## Resultado del productor / Producer outcome

El gate compila ahora objetos completos O0 y O2 además de los ejecutables. Antes
de invocar al linker comprueba que cada objeto contiene:

- `.debug$S` para símbolos, líneas, checksums y rangos;
- `.debug$T` para TPI/IPI y tipos;
- relocations `IMAGE_REL_AMD64_SECREL` y `IMAGE_REL_AMD64_SECTION` sobre
  `.debug$S`;
- registros de compilación, procedimientos, archivos y rangos de variables.

No existe un sidecar privado ni información que lld-link agregue por fuera del
objeto. El mismo gate completo puede ejecutarse seleccionando otro PE linker con
`NSC_LLD_LINK`; por eso la futura implementación NSL-30 recibe un fixture y un
oráculo semántico, no sólo archivos de muestra.

The gate now compiles full O0 and O2 objects in addition to executables. Before
invoking the linker it verifies `.debug$S`, `.debug$T`, the section-relative
relocations, and the standard compilation, procedure, file, and variable-range
records. The same complete gate can select another PE linker through
`NSC_LLD_LINK`.

## Medición de NSLink / NSLink measurement

El NSLink 0.1.0 disponible el 9 de septiembre de 2026 acepta
`--debug:codeview` para recolectar contribuciones, pero no escribe PDB. Una
compilación `-g` falla explícitamente con:

`NSL1057: unknown command line switch: '/pdb:...pdb'`

Esto coincide con su documentación: NSL-30 sigue pendiente. Por diseño no se
implementa un escritor PDB dentro de NSC ni se declara una paridad falsa. P6
cierra el handoff del productor; GUID/age, MSF, DBI, TPI/IPI finales, publics,
módulos y RSDS continúan siendo responsabilidad del linker.

## Criterio de reapertura / Re-entry criterion

Cuando NSLink acepte `/debug` y `/pdb:<ruta>` y produzca EXE/PDB, se ejecuta este
mismo gate con `NSC_LLD_LINK` apuntando al nuevo binario. La paridad queda cerrada
sólo si ambos ejecutables terminan en 42, `llvm-pdbutil` acepta ambos PDB y el
RSDS de cada PE coincide con su PDB.

## Commit

`Cierra el handoff CodeView al linker / Close the CodeView linker handoff`
