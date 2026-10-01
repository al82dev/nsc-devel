# Tanda correctiva G00: expectativas MIR y NSMETA vigentes

## Problema

Once pruebas funcionales verificaban correctamente el contenido de MIR o NSMETA, pero ademas
exigian revisiones antiguas escritas a mano: MIR 1.14 o NSMETA 1.22. El compilador publica MIR
1.21 y NSMETA 1.25, por lo que esos literales hacian fallar pruebas sin que la funcionalidad
evaluada estuviera rota.

El caso de `&bitfield` tenia una segunda diferencia: el diagnostico productivo era correcto pero
la frase contractual `bit fields have no address` quedaba separada por texto adicional y no
coincidia con la prueba causal.

## Cambios

- Nueve pruebas MIR derivan `schema=<major>.<minor>` de
  `kProjectOopMirSchemaMajor/Minor`.
- Tres pruebas NSMETA comparan contra `kMetadataFormatMinor`.
- Los textos descriptivos dejan de afirmar que la revision historica es la revision actual.
- El diagnostico de address-of sobre bitfields comienza con `bit fields have no address` y
  conserva la explicacion sobre storage direccionable y temporales.

Archivos cubiertos:

- integrated assembly;
- propagacion `noheap`;
- escape analysis de `stack class`;
- acceso a bitfields;
- vectorcall;
- namespace state y namespace function metadata;
- contratos MIR 395, 396, 397 y 398.

No se modificaron los formatos MIR/NSMETA ni sus valores canonicos; se corrigieron consumidores
de prueba obsoletos y un diagnostico.

## Mejora

Las pruebas siguen exigiendo la version vigente exacta, pero la obtienen de la misma fuente que
el productor. Los incrementos de esquema ya no generan una cascada artificial de fallos en
funcionalidades independientes.

## Validacion especifica

- Target `nsc` construido en `A:\nsc-devel\build-clang`.
- Namespace state: 24/24.
- Namespace function metadata: 9/9.
- Integrated assembly MIR: 22/22.
- Noheap: 25/25.
- Stack class: 26/26.
- Bitfields: 15/15.
- Vectorcall: 7/7.
- MIR layout/ABI: 7/7.
- MIR ownership/cleanup: 6/6.
- MIR exception flow: 6/6.
- MIR generics/interfaces: 7/7.
- Total: 154/154 verificaciones dirigidas.
- Temporales y evidencia dirigidos a A: y eliminados al terminar.
- No se ejecuto CTest general ni el baseline.
