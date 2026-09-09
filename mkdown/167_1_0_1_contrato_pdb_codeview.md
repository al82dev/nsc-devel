# 167 — Contrato PDB/CodeView para depuración nativa / Native PDB/CodeView debug contract

## Resumen / Summary

Este cambio inicia el carril urgente de depuración estándar de NativeSharp. Define el contrato que
deben compartir `nsc`, el linker PE y el debugger sin introducir un sidecar privado.

This change starts NativeSharp's urgent standards-based debugging track. It defines the contract
shared by `nsc`, the PE linker and the debugger without introducing a private sidecar.

## Archivos creados / Files created

### `spec/NativeSharp_Debug_Artifact_Contract_1_0.md`

- fija CodeView en COFF y PDB/MSF como formatos normativos;
- separa la selección de optimización de la selección de información de depuración;
- reserva `-g0`, `-gline-tables-only` y `-g` con semántica explícita;
- exige `<nombre>.pdb` y un registro RSDS correspondiente para ejecutables;
- define el perfil mínimo de archivo, función y línea;
- conserva como meta completa scopes, parámetros, locales, tipos, defranges, lambdas, callbacks y
  funclets SEH;
- exige validación automatizada en IR, COFF, PDB y con `llvm-pdbutil` para O0 y O2.

- establishes CodeView in COFF and PDB/MSF as the normative formats;
- keeps optimization and debug-information selections independent;
- reserves `-g0`, `-gline-tables-only` and `-g` with explicit semantics;
- requires `<name>.pdb` and a matching RSDS record for executables;
- defines the minimum file/function/line profile;
- retains scopes, parameters, locals, types, defranges, lambdas, callbacks and SEH funclets as the
  complete-profile goal;
- requires automated validation at the IR, COFF and PDB layers with `llvm-pdbutil` at O0 and O2.

### `mkdown/167_1_0_1_contrato_pdb_codeview.md`

Registra los archivos, decisiones, alcance y razón de este cambio según la política local de
historial. Este documento no declara implementado el contrato: la implementación se cerrará en
commits posteriores con evidencia ejecutable.

Records the files, decisions, scope and rationale of this change under the local development
history policy. This document does not claim the contract is implemented; later commits will close
the implementation with executable evidence.

## Decisiones de diseño / Design decisions

Se eligió PDB estándar porque conserva interoperabilidad con Visual Studio, WinDbg, DbgHelp,
LLVM/lld y herramientas externas. Un formato privado trasladaría complejidad al debugger y haría
que cada consumidor dependiera de NativeSharp.

Standard PDB was selected to preserve interoperability with Visual Studio, WinDbg, DbgHelp,
LLVM/lld and external tools. A private format would move complexity into the debugger and make
every consumer depend on NativeSharp.

La primera entrega ejecutable será deliberadamente vertical: metadatos LLVM válidos, CodeView en
el objeto, PDB enlazado y prueba de archivo/función/línea. Los locales y tipos completos se agregan
después sobre ese camino comprobado.

The first executable delivery is deliberately vertical: valid LLVM metadata, CodeView in the
object, a linked PDB and file/function/line evidence. Complete locals and types follow on top of
that proven path.

## Commit de referencia / Reference commit

**Título ES:** Define el contrato estándar PDB/CodeView de NativeSharp  
**Title EN:** Define NativeSharp's standard PDB/CodeView contract

**Descripción ES:** Define perfiles de depuración independientes de optimización, artefactos PDB y
RSDS estándar, el perfil mínimo archivo/función/línea, la meta completa de locales y tipos y las
pruebas obligatorias con LLVM, lld-link y llvm-pdbutil.

**Description EN:** Define optimization-independent debug profiles, standard PDB and RSDS
artifacts, the minimum file/function/line profile, the complete locals-and-types target, and the
required LLVM, lld-link and llvm-pdbutil evidence.
