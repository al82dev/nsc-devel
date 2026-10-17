# Tanda NSC305-C04F — Identidad MIR indexada por callable

> Correctiva post-L05 de la campaña NativeSharp Compiler 3.0.5.
> Rama: `development`. Base: `c4c961479`.

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Problema medido | Un build warm j16 con 12/12 objetos restaurados conserva ~1,477 s en `llvm-object-generation`. |
| Causa auditada | Cada unidad vuelve a serializar en serie el modelo MIR común y las declaraciones que comparte con otras unidades. |
| Objetivo | Serializar una vez el modelo común y cada callable; componer identidades compactas por unidad sin debilitar invalidación. |
| Producción permitida | `ProjectOopMirCacheIdentity.{h,cpp}` y su consumidor inmediato `ProjectOopObjectGenerator.cpp`. |
| Pruebas permitidas | Pruebas focales existentes de identidad MIR e invalidación precisa O0. |
| Protegido | Parser, sintaxis, semántica, diagnósticos, HIR/MIR, lowerer LLVM, ABI, layout, ownership, NSMETA, driver y productos externos. |
| Prohibido | Baseline, suite completa, deploy, instalación, red Git, rutas fuera de `A:` y versionamiento NSMETA. |

## 2. Diseño

- El índice calcula una huella SHA-256 del modelo común exactamente una vez.
- Calcula por ordinal una huella de declaración y una huella de definición. La segunda incluye
  cleanup, exception flow y el árbol MIR completo.
- Cada unidad conserva el orden y los ordinals de sus declaraciones, distingue declaración de
  definición y concatena sólo las huellas correspondientes.
- La identidad privada cambia de esquema deliberadamente, por lo que el primer build posterior es
  cold. No cambia ningún formato público ni la validez de objetos ya publicados fuera de la caché.
- SHA-256 sólo compacta material ya incluido; ningún dato que antes participaba en invalidación se
  omite.

## 3. Gates

1. La API histórica y el índice producen la misma identidad compacta para la misma selección.
2. Un cambio de un cuerpo conserva el plan y produce exactamente un miss; la repetición tiene todos
   los hits.
3. El target focal compila y la prueba O0 pasa.
4. En el perfil c23 warm j16 continúan 12 hits/0 misses y disminuye materialmente el tiempo LLVM.
5. `git diff --check` pasa; commit local aislado; sin baseline ni suite completa.

## 4. Implementación

- `ProjectOopMirCacheIdentityIndex` calcula una vez la huella común y las huellas de declaración y
  definición de cada callable. El generador crea un índice por compilación y compone cada clave de
  unidad con hashes compactos.
- `IdentityWriter` deja de usar `std::ostringstream`: reserva un buffer continuo, convierte enteros
  con `std::to_chars` y agrega texto directamente. Esto conserva toda la información invalidante y
  elimina locale, formato de streams y asignaciones por escalar.
- `writeNode` usa una pila explícita. Empuja hijos en orden inverso para emitir exactamente la misma
  preorden que la versión recursiva, incluida la marca de presencia y la cardinalidad de hijos.
- La función histórica `projectOopMirCacheIdentity` delega al índice, por lo que no se rompe su API.
- El esquema privado pasa a `Indexed.v1`; el primer build es cold por diseño. No cambia NSMETA ni
  ningún artefacto público.

## 5. Validación funcional

Fecha: 2026-10-17.

1. `nsc_namespace_function_llvm_tests`: PASS 10/10, exit 0.
2. `nsc_project_oop_lowering_tests --precise-incremental-invalidation`: PASS 13/13, exit 0.
3. La segunda prueba conserva el contrato L05: una edición de cuerpo produce un miss y la repetición
   restaura todos los objetos.
4. `nsc` Release compila y pasa su contrato de runtime host estático.
5. `git diff --check`: PASS.

## 6. Evidencia cuantitativa

Corpus: `tests/cc1/c23_views_and_lifetimes`, O0/g0, j16, misma máquina y cinco muestras warm después
de una preparación cold. Todas las muestras warm informaron 12 hits y 0 misses.

| Versión | Cold LLVM | Warm mediana | Warm p95 | Warm RSS mediana |
|---|---:|---:|---:|---:|
| Índice con escritor de streams | 1.514.558 us | 1.484.551 us | 1.504.694 us | 135.262.208 B |
| Índice compacto e iterativo | 565.349 us | 452.361 us | 483.108 us | 136.773.632 B |

La mediana warm disminuye 69,5 %, el p95 67,9 % y el cold 62,7 %. RSS mediana aumenta 1,1 %.
Evidencia final: `A:\temp\nsc305-c04f-profile-final\measurements.csv` y sus seis `nsc.log`.

NSLink no está en el `PATH`; cada muestra completa LLVM y luego se detiene en la publicación EXE.
Por eso esta evidencia prueba el costo e invalidación de la fase corregida, no reclama enlace o
ejecución final. No se buscó NSLink por rutas y no se modificó `PATH`.

## 7. Resultado

La reserialización textual redundante y costosa queda eliminada sin reducir la superficie de la
huella. La ruta warm continúa haciendo una pasada lineal necesaria sobre el MIR para detectar
cambios, pero ya no repite el modelo por unidad ni paga streams/recursión. La correctiva queda
cerrada en su alcance focal; sin baseline, suite completa, deploy ni instalación.
