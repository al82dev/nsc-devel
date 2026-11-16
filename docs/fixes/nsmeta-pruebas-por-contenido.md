> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Pruebas de NSMETA por estructura y contenido (NSC-META-001)

Decisión del usuario (2026-11-14): `nsc.stage48_2.library-metadata` y
`nsc.stage50_9.metadata-host-compatibility` validan la estructura y el contenido de los metadatos.
No seleccionan reglas ni rechazan archivos por una etiqueta numérica. Se conservan los fixtures y
la cobertura funcional; se sustituye el criterio que dependía de la etiqueta.

## Estado previo

Con los ejecutables reconstruidos, 132 de 412 comprobaciones de `stage48_2` y 11 de 58 de `stage50_9`
fallaban. Las expectativas pedían que un archivo "conservara su identidad 1.N", que el escritor
omitiera campos según el número, que un 1.0/1.1 mínimo se aceptara por serlo y que un minor futuro
se rechazara. El códec actual no codifica `formatMinor`: el escritor siempre escribe la estructura
completa y el lector solo decodifica esa estructura.

## Criterios nuevos

- **La etiqueta no cambia nada.** Cada modelo se escribe con su etiqueta y con la actual, y los
  dos archivos deben ser idénticos byte a byte (perfiles 1.0–1.43 de `stage48_2`; modelos 1.1–1.7
  de `stage50_9`; `TypedParameterPackProducer` con etiqueta 1.23).
- **Lo compatible se lee y conserva sus datos.** Cada perfil vuelve con los mismos tipos,
  plantillas, miembros, funciones y tablas de despacho, y el compilador lo acepta. En `stage50_9`
  vuelven exactos los datos que antes debían desaparecer: el valor por defecto entero, el modo
  `ref`, la firma del delegado, el accesor del descriptor, la accesibilidad y la declaración de la
  plantilla, y el contrato ABI. Los registros siguientes no se desplazan. El pack de parámetros
  tipados sigue presente tras la etiqueta 1.23.
- **Lo incompleto o contradictorio se rechaza con el problema concreto, y cada negativo tiene su
  control.**
  - Sin identidad de módulo: "metadata module identity is empty", con etiqueta 1.0 y 1.1. El mismo
    modelo con identidad se acepta con ambas.
  - Plantilla sin contratos de parámetro: "parameter-contract count that differs from its generic
    arity". Con el contrato se acepta.
  - Object ABI 1 y 2: "object ABI expected 3, found N; recompile the library with the current
    compiler". El ABI 3 se acepta. El ABI de objetos es contenido declarado, no etiqueta de formato.
  - Se mantienen sin cambios los negativos de suma de control, magia, cabecera truncada, puntero,
    objetivo y contratos ABI, y los positivos con otra etiqueta o un minor futuro.
- **Productores y consumidores concuerdan.** Se conservan las pruebas de productor/consumidor
  (DLL, LIB, exportaciones contra NSMETA, consumidores sin fuente).

## Ambigüedad registrada

Los 40 archivos de `tests/fixtures/nsmeta/historical-1.*.nsmeta` tienen la forma de registro de su
época, sin los campos añadidos después. Sin la etiqueta, nada en los bytes dice qué campos omite un
registro: una forma antigua y la actual no se distinguen por contenido. No se usa la etiqueta para
resolverlo. Cada archivo debe rechazarse con un diagnóstico de registro malformado ("truncated or
invalid metadata type record", "invalid metadata variant case count", "... enum underlying type",
"... delegate dyn signature"), nunca por magia, versión o formato. El perfil del mismo contrato
escrito en la forma actual es el control positivo. Los fixtures se conservan y la prueba exige
encontrar los 40. Se retiró el refresco `NSC_P2_REFRESH_GOLDENS`: sin etiqueta no hay un minor nuevo
que archivar. `environmentFlag` queda sin uso y marcada `[[maybe_unused]]`.

## Otro ajuste

El productor de `ClosedGenericConsumer` pasaba un campo `owned` prestado (`box.Values`) a un
parámetro de clase sin calificar. Desde `921cef466` ese parámetro puede retener el valor y la
llamada se rechaza también desde fuente (comprobado aparte). El parámetro se declara `borrowed`, que
es lo que hace. El consumidor sin fuente lo lee del NSMETA.

## Resultado

`stage50_9`: 67/67 comprobaciones. `stage48_2`: 415/415 (antes 132 de 412 fallaban). Pasan también las demás
pruebas `metadata`/`nsmeta` con sus ejecutables reconstruidos (ver el seguimiento).
