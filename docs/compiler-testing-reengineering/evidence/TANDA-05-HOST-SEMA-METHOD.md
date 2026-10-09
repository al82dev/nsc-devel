# Evidencia de método — Tanda 05, Host.Sema

## Universo leído

Se leyeron completos y en orden físico los 30 archivos de `Host.Sema`: 17 headers y 13 sources.

| Base | Archivos | Líneas | Bytes | Huellas congeladas |
|---|---:|---:|---:|---|
| `ConstantValue` | 2 | 104 | 2,853 | confirmadas |
| `ConversionInfo` | 1 | 30 | 526 | confirmadas |
| `ConversionKind` | 2 | 63 | 1,891 | confirmadas |
| `EffectContext` | 1 | 85 | 2,181 | confirmadas |
| `ExpressionInfo` | 1 | 65 | 1,634 | confirmadas |
| `OwnershipKind` | 2 | 30 | 581 | confirmadas |
| `ResolvedOperatorInfo` | 1 | 18 | 364 | confirmadas |
| `Scope` | 2 | 65 | 1,383 | confirmadas |
| `SemanticAnalyzer` | 2 | 3,704 | 136,630 | confirmadas |
| `SemanticDumper` | 2 | 174 | 5,192 | confirmadas |
| `SemanticModel` | 2 | 96 | 2,815 | confirmadas |
| `Symbol` | 2 | 117 | 3,855 | confirmadas |
| `SymbolDumper` | 2 | 63 | 1,487 | confirmadas |
| `SymbolKind` | 2 | 42 | 856 | confirmadas |
| `Type` | 2 | 207 | 6,178 | confirmadas |
| `TypeContext` | 2 | 210 | 5,314 | confirmadas |
| `TypeKind` | 2 | 70 | 1,598 | confirmadas |
| **Total** | **30** | **5,143** | **175,338** | **confirmadas** |

La agrupación anterior es de revisión, no de ownership contractual. Para cada archivo se conservaron ID,
SHA-256, tamaño y líneas del manifiesto de la revisión congelada.

## Lectura del archivo dominante

`src/Sema/SemanticAnalyzer.cpp` contiene 3,531 líneas y 129,156 bytes. Se leyó completo en bloques físicos
contiguos: 1–300, 301–600, 601–900, 901–1200, 1201–1500, 1501–1900, 1901–2300, 2301–2700,
2701–3100 y 3101–3531. Después se hicieron extracciones independientes de:

- 114 apariciones de `DiagnosticId`;
- 56 despachos explícitos de `AstNodeKind` y tres ownership qualifiers;
- la matriz completa de tokens usada por unary, binary, compound assignment y constant folding;
- 89 definiciones físicas del analyzer, incluidas cinco funciones internas del translation unit.

La extracción no sustituyó la lectura: se usó después para contrastar omisiones.

## Extracción y contraste

1. Se identificaron 22 tipos definidos en headers, excluyendo forward declarations.
2. Se identificaron 178 definiciones físicas en sources, incluidos destructores y helpers internos.
3. Se identificaron 14 funciones/métodos inline con conducta en headers.
4. El inventario resultante contiene 214 símbolos, `SYM-001053..SYM-001266`.
5. Cada símbolo tiene exactamente un contrato propietario entre `CS-SEMA-001..030`.
6. Se diseñaron 321 casos parametrizables: 313 PR y ocho NIGHTLY.
7. Los casos NIGHTLY se reservan a profundidad, grafos fabricados o lifetime/destrucción; ninguna regla semántica
   esencial depende exclusivamente de una corrida NIGHTLY.
8. Se contrastaron filas CSV/JSON, huellas, referencias y paridad Markdown/CSV.

## Fronteras revisadas

- valores constantes, conversiones, ownership y effects;
- categorías de expresión, scopes, símbolos, tipos e interning;
- modelo semántico, declaraciones globales, structs, enums y funciones;
- variables, control de flujo, assertions e inline assembly;
- literals, operadores, assignment, calls, indexing, members y casts;
- layout, C ABI, memoria explícita, resolución de tipos y conversions;
- evaluación constante, fallthrough y dumpers.

## Separación de hechos y política

Se conservaron como implementación vigente las etiquetas históricas, conceptos `unsafe`/ownership y rechazos
de la ruta single-file. Las decisiones posteriores del usuario se registran como contradicciones; no se
reescribió el código ni se presentó la política deseada como conducta actual.

## Límites del trabajo

No se cambió ni ejecutó código, pruebas, builders o infraestructura. No hubo build, baseline, coverage,
instalación, deploy, escritura en C: ni remotos. Las 684 pruebas históricas y los seis cambios preexistentes
permanecieron intactos.
