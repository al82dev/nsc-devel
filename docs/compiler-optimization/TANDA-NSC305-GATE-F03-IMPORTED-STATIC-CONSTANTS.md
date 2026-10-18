# TANDA NSC 3.0.5 · GATE-F03 · constantes estáticas importadas

## SCOPE LOCK

Esta tanda corrige exclusivamente la selección de declaraciones de campos estáticos que realiza
el coordinador de particiones LLVM. No cambia sintaxis, API, ABI, NSMETA, serialización, layout,
diagnósticos, reglas de enlace ni ownership de almacenamiento mutable. No modifica Stage 46 ni
sus expectativas. No ejecuta baseline, batería completa, deploy ni instalación.

## Defecto y causa

`projectOopStaticFieldDeclarationMask` excluía del coordinador todo campo marcado `isImported`.
Esa regla es correcta para almacenamiento estático mutable: su productor conserva el storage y
el consumidor llama al accessor transportado por NSMETA. No es correcta para una constante
importada: `ProjectOopLlvmLowerer::declareStaticFields` materializa en el consumidor una copia ODR
constante, y por ello necesita que el coordinador seleccione su declaración/definición.

La consecuencia observable era que Stage 46 compilaba referencias a enum estático y constantes
numéricas o string importadas, pero NSLink rechazaba sus símbolos de storage como no resueltos.

## Corrección causal mínima

El coordinador selecciona un campo cuando es local o constante:

```text
!field.isImported || field.isConst
```

La rama ordinaria de cada unidad, la clausura de inicializadores, la resolución de accessors, la
emisión ODR y el formato NSMETA permanecen intactos.

## Contrafactual unitario

`ProjectOopLoweringTests` construye un modelo MIR mínimo con tres campos y consulta directamente
la máscara del coordinador:

- mutable local: seleccionado;
- mutable importado: no seleccionado, conserva ownership en el productor;
- constante importada: seleccionada para su copia ODR.

El selector focal es `NSC_PROJECT_OOP_LOWERING_FOCUS=llvm-imported-constant-reachability`.
El archivo ya pertenece al target `nsc_project_oop_lowering_tests` y a su `add_test`; no se crea
fuente ni caso externo, por lo que no aplican CMake adicional, manifiesto, catálogo ni Builder 3.

## Gates de cierre

1. Reconfigurar y construir `nsc`, `nsc_project_oop_lowering_tests` y
   `nsc_stage46_native_libraries_tests` con temporales y logs en `A:`.
2. Ejecutar el contrafactual unitario de reachability.
3. Ejecutar Stage 46 completo. La expectativa histórica de la hoja era `451/451`, pero el
   inventario autoritativo del HEAD de esta tanda contiene 459 checks; el cierre exige `459/459`.
4. Confirmar dentro de Stage 46 las variantes O0/O2 de enum/imported-base, constantes numéricas
   y string en biblioteca estática y DLL.
5. Confirmar el control mutable importado accessor-only.
6. Ejecutar el validador de registro de fuentes y auditoría final de diferencial/whitespace.

NSLink se resuelve únicamente por su nombre desde el `PATH` nativo de Windows; la resolución
esperada para esta máquina es `A:\nslink-devel\bin\release\nslink.exe`.

## Evidencia de cierre

- Reconfiguración `build-clang`: PASS.
- Build focal `nsc`: PASS; contrato de runtime estático: PASS.
- Reconfiguración `build-test-infrastructure`: PASS.
- Build `nsc_project_oop_lowering_tests`: PASS.
- Build `nsc_stage46_native_libraries_tests`: PASS.
- Focus `llvm-imported-constant-reachability`: PASS, 1/1 tanto directo como mediante CTest.
- Stage 46 completo: PASS, 459/459; CTest 1/1 en 40.49 s. Los 451 checks consignados al
  descubrir GATE-F03 eran el inventario histórico anterior y no deben sustituir el conteo actual.
- El gate completo ejecutó las funciones existentes para enum/imported-base O0/O2, constantes
  numéricas y string en static/DLL O0/O2, y el control mutable importado accessor-only.
- Registro canónico de fuentes: PASS.

Logs persistentes:

- `A:\temp\gate-f03-configure-build-clang.log`
- `A:\temp\gate-f03-build-nsc.log`
- `A:\temp\gate-f03-configure-test-infrastructure.log`
- `A:\temp\gate-f03-build-tests-retry.log`
- `A:\temp\gate-f03-reachability-focus.log`
- `A:\temp\gate-f03-reachability-ctest.log`
- `A:\temp\gate-f03-stage46.log`
- `A:\temp\gate-f03-source-registration.log`
