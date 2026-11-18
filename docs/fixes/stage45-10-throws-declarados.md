# Stage 45.10: fixtures con `throws` declarado (regla 3.0 C-13)

## Problema

`nsc_stage45_10_eh_raii_reuse_after_catch_tests` fallaba en 24 de 66 comprobaciones registradas. El fallo apareció
al recompilar el ejecutable con el guardia de binario vigente. Seis fixtures dejaban salir
`InsufficientFundsException` de un callable sin declararla: un `Withdraw` fuera de cualquier
`catch` que la capture. La regla C-13 de 3.0 lo rechaza: "'Withdraw' throws … neither caught here
nor declared by the enclosing callable".

## Corrección

Esos callables declaran ahora `throws InsufficientFundsException`. Es el mismo patrón que ya usan
otros `Main` del archivo y que se aplicó en stage46 (`875c12012`). El caso
`UnmatchedCatchRunsEnclosingCleanup` lo declara en `RunScope`, porque su `catch` no coincide.

No cambian las rutas ejercidas ni los códigos de salida esperados. Un comentario en el archivo
explica la adaptación.

## Prueba

`nsc_stage45_10_eh_raii_reuse_after_catch_tests`: 78/78 en O0 y O2. Son más que antes porque, al compilar, se ejecutan las comprobaciones de ejecución.
