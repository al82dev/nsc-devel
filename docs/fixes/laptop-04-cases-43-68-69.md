# Verificación independiente de los casos 43, 68 y 69 (laptop 04)

Fecha: 2026-11-12

## Alcance y base

- Repositorio: `A:/nsc-fixes-laptop-04`.
- Rama: `development-fixes-limitaciones-laptop-04`.
- Base compartida indicada: `71cd86d35220f654140e5619e9f9db0020f0b158`.
- HEAD previo a esta verificación: `9c52345edc05f7ce65b536f455fd4a0cb8808ecf`.
- Compilador aislado: `A:/nsc-fixes-laptop-04-build/bin/nsc.exe`.
- Identidad del compilador: NativeSharp Compiler 3.0.0, build hash
  `6135c72878bb838e`, SHA-256
  `27073264c15963e41bda1c26bfa8514048145941d159c073438f3596056d4dfb`.
- Runtime y todos los temporales, cachés, salidas y evidencias usados están en
  los directorios asignados de `A:`.

Se conservaron sin cambios los fixtures versionados. El reporte histórico
`A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md` y los ejemplos
originales externos mencionados por documentación anterior no están presentes
en esta laptop. Por eso no se afirma identidad byte a byte con esos archivos
externos; la evidencia corresponde a los fixtures versionados actuales.

## Bloqueo de alcance

Entrada: los fixtures existentes de despacho sellado, impl sobre struct,
delegado genérico y lambda global. Operación: compilar y ejecutar en O0/O2 y,
cuando existe un test binario aplicable, comprobar productor y consumidor.
Invariantes: no cambiar semántica, parser, HIR, metadata, préstamos ni archivos
del equipo de campos. Archivos protegidos: `CMakeLists.txt`, frontend compartido,
analizadores de préstamos y productores/consumidores de metadata. Las únicas
ediciones de implementación son runners localizados de estos casos. No se modifica ninguna
ruta semántica ni de metadata.

## Caso 43: override sellado

La corrección histórica `a8cb061f9ca55dd9988c620d0e027d835e42d8e4`
está en la historia de la rama. La prueba dirigida actual confirma:

- `Program.Sealed(Derived)` contiene `direct-instance-call` al override.
- El control `Program.Open(Base)` conserva `virtual-instance-call`.
- O0 y O2 compilan y terminan con código 42.
- Los controles nulos sellado y abierto producen el mismo código no cero (135)
  en O0 y O2.
- El productor binario O0/O2 y el consumidor O0/O2 pasan las cuatro
  combinaciones; el MIR del consumidor conserva directo frente a virtual.

Esto prueba la forma de despacho y el comportamiento funcional. No es una
medición de rendimiento: no se ejecutó benchmark, perfilador ni comparación de
latencia o throughput.

Evidencia: `A:/nsc-fixes-laptop-04-evidencia/case43-source.log` y
`case43-binary.log`.

## Caso 68: impl externo sobre struct

La corrección histórica `f3b0314c0e82da34190768a192622960abc4b486`
está en la historia de la rama. `VerifyStructTraitImpl.cmake` pasa en O0 y O2:
ejecución 42, llamada directa al método de `Number`, ausencia en ese trayecto de
asignación/construcción de objeto y de llamada de interfaz, y rechazo de la
conversión dinámica negativa.

La auditoría del runner detectó que su control negativo buscaba
`interface-call`, mientras que el nombre MIR real es
`interface-instance-call`. Se corrigió esa aserción y el runner ahora también
rechaza `virtual-instance-call`; la prueba completa volvió a pasar.

Esta es evidencia estructural de despacho estático y ausencia de boxing en el
trayecto probado; no es una medición general de asignaciones del runtime.

El gate binario exacto publica el trait, el struct no genérico y su impl en
`Producer.lib`/`Producer.nsmeta`; el consumidor sólo usa
`LibraryReference`. Pasan las cuatro combinaciones productor O0/O2 por
consumidor O0/O2 con salida 42. Cada MIR consumidor contiene dos
`direct-instance-call` a `Number.Value` y cero
`interface-instance-call`, `virtual-instance-call` o
`allocate-and-construct`. La reproducción quedó versionada en
`tests/integration/struct_trait_impl_binary.py` y se ejecuta directamente con
el compilador aislado como argumento.

Como evidencia binaria suplementaria se ejecutó
`generic_struct_trait_impl.py`: productor 0, consumidores O0/O2 42. Ese test
también contiene impls genéricos y restricciones, propiedad del equipo de
campos. Se usó solo como validación de lectura, no se modificó y no se presenta
como sustituto exacto del fixture no genérico.

Evidencia: `A:/nsc-fixes-laptop-04-evidencia/case68-struct.log`,
`case68-struct-strengthened.log`, `case68-binary-regression.log`,
`case68-binary-exact.log` y `case68-binary-related.log`. Los artefactos
exactos conservados quedaron en `case68-binary-exact-8cij4tnj`.

## Caso 69: delegado genérico y lambda global

Las correcciones históricas `3c03a4440` (retorno genérico del delegado) y
`f8a05890f` (lambda global sin captura) están en la historia de la rama. Los
fixtures versionados no han cambiado desde sus correcciones respectivas.

- La lambda global compila y ejecuta con salida 0 en O0 y O2.
- El delegado genérico con retorno `T` pasaba en O0.
- Se reprodujo una brecha de regresión: su runner no ejecutaba O2 aunque la
  documentación histórica declaraba ambas optimizaciones.
- El runner ahora itera O0/O2, usa ejecutables distintos y reporta el nivel en
  cualquier error. Ambos niveles compilan y ejecutan con salida 0.
- El gate binario publica `Reducer<T>` y métodos para `int` y `long` en
  `.lib`/`.nsmeta`. Los consumidores reconstruyen e invocan ambos delegates
  mediante `LibraryReference`. Pasan las cuatro combinaciones productor O0/O2
  por consumidor O0/O2 con salida 0. La reproducción quedó versionada en
  `tests/integration/generic_delegate_return_binary.py`.

El gate binario sólo valida el round trip; no modifica serialización ni
reconstrucción de metadata. La lambda global es un callable local sintetizado,
no una API publicada, y por eso no tiene contrato productor/consumidor
separado.

Evidencia: `A:/nsc-fixes-laptop-04-evidencia/case69-global-lambda.log`,
`case69-delegate-baseline.log`, `case69-delegate-o0-o2.log` y
`case69-delegate-binary-regression.log` y `case69-delegate-binary.log`. Los
artefactos binarios conservados quedaron en `case69-delegate-binary-98ln0_tw`.

## Límites y archivos compartidos

- No se modificaron `CMakeLists.txt`, `DeclarationParser.cpp`,
  `HirBuildCoordinator.cpp`, `MirNodeMachine.cpp`, LLVM, metadata, préstamos ni
  archivos del equipo de campos o del integrador.
- Las dos regresiones binarias nuevas se ejecutan directamente; no se
  registraron en `CMakeLists.txt` porque ese archivo pertenece al integrador.
- No se ejecutó la batería completa; solo build inicial autorizado y pruebas
  dirigidas incrementales.
- No se usó `C:/native-sharp` como compilador ni SDK, ni se hizo deploy, push,
  merge o rebase.
- El PATH heredado excede el comportamiento fiable de resolución de CMD. Tras
  comprobar que los nombres no resolvían, las pruebas fuera del aislamiento
  usaron rutas verificadas de CMake, Python y Git, y el linker aislado
  `A:/nslink-devel/bin/release/nslink.exe`, sin modificar PATH.
- El primer intento de ejecución mediante `start /wait` cambió el directorio a
  `C:/Windows` y ocultó el código de salida. Sus aparentes resultados se
  descartaron; todos los resultados informados arriba proceden de repeticiones
  directas con el código real del proceso.
