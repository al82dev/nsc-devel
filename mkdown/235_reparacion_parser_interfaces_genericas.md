# 235 — Reparación del parser de interfaces genéricas / Generic-interface parser repair

## Resultado / Outcome

**ES.** El parser acepta la superficie que la entrega 224 ya conectó al sistema de
monomorfización: `interface I<T>`, herencia con argumentos cerrados y cláusulas `where`. El AST
conserva parámetros y constraints antes de que name lookup construya las especializaciones.

**EN.** The parser accepts the surface delivery 224 connected to monomorphization:
`interface I<T>`, inheritance using closed arguments, and `where` clauses. The AST preserves type
parameters and constraints before name lookup builds specializations.

## Causa / Cause

La implementación anterior agregó `Interface` al modelo genérico, pero
`parseInterfaceDeclaration` seguía saltando `parseGenericParameterList`. El token `<` llegaba a una
posición que sólo aceptaba `:` o `{`; el pipeline nunca alcanzaba el código nuevo. Un comentario de
`foreach` y el documento 224 repetían la misma premisa falsa.

The earlier implementation added `Interface` to the generic model, but
`parseInterfaceDeclaration` still skipped `parseGenericParameterList`. The `<` token reached a
position accepting only `:` or `{`, so the pipeline never reached the new code. A `foreach`
comment and the delivery 224 document repeated the same false premise.

## Implementación / Implementation

- Se leen parámetros inmediatamente después del nombre de la interfaz.
- Se leen constraints después de las interfaces base y antes del cuerpo.
- Las especializaciones cerradas conservan el módulo y namespace de su plantilla al sustituirse
  dentro de constraints de otro genérico.
- EBNF, especificación vigente y comentarios describen la implementación real.
- Se reutilizan los parsers comunes; no existe una segunda gramática genérica.

Type parameters are parsed immediately after the interface name, constraints after base interfaces
and before the body. Closed specializations retain their template module and namespace when copied
into another generic constraint. The EBNF/specification now describe the real implementation.
Existing shared parsers are reused; no parallel generic grammar was introduced.

## Archivos / Files

- `src/Parser/Parser.cpp`
- `docs/1.0.1/language/NativeSharp_Grammar_1_0_1.ebnf`
- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`
- `mkdown/224_1_0_1_miembros_interfaces_genericos.md`
- `mkdown/235_reparacion_parser_interfaces_genericas.md`

## Verificación dirigida / Targeted verification

Se ejecutan sólo Stage 44 y Stage 46. No hay baseline intermedio.

Only Stage 44 and Stage 46 are run. There is no intermediate baseline.

## Commit de referencia / Reference commit

**Título ES:** Repara el parser de interfaces genéricas

**Title EN:** Repair generic-interface parsing

**Descripción ES:** Conserva parámetros y constraints de interfaces en el AST y habilita la ruta
genérica local y precompilada ya implementada.

**Description EN:** Preserve interface parameters and constraints in the AST and enable the already
implemented local and precompiled generic path.
