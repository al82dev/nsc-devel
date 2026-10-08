# Tanda universal 49: escalares calificados por ownership en el self-host

## SCOPE LOCK

- Objetivo exacto: restablecer la equivalencia de `--dump-symbols` entre `nsc0` y `nsc2` haciendo que
  el backend self-hosted materialice `owned bool`, `borrowed int` y los demás escalares calificados
  con el mismo tipo físico LLVM que su tipo base.
- Naturaleza: corrección funcional de una regresión del backend self-hosted.
- Comportamiento que debe permanecer idéntico: `T` y los demás placeholders genéricos continúan
  usando el ABI borrado basado en punteros; las referencias, arreglos, funciones y tipos nominales
  continúan siendo referencias; los escalares sin calificador conservan su representación actual.
- Revisión conocida como correcta: `nsc0` produce dos símbolos para
  `tests/integration/valid/cfg_features.ns`.
- Rango causal examinado: la ruta de conversión genérica introducida hasta `9add8b9c56` y el estado
  vigente en `feba2300c`.
- Archivos autorizados: `selfhost/nsc/CodeGen/LlvmBackend.ns` y este documento.
- Archivos protegidos: `SemanticDumper.ns`, `List.ns`, el fixture `cfg_features.ns`, verificadores,
  contratos, metadatos, runtime, bibliotecas, NSLink y cualquier contenido en `C:`.
- Pruebas protegidas: todas las pruebas existentes deben conservarse sin modificaciones.
- Acciones prohibidas: baseline, build, ejecución local de pruebas, perfiles O1/O2/O3, deploy,
  cambios en `C:`, workaround en el consumidor y recursión nueva.
- Criterio de terminación de la tanda: normalización física implementada, diferencial auditado,
  Markdown y código incluidos en un commit local; la validación funcional queda pendiente de la
  corrida externa del usuario.

## Evidencia congelada

La corrida externa `20261007-192622-6fa7fec1` llegó a SH42 después de aprobar SH40 y SH41. SH42
falló al comparar el primer artefacto semántico:

```text
verify_semantics: error: O0 semantic dump differs from nsc0:
--dump-symbols tests/integration/valid/cfg_features.ns
```

El frontend de la misma generación sí reconoció las dos funciones del fixture. La diferencia no
se origina en parser, fixture ni descubrimiento de unidades. El `nsc2` generado emitió un dump de
símbolos vacío mientras `nsc0` emitió `Select` y `Main`.

La inspección del objeto O0 ya generado mostró que `SemanticDumper.DumpSymbol` llamaba al indexer
de `List<bool>` y comparaba el puntero retornado contra cero. Como el puntero de la caja existe,
`visited[id]` se evaluaba siempre como verdadero y cada símbolo retornaba antes de escribirse.

## Causa raíz

El indexer genérico declara `owned T` y el hecho semántico cerrado declara `owned bool`. La ruta
`ConvertScalar` reconoce que `owned T` es un placeholder, pero `PhysicalValueScalar` sólo reconocía
los nombres desnudos como `bool`. Además, `SemanticValueType` enviaba `owned bool` a `ScalarType`,
que lo clasificaba como puntero. Por ello nunca se producía la carga `i1` desde la caja genérica.

La causa pertenece al productor físico del backend. Cambiar el dumper para evitar `List<bool>` o
cambiar el indexer sería un workaround y dejaría el mismo defecto en cualquier consumidor de un
escalar genérico calificado.

## Corrección

1. Eliminar iterativamente los prefijos físicos `owned ` y `borrowed ` antes de clasificar un tipo.
2. Usar el nombre físico tanto para ABI/valores LLVM como para enteros, flotantes y signedness.
3. Conservar el nombre semántico original para detectar placeholders genéricos.
4. Mantener `owned T` físicamente como puntero porque, después de quitar el calificador, `T` sigue
   siendo un placeholder sin representación escalar concreta.

## Complejidad y memoria

La normalización consume tiempo `O(Q)` y memoria `O(N)`, donde `Q` es la cantidad de prefijos de
ownership y `N` la longitud del nombre del tipo. El algoritmo es iterativo y no utiliza recursión.
Para los tipos habituales `Q` es cero o uno.

## Estado

- Diagnóstico estático: confirmado mediante el log externo y el objeto O0 ya producido.
- Implementación: aplicada en la frontera física del backend self-hosted.
- Validación local: no ejecutada por instrucción expresa; el usuario ya está ejecutando la matriz
  en otra máquina.
- Validación externa: pendiente.
