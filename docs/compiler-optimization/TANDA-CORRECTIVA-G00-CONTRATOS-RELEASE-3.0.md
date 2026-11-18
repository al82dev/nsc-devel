> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Tanda correctiva G00: contratos de release NativeSharp

## Problema

En aquella campaña la construcción generaba siete artefactos heredados cuyo contenido describía
una migración y una promoción anteriores. Reutilizarlos con el compilador de esa revisión podía
mezclar contratos y presentar como estable una revisión que no había completado su baseline.

## Cambios históricos y estado actual

- El ledger histórico describía cambios del compilador, NSMeta, Runtime ABI y runtime symbol set.
  El contenido vigente identifica el producto 1.0 sin seleccionar compatibilidad por versión de
  lenguaje o NSMeta.
- La guía histórica describía una selección por edición. Esa regla fue retirada: la guía vigente
  no exige una etiqueta ni selecciona la semántica por versión.
- El baseline de migracion publica los contratos fisicos actuales y queda
  `pending-final-baseline`.
- El freeze de esa campaña registraba la candidata y los contratos físicos de su revisión. No
  acredita el estado actual.
- La matriz se marca `pending-final-baseline`; no se presenta como certificada antes de tiempo.
- El contrato final distingue el staging que debe verificarse en
  `A:/native-sharp-3.0-baseline` del destino de promocion `C:/native-sharp`.
- El acta de promocion publica canal `development` y decision `pending-final-baseline`. El cambio
  a `stable`/`officially-promoted` queda reservado para el gate final ejecutado por Alberto.
- En esa campaña CMake, el manifiesto de instalación y `InstallationVerifier` usaban nombres
  históricos para los siete artefactos; los nombres actuales no incluyen una versión de lenguaje.
- Las pruebas dirigidas y sus fixtures de instalacion fueron migrados a los mismos nombres y
  contratos.

No se modifico ni se instalo nada en C:.

## Archivos nuevos

Solo este documento. Los artefactos del contrato son productos generados del build, no archivos
fuente; sus nombres actuales no incluyen una versión de lenguaje.

## Mejora

La campaña evitó mezclar contratos de revisiones distintas. La promoción permanece condicionada a
la evidencia correspondiente; los resultados históricos no certifican cambios posteriores.

## Validacion especifica

- Target `nsc` construido en `A:\nsc-devel\build-clang`.
- La construccion genero los siete artefactos `NativeSharp3*`.
- Verificador de instalacion: 51/51.
- Baseline de migracion: 15/15.
- Ledger: 15/15.
- Guia de migracion: 9/9.
- Freeze RC: 10/10.
- Matriz final: 16/16.
- Contrato de baseline final: 24/24.
- Acta de promocion pendiente: 11/11.
- La evidencia dirigida se mantuvo en A: y fue eliminada al terminar.
- Se eliminaron 14 residuos `NativeSharp2*` unicamente de los dos directorios `generated`
  utilizados; no se toco evidencia `known-good` ni historiales de autoalojado.
- No se ejecuto CTest general ni el baseline.
