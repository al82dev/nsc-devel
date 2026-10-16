# Tanda de validación 258 — preflight de baseline y cierre de metadata

## SCOPE LOCK

- Objetivo: corregir la preparación de Gate 0 hasta dejar el baseline listo para su ejecución
  completa desde NSC Builder 3.
- Repositorio y rama: `A:\nsc-devel`, `development`.
- Alcance de ejecución: compilación dirigida de `nsc`, prueba focal de Stage 48.2, bootstrap O0
  de `NativeSharp.Core`, auditoría de catálogos y smoke de GUI.
- Fuera de alcance: matriz completa de `5,991` casos, promoción a `C:\native-sharp`, deploy,
  batería CTest completa, push, pull y fetch.

## Causas demostradas

1. El cierre de plantillas reutilizables trataba `Future<T>`, `Iterator<T>`,
   `AsyncIterator<T,E>`, tuplas y SIMD estructural como dependencias nominales. El bootstrap
   rechazaba por ello el contrato público de `TaskScope<T>` aunque no existe una plantilla
   nominal que publicar para esas superficies.
2. La reconstrucción de metadatos exigía que getter y setter compartieran el flag `readonly`.
   `Span<T>` declara legalmente un `readonly get` y un `set` mutable; el read-back fallaba con
   `metadata accessor pair for 'Item' has inconsistent type or modifiers`.
3. Tres mejoras vigentes del Builder modular no estaban sincronizadas en la referencia
   monolítica `nscbuilder3.py`: `NA` como optimización no aplicable, descubrimiento A-only de
   7-Zip y piso de 15 segundos para historiales cortos.

## Corrección

- `ProjectGenericModel` comparte una sola clasificación de superficies genéricas estructurales
  entre especialización y descubrimiento de dependencias.
- El importador de NSMETA compara sólo los modificadores comunes de un par de accesores,
  regenera `readonly get` cuando existe un setter mutable y sigue rechazando un setter
  `readonly` imposible de expresar legalmente.
- La regresión Stage 48.2 combina un `Future<void>` público con un indexador genérico de getter
  readonly y setter mutable, obligando al ciclo productor/read-back a conservar ambos contratos.
- La referencia monolítica del Builder quedó sincronizada con las reglas modulares ya
  certificadas; no se reactivó versionamiento NSMETA ni se alteró su formato.

## Evidencia dirigida

- `nsc_stage48_2_library_metadata_tests` compila. La nueva regresión no reporta fallo; el
  ejecutable monolítico conserva 16 fallos históricos ajenos a este cambio, principalmente de
  productos DLL/archive y una huella ABI dorada. No se presentan como aprobados.
- `nsc` Release compila y pasa su contrato de runtime host estático.
- El bootstrap real O0 de las 52 fuentes de `lib/NativeSharp.Core` termina con código 0 y produce
  `NativeSharpCore.lib`, `NativeSharpCore.h` y `NativeSharpCore.nsmeta` bajo `A:`.
- La primera corrida modular pasó `67/70` y descubrió exactamente las tres divergencias de la
  referencia monolítica descritas arriba. Tras sincronizarlas, la corrida final pasó `70/70`.
- Las 10 pruebas dirigidas de esquema CSV y autoridad del manifiesto pasan.
- `--audit-catalog` informa `5,991` casos ejecutables, `682` contratos y `30` componentes.
- `--gui-smoke` construye y cierra correctamente la interfaz.
- El plan real de Gate 0 materializa `A:\native-sharp-3.0-baseline`, toma NSLink de
  `A:\nslink-devel\bin\release\nslink.exe`, ejecuta la matriz con los binarios staged y conserva
  la matriz obligatoria `O0,O1,O2,O3`.

## Estado

`PREFLIGHT APROBADO; MATRIZ COMPLETA PENDIENTE DEL USUARIO EN GUI`.

No se ejecutó el baseline completo ni se tocó `C:`. Sólo un PASS integral y árbol inmóvil
autorizarán la promoción automática ya existente.
