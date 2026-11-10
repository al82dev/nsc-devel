# NSMETA: contrato vigente

Decisión del propietario del 8 de noviembre de 2026: NativeSharp tiene un solo
contrato vigente de NSMETA, el que admite el compilador actual. Los números de
formatos históricos no identifican productos, dialectos ni reglas de desarrollo.

El lenguaje crece de forma acumulativa. Una capacidad que ya era válida debe
seguir funcionando cuando se agreguen otras. El compilador debe interpretar el
contenido que conoce y diagnosticar aquello que no puede entender; una etiqueta
histórica, por sí sola, no debe decidir si una capacidad es válida.

No se mantienen versiones paralelas de NSMETA ni se exige compatibilidad entre
etiquetas antiguas. Un programa anterior que emplea capacidades que el compilador
actual sigue admitiendo debe poder seguir compilándose. Si una entrada contiene
una capacidad que el compilador no entiende, debe diagnosticar esa limitación;
no debe rechazarla simplemente por una etiqueta histórica.

Esta es la dirección del producto, no una certificación de que todos los
artefactos anteriores se hayan probado. La compatibilidad concreta se verifica
con sus productores, consumidores y contenido reales.

Los expedientes históricos de versionamiento se conservan fuera del repositorio,
en `X:\nsc-nsmeta-historico`. No consultarlos como instrucciones para el trabajo
actual. Las pruebas existentes se conservan intactas.
Las referencias históricas que todavía aparezcan mezcladas en otros documentos
no son instrucciones vigentes y no deben consultarse para decidir cambios.
