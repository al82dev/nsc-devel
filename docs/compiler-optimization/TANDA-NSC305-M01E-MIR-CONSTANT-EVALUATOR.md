# Tanda NSC 3.0.5 M01E — extracción de `MirConstantEvaluator`

Fecha: 2026-10-17
Rama: `development`
Revisión de partida: `fff78519c86a7f2ed05e132358a7174531bc24a2`

## SCOPE LOCK

| Dimensión | Contrato de la tanda |
|---|---|
| Objetivo | Extraer físicamente el evaluador de constantes HIR→MIR desde `MirBuildCoordinator.cpp` hacia `MirConstantEvaluator.cpp`. |
| Naturaleza | Refactor por movimiento puro. |
| Conducta congelada | Sintaxis, semántica, orden de evaluación, recursión vigente, conversiones, aritmética definida, diagnósticos, API, ABI, NSMETA, MIR y dumps. |
| Archivos de producto autorizados | `src/Frontend/MIR/MirBuildCoordinator.cpp`, `src/Frontend/MIR/MirConstantEvaluator.h`, `src/Frontend/MIR/MirConstantEvaluator.cpp`. |
| Infraestructura autorizada | Una entrada en `nsc_frontend` y una entrada espejo en `nsc_component_tests`. |
| Documentación autorizada | Este expediente y la hoja M01E del tracking externo. |
| Protegidos | Parser/R01D, `tests/unit/ProjectMethodBodyModelTests.cpp`, LLVM, HIR, semántica, fixtures y expectativas existentes. |
| Prohibido | Migrar todavía la recursión, corregir fallos Stage 37 adyacentes, agregar lenguaje, cambiar contratos o ejecutar baseline/suite completa. |
| Terminación | Fuente registrada dos veces, `nsc` y targets focales construidos, pruebas M01E aplicables en PASS, cero funciones nuevas mayores de 60 ejecutables, diff auditado y commit local aislado. |

## Inventario movido

La unidad extraída conserva cuatro entradas observables:

- `MirConstantEvaluator::integerValue`;
- `MirConstantEvaluator::floatingValue`;
- `MirConstantEvaluator::booleanValue`;
- `MirConstantEvaluator::stringValue`.

También conserva los helpers de búsqueda de campos constantes, signo/ancho/máscara,
normalización entera y flotante, división/módulo con signo y comparaciones. La coordinación sólo
cambia cuatro llamadas durante la materialización de campos `const`; el orden de iteración de
campos e inicializadores no cambia.

M01E no migra la recursión. Los descensos a hijos continúan llamando al mismo evaluador en el
mismo orden. La eventual transformación algorítmica requiere una hoja distinta y evidencia
contrafactual propia.

## Descomposición mecánica y límite de tamaño

Las funciones históricas de entero y booleano excedían la cota física si se trasladaban como un
monolito. Se separaron por la misma discriminación que ya existía: conversión, unario, binario,
división/módulo con signo y comparación. No se añadió ninguna rama ni política.

Medición de las 20 funciones definidas en la unidad:

- máximo de líneas físicas no vacías dentro de un cuerpo: 45;
- cota superior conservadora de líneas ejecutables: 27;
- funciones por encima de 60 ejecutables: 0.

La auditoría de identidad contó exactamente las clases de nodo, operadores y tipos builtin antes
y después del movimiento. Los tres multiconjuntos son idénticos:

- ocho clases HIR, con las mismas 19 apariciones;
- veinte operadores, con las mismas 46 apariciones;
- diecisiete claves builtin, con las mismas 27 apariciones.

Los símbolos históricos `integerConstantValue`, `floatingConstantValue`,
`booleanConstantValue`, `stringConstantValue` y sus helpers quedaron en cero apariciones dentro
de `MirBuildCoordinator.cpp`. `MirConstantEvaluator` tiene una sola definición.

## Infraestructura

No se creó prueba, ejecutable, caso, catálogo, manifiesto ni ruta default. La infraestructura
aplicable a una fuente nueva queda cerrada por:

1. `CMakeLists.txt`: una entrada en `nsc_frontend`;
2. `tests/compiler-suite/CMakeLists.txt`: una entrada en `nsc_component_tests`;
3. `validate_source_registration.py`: PASS, exit 0;
4. conteo estático: una aparición exacta en cada lista.

## Validación focal

Todos los temporales y logs se conservaron en `A:\temp`.

| Gate | Resultado | Evidencia |
|---|---|---|
| Build `nsc` | PASS, exit 0, 15.39 s | `A:\temp\m01e-build-nsc.log` |
| Build `nsc_stage37_static_initialization_tests` | PASS, exit 0, 8.84 s | `A:\temp\m01e-build-stage37.log` |
| Build `nsc_component_tests` | PASS, exit 0, 15.65 s | `A:\temp\m01e-build-component-tests.log` |
| `nsc.stage37.mir-dump` | PASS 1/1, 0.09 s | `A:\temp\m01e-ctest-stage37-mir-dump.log` |
| `nsc.stage37.llvm` + `nsc.stage37.object` | PASS 2/2, 0.05/0.08 s | `A:\temp\m01e-ctest-stage37-llvm-object.log` |
| `TL-LANG-428` | PASS, exit 0, 593,507 us | `A:\temp\m01e-tl-lang-428.log` |

El proyecto Stage 37 focal materializa `Seed = 2 + 3`, `TwiceSeed = Seed * 2` y
`Enabled = (Seed < TwiceSeed) && true`, y después atraviesa dump MIR, lowering LLVM y emisión de
objeto. `TL-LANG-428` cubre materialización de una constante decimal de 128 bits.

La batería unitaria amplia `nsc.stage37.static-initialization` ejecutó 55 comprobaciones: 51
pasaron y cuatro fallaron. La función `testConstEvaluation` completa no produjo ningún fallo:
enteros, referencias a const, booleanos, división/módulo con signo, wrap, shift, floating, string
y LLVM const conservaron sus resultados. La tanda no presenta el target amplio como PASS.

## Fallos Stage 37 separados de M01E

Los cuatro checks rojos corresponden a tres causas distintas y se conservan como hojas
correctivas explícitas; no se corrigen dentro del commit de movimiento:

1. **Dump member flags.** `testMemberFlagsAndStaticAbi` espera la subcadena contigua
   `const=true constant=true value=5`. El dump real contiene
   `const=true imported=false constant=true value=5`; las dos mitades semánticas esperadas sí
   están presentes y el valor evaluado es 5. Es una expectativa textual desactualizada por el
   campo intermedio `imported=false`.
2. **Initializer inheritance/delegation.** `testInitializerOrderingAndTypeInit` llama
   `compileProjectOopMir` sobre una fixture sin campos `const`; recibe `false` con error vacío.
   No alcanza las aserciones posteriores. Debe diagnosticarse como productor independiente.
3. **LLVM cctor panic profile.** `testTerminalFailureUsesPanicProfile` compila MIR y baja LLVM,
   pero falla dos checks textuales: failure pad/state=3/rethrow y orden
   failed-block→RaiseException→trap→cleanup. Es una sola causa LLVM con dos manifestaciones.

Procedencia: la expectativa de dump proviene de `0fadb35376`/`c0b7f713de`; la fixture de
ordering de `c0b7f713de` con extensión cctor `e5cf52caf7`; y los dos checks LLVM de
`00e50faa92`. El dump real quedó en `A:\temp\m01e-stage37-mir-actual.log`.

## Resultado

M01E conserva el contrato del evaluador, reduce la responsabilidad física de
`MirBuildCoordinator.cpp`, cierra la infraestructura doble y tiene evidencia focal directa en
PASS. Los tres defectos Stage 37 adyacentes permanecen visibles y separados; no invalidan la
paridad de la evaluación constante demostrada, pero sí impiden declarar verde el target amplio.
