# Contrato de escape desconocido en biblioteca NativeSharp

Un método precompilado que recibe un puntero puede publicar el contrato de
escape `Unknown`. La reproducción del fixture usa un método que conserva el
puntero y una copia de metadata que marca ese contrato como desconocido. Antes
del ajuste el consumidor que pasaba la dirección de un local se aceptó
(`A:/nsc-fixes-evidencia/unknown-native-escape-before.log`). El ejemplo se
analizó, nunca se ejecutó. Una función precompilada de espacio de nombres
reprodujo la misma aceptación (`unknown-native-function-before.log`).

`validateEscapeCall` ahora trata `Unknown` de un método o función NativeSharp
precompilados como insuficiente para pasar almacenamiento del marco: reutiliza
el análisis de procedencia de `Global` y emite un diagnóstico de contrato
desconocido. Para funciones consulta el símbolo original, porque su adaptador
no conserva la marca de precompilación. La regla excluye explícitamente imports
C y no modifica `NoEscape`, `Return` ni llamadas fuente. Almacenamiento global
sigue admitido. Una biblioteca legítima sin garantía puede requerir actualizar
su contrato o evitar pasar una dirección local; esta es la consecuencia
conservadora elegida. No se añadió política ni número de versión.

Tras compilaciones incrementales, la prueba
`nsc.ns3.t06.escape-summaries` pasó 16 comprobaciones
(`A:/nsc-fixes-evidencia/unknown-native-alias-final.log`): incluye métodos y
funciones con contrato desconocido, dirección local directa, alias local,
direcciones globales y los controles existentes de `NoEscape` y `Global`.
Los controles dirigidos de Rc/Arc y Core binario pasaron
(`unknown-native-function-pointers.log`, `unknown-native-function-binary.log`),
al igual que dos pruebas existentes de vida y escape
(`unknown-native-function-controls.log`, 2/2).

El cambio se probó con el árbol conjunto. La evidencia cubre
las procedencias señaladas, no toda dirección desconocida ni la veracidad de un
contrato antiguo que afirme incorrectamente `NoEscape`. Las vistas compuestas y
su materialización binaria siguen abiertas; este ajuste no las certifica.
