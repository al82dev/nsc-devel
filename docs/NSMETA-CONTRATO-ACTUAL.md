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

## Sesión de fuente (ampliación X4-06, 24 de noviembre de 2026)

Un consumidor precompilado sin el encabezado C/C++ recibe del productor lo que un
catálogo de valores no puede describir: records C++ no triviales o polimórficos
(constructores, destructor, asignación, bases, miembros virtuales),
especializaciones de plantillas de clase y typedefs `sysv_abi` que una plantilla
publicada usa sin nombrarlos.

Representación: se amplía la entrada existente de fuente de macros del catálogo C
(`MacroSource`, la fuente de la referencia preprocesada por Clang con sus macros),
sin formato nuevo ni versión paralela. Su lista de nombres admite, además de los
nombres de macro:

- `__nsc_source_session`: la fuente es una sesión de fuente completa;
- `type:<identidad>`: un record que la sesión describe (lo nombra la superficie de
  plantillas publicadas);
- `template:<nombre>`: una plantilla de clase C++ que el productor especializa;
- `option:rtti=0`, `option:exceptions=0`: opciones de generación de código de la
  referencia.

El productor marca así una referencia cuando alguno de sus records no es un valor
de bytes, cuando instancia una de sus plantillas de clase o cuando una función que
publica recibe o devuelve un puntero `sysv_abi`. Los records, funciones, globales,
alias y miembros de esa referencia no entran al catálogo de valores: la sesión es
su única descripción, y el consumidor nunca ve el mismo record por dos vías. El
consumidor abre sobre esa fuente una sesión Clang completa, propia del proyecto
precompilado, e importa de ella como lo hace un consumidor con encabezado. No se
exige instalación nueva ni reconstruir Core.

Compatibilidad: el contenido ya admitido no cambia; una entrada sin la marca sigue
siendo solo de macros. Un lector anterior ve los nombres nuevos como nombres de
macro sin efecto. Validación: `nativesharp-interop/interop-sdk/limitaciones_nsmeta.py`
(producción y consumo, lib y dll, O0–O3) y los casos `lib` de X2/X3/X4/C5/R1.

## Contratos de frontera (ampliación R1-05, 24 de noviembre de 2026)

Los `<Contract>` que un productor declara junto a su referencia C/C++ (Retained,
Transfer, Out, NonNull, Nullable y resultado Owned con su función de liberación)
viajan en el catálogo C, en un grupo de contratos que sigue a las fuentes de
macros y se escribe solo si hay alguno: módulo, referencia, función, parámetro
opcional, tipo, límite `Until`, opción `Optional` y función `Release`. Un consumidor
precompilado sin encabezado los aplica a las funciones del productor con la misma
correspondencia que la importación directa, también a las que importa de una
sesión de fuente, así que una plantilla publicada que viola un contrato se rechaza
con el mismo motivo. Un catálogo sin contratos no cambia. Validación:
`limitaciones_r1.py` (`rechazo_lib`, seis contratos en el consumidor precompilado).

Los expedientes históricos de versionamiento se conservan fuera del repositorio,
en `X:\nsc-nsmeta-historico`. No consultarlos como instrucciones para el trabajo
actual. Las pruebas existentes se conservan intactas.
Las referencias históricas que todavía aparezcan mezcladas en otros documentos
no son instrucciones vigentes y no deben consultarse para decidir cambios.
