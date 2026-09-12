# Higiene de artefactos generados / Generated artifact hygiene

## Resumen / Summary

El repositorio dejó de versionar tres árboles completos de CMake/CTest y el bytecode local de
Python. Esos productos se regeneran a partir de las fuentes y cambian en prácticamente cada
compilación, por lo que mantenerlos en el índice producía cientos de modificaciones irrelevantes y
ocupaba más de un gigabyte en la copia de trabajo observada.

The repository no longer versions three complete CMake/CTest trees or local Python bytecode. These
products are regenerated from source and change on virtually every build, so keeping them in the
index produced hundreds of irrelevant modifications and occupied more than one gigabyte in the
observed working copy.

## Archivos modificados / Modified files

- `.gitignore`
  - Añade `build-test-lldlinker` y el árbol histórico `build-tests`.
  - Conserva la exclusión ya existente de `build-test-nslink`.
  - Excluye directorios `__pycache__` y archivos `*.pyc`, `*.pyo` y `*.pyd`.
  - Adds `build-test-lldlinker` and the historical `build-tests` tree.
  - Preserves the existing `build-test-nslink` exclusion.
  - Excludes `__pycache__` directories and `*.pyc`, `*.pyo`, and `*.pyd` files.
- Índice de Git / Git index
  - Retira 1,475 productos bajo `build-test-lldlinker`, `build-test-nslink` y `build-tests`.
  - Retira dos archivos de bytecode bajo `__pycache__`.
  - Removes 1,475 products under `build-test-lldlinker`, `build-test-nslink`, and `build-tests`.
  - Removes two bytecode files under `__pycache__`.

## Qué se retiró del índice / What was removed from the index

Los árboles afectados contienen cachés de CMake, archivos Ninja, objetos COFF, librerías estáticas,
ejecutables de pruebas, PDB y bitácoras temporales. Todos son resultados derivados; NSC Builder o
CMake los vuelve a crear cuando la operación correspondiente los necesita.

The affected trees contain CMake caches, Ninja files, COFF objects, static libraries, test
executables, PDB files, and temporary logs. Every item is derived output; NSC Builder or CMake
recreates it when the corresponding operation needs it.

La operación usa una eliminación **sólo del índice**. Los árboles físicos permanecen intactos en el
disco durante este commit, de modo que ninguna compilación en curso o resultado local se destruye.
Una limpieza física posterior queda bajo el botón manual **Limpiar** de NSC Builder.

The operation is an **index-only removal**. Physical trees remain intact on disk during this commit,
so no in-progress build or local result is destroyed. Any later physical cleanup remains under the
manual **Limpiar** action in NSC Builder.

## Elementos conservados deliberadamente / Deliberately preserved items

`_targeted` no se excluye como árbol completo. La auditoría encontró que mezcla objetos y productos
históricos con 976 fuentes NativeSharp, 85 proyectos y documentación/repros que forman parte de la
historia verificable del compilador. Ignorarlo o borrarlo en bloque perdería material fuente válido.

`_targeted` is not excluded as a complete tree. The audit found that it mixes historical objects and
products with 976 NativeSharp sources, 85 projects, and documentation/repros that belong to the
compiler's verifiable history. Ignoring or deleting it wholesale would lose valid source material.

Tampoco se modifican `_abi`, `_nscbuilder/history`, `src`, `include`, `tests`, `examples`, `spec`,
`book` ni `mkdown`. El cambio no altera el contenido de ningún baseline ni la instalación ubicada en
`C:\native-sharp`.

The change also leaves `_abi`, `_nscbuilder/history`, `src`, `include`, `tests`, `examples`, `spec`,
`book`, and `mkdown` untouched. It does not alter any baseline content or the installation located at
`C:\native-sharp`.

## Motivo / Rationale

Un directorio ignorado que ya está rastreado continúa apareciendo como modificado: `.gitignore`
sólo gobierna archivos no rastreados. Por eso la corrección requiere las dos partes inseparables:
declarar las exclusiones y retirar los productos existentes del índice. A partir de ese punto, una
compilación o limpieza puede cambiar miles de artefactos sin contaminar el estado de las fuentes.

An ignored directory that is already tracked continues to appear as modified: `.gitignore` only
governs untracked files. The correction therefore requires both inseparable parts: declaring the
exclusions and removing existing products from the index. From that point onward, a build or cleanup
can change thousands of artifacts without polluting source status.

## Validación / Validation

Se comprobó con `git check-ignore` que los tres árboles de pruebas y el bytecode Python quedan
cubiertos. Se comprobó también que los archivos físicos siguen presentes después de retirarlos del
índice. No se ejecutó baseline, compilación ni limpieza destructiva.

`git check-ignore` confirmed coverage for all three test trees and Python bytecode. The physical
files were also confirmed to remain present after index removal. No baseline, build, or destructive
cleanup was run.

## Commit de referencia / Reference commit

### Título / Title

```text
Deja fuera del índice los artefactos generados / Keep generated artifacts out of the index
```

### Descripción / Description

```text
ES:
- ignora los árboles CMake/CTest regenerables y el bytecode Python
- retira los productos existentes sólo del índice sin borrarlos del disco
- elimina el ruido de compilación del estado de Git
- conserva _targeted porque contiene repros, fuentes y evidencia versionada

EN:
- ignore regenerable CMake/CTest trees and Python bytecode
- remove existing products from the index without deleting them from disk
- eliminate build noise from Git status
- preserve _targeted because it contains versioned repros, sources, and evidence
```
