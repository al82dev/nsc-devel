# Arreglos genericos de decimal

Base de la campana: 152654e5a0e2d84d4c2f245ea7590488f0497430. Caso: ejemplo 70,
decimal_dynamic_matrix, del reporte A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md.

## Alcance
Corregir la reproduccion de arreglos de decimal en la fase MIR. No se modifica el servicio
compartido de layout, ni los proyectos originales de ejemplo, A:/nsc-devel, C:/native-sharp,
LLVM/Clang, Daikon o la interoperabilidad C/C++; no se usan sus builds ni se hace merge, rebase,
push o despliegue. Builds, temporales y evidencia permanecen en A. Las decisiones de diseno
siguen pendientes de autorizacion; baseline y bateria completa pertinentes siguen pendientes.

## Causa y cambio
La reproduccion fallo con Stage 41 cannot determine native array element layout for
builtin:decimal. MirNodeMachine::lowerGroup13 consulta elementLayout para las operaciones de
arreglo; el servicio compartido no devuelve layout para decimal. El destino Windows x64 define
decimal de 16 bytes con alineacion 8. En ese camino de arreglo, y solo cuando la consulta falla
para decimal, se usa el layout definido por TargetDataLayout. No se cambia el contrato de otros
tipos ni se generaliza el servicio compartido.

## Evidencia y limites
La nueva prueba nsc.compiler.decimal-generic-array fallo antes por ese diagnostico y paso
despues: crea Matrix<decimal>, almacena y lee dos elementos y comprueba sus valores enteros.
La reproduccion historica, copiada sin alterar sus fuentes, compilo y ejecuto con salida 0.
La regresion nsc.sdk.dictionary-string-update tambien paso con el compilador actualizado.
Los logs estan en A:/nsc-fixes-evidencia/baseline-decimal-array.log,
fixed-decimal-array.log, dictionary-after-decimal.log y cases/70-decimal/.

La prueba existente nsc.stage41.arrays-strings-core no se ejecuto: su ejecutable no enlazo por
PrecompiledSemanticSnapshotCodec::decode no resuelto. Ver build-stage41-tests.log y
stage41-array-test.log. La causa y su relacion con el cambio no estan demostradas. La bateria
completa, el analisis estatico integral, otras rutas de layout y otras plataformas no se
verificaron; no se presenta esta prueba dirigida como cierre general.
