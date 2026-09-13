# 292 — Inmediatos y memoria de asm / Assembly immediate and memory operands

## Resultado / Result

Los inputs asm aceptan `"i"` para constantes integrales y `"m"` para almacenamiento direccionable;
los outputs aceptan `"=m"` y `"+m"`. Valores de ejecución usados como inmediatos y temporales usados
como memoria se diagnostican antes del lowering. / Assembly inputs accept `"i"` for integral
constants and `"m"` for addressable storage; outputs accept `"=m"` and `"+m"`. Runtime values used
as immediates and temporaries used as memory are diagnosed before lowering.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - Separa `addressable` de `assignable` en `ExpressionResult`.
- `src/Parser/Parser.cpp` e `include/nativesharp/Parser/Parser.h`
  - Generalizan `parseInlineAssemblyConstraint()` y admiten `i`, `m`, `=m` y `+m`.
- `src/Sema/SemanticAnalyzer.cpp`
  - Valida constante integral y almacenamiento direccionable en la tubería standalone.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - Marca locales, parámetros, campos, elementos raw y dereferences como ubicaciones reales.
  - Valida los mismos contratos en proyectos sin confundir properties con memoria.
- `tests/unit/IntegratedAssemblyGrammarTests.cpp`
  - Cubre AST, inmediato dinámico, constante flotante, temporal de memoria y local direccionable.
- `docs/1.0.1`
  - Actualiza especificación, contratos, mapas, estado, changelog y roadmap.

## Implementación y decisiones / Implementation and design decisions

`addressable` es independiente de `assignable`: una ubicación puede ser readonly y seguir teniendo
dirección, mientras una property con setter puede aceptar asignación sin exponer almacenamiento.
Esta distinción evita que el futuro lowering reciba una promesa falsa. / `addressable` is independent
from `assignable`: readonly storage still has an address, while a property setter can accept an
assignment without exposing storage. This prevents a false promise from reaching future lowering.

Los inmediatos se restringen por ahora a constantes integrales, que tienen representación directa y
determinista en assembly. Direcciones simbólicas y relocations requieren un contrato separado. /
Immediate operands are currently restricted to integral constants with a direct deterministic
assembly representation. Symbolic addresses and relocations require a separate contract.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: PASS: 33 checks
```

No se ejecutó baseline en este commit. / No baseline was run for this commit.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Agrega operandos inmediatos y de memoria a asm / Add immediate and memory asm operands`

**Descripción / Description**

`Valida constantes integrales, rastrea almacenamiento direccionable y rechaza temporales o properties como memoria. / Validate integral constants, track addressable storage, and reject temporaries or properties as memory.`
