# Tanda 8: enum atómico interno en biblioteca precompilada

El fixture de contratos internos incorpora `_Atomic(enum AtomicMode)` y una
asignación compuesta usada desde el código NativeSharp del productor. Una
función `static inline` del mismo encabezado C verifica el valor actualizado.

El control C y los consumidores NativeSharp directo, precompilado y a través
de DLL pasaron O0–O3. Durante el consumo precompilado se oculta el encabezado
del productor; la comparación previa de IR y los controles existentes también
pasaron. Evidencia: `A:\nsc-interop-build\tanda08-static-contratos-31639-402`.
Esto no verifica exportar la variable interna como API pública de metadata ni
todos los tamaños y operaciones.
