# Tanda universal 47: aislamiento de fixtures NSMETA históricos

## SCOPE LOCK

- Objetivo: eliminar las causas estáticas independientes de los fallos 449 y 497 sin debilitar
  la validación de `LibraryMetadataCodec`.
- Naturaleza: corrección de infraestructura de pruebas y aislamiento de fixtures.
- Comportamiento protegido: el escritor NSMETA debe continuar rechazando características cuya
  semántica no puede representarse en el minor solicitado.
- Archivos protegidos: compilador, parser, semántica, lowering, runtime, bibliotecas instaladas y
  contenido de `C:`.
- Pruebas protegidas: no se renombra ni elimina ninguna prueba; se conserva la intención de los
  contratos de Stage 46, Stage 48.2 e Item 313.
- Prohibido: baseline certificado, deploy, instalación, O1/O2/O3 y declarar cerrados los 19 fallos
  antes de la batería integral O0 con NSLink.
- Terminación de la tanda: fixtures históricos independientes de capacidades posteriores, helper
  único de proyección, Markdown, commit y validación dirigida posterior al commit.

## Causa raíz comprobada

`nsc.stage46.native-libraries` reutilizaba el metadata de un productor rico que contiene ownership,
lifetimes, genéricos, destructores y excepciones. La prueba sólo pretende reproducir un productor
1.10 que escribió una jerarquía inválida sobre una clase estática. Al aparecer un campo NSMETA nuevo,
el fixture dejaba de representar un contrato 1.10 y fallaba antes de llegar a la conducta probada.
Los errores de verificación y consumo eran cierres derivados de que `NativeMath.nsmeta` no se había
escrito.

`nsc.item313.reference-returns` cambia el minor de un modelo actual a 1.14 para demostrar que el
escritor elimina el bit de retorno por referencia introducido en 1.15. El modelo conservaba contratos
de lifetime posteriores a 1.14, por lo que el escritor lo rechazaba antes de ejecutar la proyección
del bit que la prueba realmente mide.

## Cambios

1. `HistoricalMetadataProjection.h` concentra la proyección de campos agregados en minors 1.34 a
   1.38. No elimina bitfields, variantes, traits, `Copy`, parameter packs ni otras capacidades que
   deben seguir provocando un rechazo real.
2. Item 313 proyecta los campos posteriores antes de solicitar 1.14 y conserva el bit ref-return para
   que sea el codec quien demuestre su eliminación. El diagnóstico de escritura incluye ahora el
   error concreto.
3. Stage 48.2 usa el helper compartido en lugar de mantener una copia privada que podía divergir.
4. Stage 46 crea un productor mínimo y exclusivo para la jerarquía estática histórica. Su `.lib` y
   su `.nsmeta` ya no dependen de destructores, throws, genéricos ni del resto del productor rico.

## Archivos

- Nuevo: `tests/support/HistoricalMetadataProjection.h`.
- Modificado: `tests/unit/Item313ReferenceReturnsTests.cpp`.
- Modificado: `tests/unit/Stage46NativeLibrariesTests.cpp`.
- Modificado: `tests/unit/Stage48_2LibraryMetadataTests.cpp`.
- Nuevo: este documento.

## Algoritmo y complejidad

La proyección recorre una vez tipos, miembros, funciones y plantillas. Su costo es lineal,
`O(T + M + F + G)`, y utiliza memoria adicional constante fuera de las capacidades ya almacenadas
por el modelo. No usa recursión.

## Estado de validación

- Análisis estático: completado.
- `git diff --check`: sin errores; sólo avisos de normalización LF/CRLF del checkout.
- Compilación dirigida: pendiente hasta después del commit, conforme a la metodología vigente.
- Pruebas 449 y 497: abiertas hasta la corrida integral O0 con NSLink.
- Inventario integral: 19 fallos abiertos; esta tanda no lo reduce por evidencia dirigida.
