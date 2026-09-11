# Cierre de runtime para campos estáticos precompilados / Runtime closure for precompiled static fields

## Resumen / Summary

Un consumidor de una DLL precompilada podía leer y escribir correctamente un campo estático mutable a través de su accessor publicado, pero el empaquetado no copiaba la DLL junto al ejecutable. El enlace terminaba porque la import library resolvía el símbolo; la ejecución fallaba porque el cierre de runtimes no reconocía el acceso a campo como uso físico del módulo.

A consumer of a precompiled DLL could correctly read and write a mutable static field through its published accessor, but packaging did not copy the DLL beside the executable. Linking succeeded because the import library resolved the symbol; execution failed because runtime closure did not recognize field access as physical module use.

## Archivo modificado / Modified file

- `src/Driver/Driver.cpp`
  - Amplía el cálculo de referencias físicas dentro de la validación de `LibraryReference`.
  - Extends physical-reference collection during `LibraryReference` validation.

## Funciones y estructuras involucradas / Functions and structures involved

- Recorrido recursivo `collectReferencedCallees` dentro del cálculo de bibliotecas precompiladas.
- `ProjectOopMirNodeKind::StaticFieldLoad`.
- `ProjectOopMirNodeKind::StaticFieldAddress`.
- `ProjectOopMirStaticField::accessorName`.
- Selección de runtime mediante `selectRuntimeForModule`.

## Implementación / Implementation

El recorrido MIR conserva ahora los ordinales de campos estáticos leídos o direccionados. Después de resolver llamadas nativas y `extern C`, recorre el catálogo autoritativo de campos estáticos y procesa únicamente los que cumplen todas estas condiciones:

1. Proceden de metadata precompilada.
2. Son mutables; los `const` no necesitan almacenamiento del productor.
3. Su ordinal aparece realmente en el MIR del consumidor.

Para cada campo usado, el accessor publicado en NSMETA se resuelve contra el mapa de símbolos físicos y selecciona el runtime de su módulo. Si la metadata no puede identificar al propietario del accessor, la compilación falla con un diagnóstico específico en vez de producir un ejecutable imposible de desplegar.

The MIR walk now records the ordinals of static fields that are loaded or addressed. After native calls and `extern C` imports are resolved, the authoritative static-field catalog is scanned. Only imported, mutable, actually referenced fields select a runtime. The NSMETA accessor identity is resolved through the physical-symbol owner map; an unowned accessor is diagnosed instead of producing an undeployable executable.

## Motivo / Rationale

Los accesos a campos estáticos no se representan como `DirectStaticCall` en MIR. LLVM introduce la llamada al accessor posteriormente en `staticFieldAddress`, demasiado tarde para que el Driver la descubra mediante `calleeName`. Rastrear explícitamente los nodos de campo mantiene el cierre mínimo: copia la DLL cuando hay almacenamiento físico usado, pero no por simples menciones nominales ni por constantes materializadas localmente.

Static-field accesses are not represented as `DirectStaticCall` in MIR. LLVM introduces the accessor call later in `staticFieldAddress`, too late for the Driver's `calleeName` scan. Explicit field-node tracking preserves a minimal closure: it copies the DLL for used physical storage, but not for nominal-only references or locally materialized constants.

## Validación / Validation

No se ejecutaron pruebas ni baseline durante esta corrección. El caso existente `testPrecompiledMutableStaticFieldContract` cubre biblioteca estática y DLL; se ejecutará en la validación acumulativa posterior.

No tests or baseline were run during this correction. The existing `testPrecompiledMutableStaticFieldContract` covers both static library and DLL products and will run during later cumulative validation.

## Commit de referencia / Reference commit

### Título / Title

```text
Incluye campos estáticos en el cierre de runtimes / Include static fields in runtime closure
```

### Descripción / Description

```text
ES:
- rastrea cargas y direcciones de campos estáticos en MIR
- selecciona la DLL propietaria del accessor precompilado usado
- diagnostica accessors publicados sin propietario de runtime

EN:
- track static-field loads and addresses in MIR
- select the DLL that owns the referenced precompiled accessor
- diagnose published accessors without a runtime owner
```
