# WORKLOG CURRENT STATE — Stage 51 (cierre)

Esta ronda se construyó sobre el paquete Stage 51 previamente entregado
(`NativeSharp_Stage51_Package.zip`), en respuesta directa a la crítica detallada de Alberto
sobre ese paquete.

Key decisions:

1. El gap de `enum`/`StringComparison` se trata como bug de compilador a corregir, no como
   limitación a documentar y esquivar — instrucción explícita de Alberto.
2. El fix de enums reusa la infraestructura de Stage 37 ("static const field") en vez de
   inventar mecanismo nuevo: un enum constant se registra, materializa y lowerea exactamente
   como un `static const int` con un valor conocido en compile-time.
3. `enum` sigue restringido a declaración namespace/top-level en esta ronda — el caso anidado
   en clase es un límite de parser distinto, no tocado, y documentado como tal.
4. El diferimiento de "owned `string[]` como retorno" se re-verifica desde cero (no se asume
   de la ronda anterior) antes de decidir cómo completar Split/Partition/etc.; se encuentra un
   patrón distinto (buffer de destino como parámetro) que sí funciona y no requiere tocar el
   compilador.
5. Las fallas de la suite de regresión no se re-etiquetan como "preexistentes" sin evidencia:
   se reconstruye el `NativeSharp_Stage50_FULL.zip` certificado original en este sandbox y se
   corre su suite completa para separar fallas de entorno/toolchain de regresiones reales.
6. Ningún test se relaja para forzarlo a pasar sin causa raíz verificada contra la evidencia
   real (diagnóstico/IR emitido); el único test que sigue fallando al cierre
   (contenido de README histórico inexistente) se deja fallando explícitamente en vez de
   fabricar el archivo que buscaría.

Bugs found and fixed during implementation:

- `ProjectMemberModel::build()` no procesaba `NamedTypeDeclarationKind::Enum` en absoluto;
- `resolveTypeIdentifier` y un segundo gate en `analyzeMember` no aceptaban `Enum` como
  receptor de tipo;
- `integerWidth()`/`isSignedInteger()` en el LLVM lowerer no reconocían enum type keys,
  rompiendo el cast explícito `(int)` aun después de que el acceso calificado funcionara;
- primer intento de tipar el enum constant como `builtin:int` en vez de su propio type key
  rompía la inicialización tipada (`Mode m = Mode.Second;`), lo que a su vez requirió tres
  fixes de seguimiento para mantener vivo el camino de materialización de constante de
  Stage 37 con un type key no-numérico;
- `Driver.cpp` seguía imprimiendo "Stage 49" en `--help` y en el mensaje de línea de comandos
  inválida, sobreviviente de antes de Stage 50;
- olvido real (no de diseño): `ToLowerInvariant`/`ToUpperInvariant` planificados pero no
  escritos en el primer pase — expuesto por el propio compilador al fallar `Main.ns`;
- colisión de nombre de variable local (`parts`) entre el ejemplo de `Join` preexistente y
  los checks nuevos de `Split` agregados en `Main.ns`.

Final mechanical package data is recorded in `PACKAGE_VERIFICATION_STAGE51.md` and
`HANDOVER_STAGE51_TO_STAGE52.md`.
