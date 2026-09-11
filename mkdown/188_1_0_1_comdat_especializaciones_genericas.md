# 1.0.1 — COMDAT para especializaciones genéricas reutilizables

## Resultado

Un productor estático y su consumidor pueden materializar la misma
especialización genérica cerrada sin generar definiciones fuertes duplicadas.
Las entidades de una especialización se emiten como un solo grupo ODR con un
COMDAT COFF estable, derivado de su clave canónica.

La corrección abarca cuerpos de métodos, constructores, destructores, entradas
de destrucción, almacenamiento y accessors estáticos, vtables, tablas de
interfaces, mapas de interfaces, descriptors, accessors de descriptors y
nombres RTTI.

## Causa raíz

El frontend ya marcaba algunas funciones con `genericSpecializationKey`, pero
esa procedencia no llegaba a las entidades sintetizadas por tipo. Además,
`linkonce_odr` sin un COMDAT explícito seguía produciendo símbolos fuertes en
COFF. Un COMDAT asociativo también necesita un símbolo líder real.

## Archivos e implementación

- `include/nativesharp/Frontend/ProjectOopMirModel.h`: añade la procedencia
  genérica a campos estáticos y tablas de despacho.
- `src/Frontend/ProjectOopMirModel.cpp`: propaga la clave desde la marca AST y,
  cuando la agregación pierde esa marca, reconoce el nombre cerrado estable
  `__g…__h<16 hex>`.
- `src/Frontend/ProjectOopMirModelDumper.cpp`: muestra la procedencia genérica de
  cada tabla para diagnóstico dirigido.
- `src/LLVM/LlvmCApi.h`: declara las linkages y la API COMDAT oficial de LLVM.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: crea un COMDAT por clave canónica, añade
  un líder `i8` estable y asocia todas las definiciones de la especialización.
  Usa `weak_odr`, no `linkonce_odr`, para conservar símbolos que una DLL debe
  exportar aunque O2 haya eliminado todas sus llamadas locales.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: el consumidor instancia el
  mismo `Bucket<ProducerValue>` ya emitido por el productor.

## Decisiones

La unidad de selección es la especialización completa. Esto mantiene coherentes
descriptor, vtable, destrucción y RTTI, en vez de plegar sólo cuerpos aislados.
La clave del grupo usa SHA-256 de la identidad canónica; no depende de rutas ni
de la ortografía empleada por el consumidor.

No cambia el formato NSMETA ni el mangling observable. No se modificaron
bindings ni tutoriales.

## Evidencia dirigida

- El productor y `ConsumerInstantiate` se reconstruyeron con el mismo NSC.
- El ejecutable O0 enlazó sin símbolos duplicados.
- `ConsumerInstantiate.exe` terminó con código 0.
- La regresión dirigida Stage 48.2 terminó con 185 comprobaciones correctas.

No se ejecutó baseline.

## Commit de referencia

### Español

**Título:** Deduplica especializaciones genéricas con COMDAT

**Descripción:** Propaga la procedencia genérica a todas las entidades
sintetizadas y emite cada especialización como un grupo `weak_odr` con líder
COMDAT estable. Evita símbolos fuertes duplicados entre bibliotecas estáticas y
conserva exports genéricos de DLL.

### English

**Title:** Deduplicate generic specializations with COMDAT

**Description:** Propagate generic provenance to every synthesized entity and
emit each specialization as a `weak_odr` group with a stable COMDAT leader.
Prevent duplicate strong symbols across static libraries while preserving
generic DLL exports.

## English summary

Closed generic specializations now form one stable COFF COMDAT group across
producer and consumer modules. The policy covers functions, static storage,
destruction, dispatch metadata, descriptors, and RTTI while keeping DLL exports
available.
