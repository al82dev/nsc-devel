# `nsc.debug.codeview-pdb`: nombre de depuración de arreglos (NSC-NEW-008, parcial)

La comprobación O0 de `cmake/VerifyNativeSharpDebugArtifacts.cmake` exigía un `LF_STRUCTURE` llamado
`` `builtin:int[]` ``. `debugTypeKeyDisplayName` nombra los tipos con la ortografía del lenguaje
(`string`, `Ns.T[]`), y el PDB actual contiene `` `int[]` ``; la clave interna ya no aparece. Con un
script que aplica a la salida conservada cada patrón de la comprobación, ese fue el único que
faltaba en O0. La expectativa pasa a `` `int[]` `` y O0 pasa.

Sigue abierto O2: la comprobación exige que `capturedTypeValue` tenga una ubicación de registro y
prohíbe "optimized away". El PDB actual marca ese local como "optimized away" en la copia inline de
`TypeSurface.ExerciseTypes` y en `TypeSurface.ExerciseTypes.lambda#1` (7 bytes, el valor capturado
se lee del entorno). Las demás comprobaciones O2 (constantes, `label` optimizado, rangos con huecos,
`S_DEFRANGE_REGISTER_REL`, dos rangos fragmentados y los sitios inline) pasan. No se ajustó esa
expectativa: perder la ubicación de un valor capturado es una pérdida de información de depuración y
no una diferencia de nombre.
