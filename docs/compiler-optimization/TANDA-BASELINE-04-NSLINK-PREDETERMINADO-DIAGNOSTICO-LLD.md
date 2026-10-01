# Tanda baseline 04 — NSLink predeterminado y diagnóstico selectivo con LLD

## Objetivo

Hacer que NSCBuilder valide de forma ordinaria el camino que utilizará el usuario final:
`nsc -> NSLink`. La pasada completa con LLD deja de repetirse por defecto. Si NSLink falla,
NSCBuilder ejecuta con LLD únicamente las pruebas exactas que fallaron y conserva el fallo
original.

Esta tanda reemplaza la regla de certificación descrita en la tanda 03: Gate 0 ya puede
certificar la matriz completa O0–O3 de NSLink con diagnóstico LLD bajo demanda. La ejecución
completa de ambos enlazadores permanece disponible como validación profunda explícita.

## Políticas disponibles

### Predeterminada

- NSLink ejecuta CTest, fixtures y bug triage en O0–O3.
- Si todo pasa, LLD no se ejecuta.
- Si una prueba falla, LLD repite únicamente ese nombre CTest o ese par exacto
  `fixture/optimización`.
- Un resultado aprobado con LLD clasifica el problema como `nslink-only`.
- Un resultado también fallido con LLD lo clasifica como `both-failed`.
- Ningún resultado diagnóstico cambia el fallo primario de NSLink a aprobado.
- Con los cuatro perfiles, esta política es certificable y puede promover sólo si Gate 0 queda
  completamente verde.

### Profunda

- Ejecuta toda la matriz con NSLink y LLD.
- No agrega una tercera pasada diagnóstica.
- Es certificable cuando incluye O0, O1, O2 y O3.

### LLD aislado

- Ejecuta únicamente LLD.
- Es una validación de alcance reducido.
- No escribe G00 ni promueve a `C:\native-sharp`.

## CTest sin un segundo árbol redundante

Los árboles CTest de NSLink y LLD producen los mismos ejecutables C++. El enlazador usado por
los programas que esas pruebas generan se inyecta mediante `NSC_LLD_LINK` al momento de
ejecutarlas. Por ello, el diagnóstico selectivo con LLD reutiliza el árbol CTest ya construido
para NSLink y cambia solamente el entorno del proceso.

Esto evita configurar, compilar y enlazar por adelantado una segunda copia completa de los
ejecutables de prueba. El modo profundo conserva árboles separados porque fue solicitado de
manera explícita como matriz completa.

## Evidencia y clasificación

- `metadata.json` registra `lld_diagnostics` con tipo, prueba, optimización y clasificación.
- `report.md` incluye una tabla específica de diagnóstico selectivo.
- El payload de Gate 0 registra `linker_policy` y los diagnósticos LLD.
- Los canales y consolas de LLD se crean desde el principio, pero sólo reciben trabajo si NSLink
  falla.
- La barra de progreso amplía su total únicamente cuando se activa trabajo diagnóstico real.
- Se eliminó el avance duplicado que contabilizaba dos veces una etapa CTest ya registrada.

## Interfaz y consola

- La interfaz de pruebas selecciona por defecto `NSLink (LLD sólo diagnostica fallos)`.
- Gate 0 selecciona por defecto `NSLink — predeterminado; LLD sólo diagnostica fallos`.
- La selección histórica basada en índices se migra a una política con nombre para que el nuevo
  orden no active accidentalmente el modo profundo.
- `--linker NSLink` es ahora el valor predeterminado de consola.
- `--linker both` solicita la validación profunda.
- `--linker lld-link` conserva el diagnóstico aislado.

## Archivos modificados

- `nscbuilder.py`
  - política y certificación;
  - fallback selectivo de CTest, fixtures y triage;
  - canales, progreso, evidencia, reportes, GUI, CLI y migración de preferencias.
- `tests/selfhost/test_bootstrap_infrastructure.py`
  - contratos del modo predeterminado, profundo y reducido;
  - selección exacta del fallback;
  - garantía de que un fallo diagnóstico no incrementa ni sustituye el fallo primario.

## Archivo nuevo

- Este documento. Se crea para conservar de forma permanente el comportamiento, las decisiones,
  los archivos afectados, la mejora de rendimiento y la validación de la tanda.

## Mejora esperada

En una corrida verde ordinaria se elimina por completo la segunda ejecución LLD de CTest,
fixtures y triage, además del segundo árbol CTest. El costo LLD queda proporcional al número de
fallos de NSLink, normalmente cero, en vez de ser proporcional a toda la matriz.

## Validación específica

- Suite Python exclusiva de infraestructura NSCBuilder: 75/75 aprobadas en 0.225 s.
- Cinco contratos iniciales de política/caché: 5/5 aprobados.
- Dos contratos aislados de fallback y contabilización: 2/2 aprobados.
- Sintaxis de `nscbuilder.py` y sus pruebas: correcta.
- `nsc` construido exclusivamente en `A:\nsc-devel\build-clang` con O0, LLVM estático,
  `clang-cl` y `lld-link`: 274 acciones, aprobado.
- Contrato posbuild de host estático: aprobado.
- `nsc.exe --version`: NativeSharp Compiler 3.0.0, ejecución correcta.
- `LLVM-C.dll`: ausente.
- Baseline/CTest completo: no ejecutado.
- Instalación o modificación de `C:\native-sharp`: ninguna.
- `_abi/gates/G00/gate.json`: cambio preexistente del usuario, preservado y excluido del commit.
