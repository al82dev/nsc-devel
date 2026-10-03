# NS3-T05-F07 — Operadores mediante traits / Trait-based operators

## Estado

`TERMINADA`. Los operadores de usuario se resuelven por identidades `lang:*` estables y witnesses
estáticos de trait, sin alterar las reglas intrínsecas de built-ins ni convertir el cortocircuito
en una llamada eager.

## Qué se hizo

- `TraitSemantics` concentra veinte contratos reversibles entre escritura, aridad, lang-item y
  nombre ABI; parser, miembros y resolución consumen una sola tabla.
- Cada operador declarado conserva su lang-item. `ProjectInterfaceModel` publica el requisito de
  trait y el miembro concreto que lo implementa.
- La resolución consulta witnesses coherentes de ambos operandos y conserva como compatibilidad
  los operadores directos ya existentes. La selección inviable o ambigua termina ahí y no cae por
  accidente a una operación builtin.
- Operadores con parámetros `in`/`ref` reciben direcciones físicas sin exigir keywords en la
  expresión. Se exige identidad/addressability; `ref` exige escritura y `out` se rechaza.
- HIR baja esos argumentos en contexto de dirección y LLVM conserva el ABI estático. La política
  `forceinline` llega a LLVM y elimina la llamada al witness en O0.
- `&&`, `||` y `??` producen un diagnóstico específico que preserva su semántica intrínseca de
  control de flujo.
- El manifiesto `NSLANGSURFACE 3.0` publica `trait-operators=implemented`.

## Archivos principales

- `include/nativesharp/Language/TraitSemantics.h` y `src/Language/TraitSemantics.cpp`: tabla
  canónica y consultas.
- `ProjectMemberModel`, `ProjectInterfaceModel` y `ProjectMethodBodyModel`: identidad, witnesses,
  lookup, overloads, referencias y diagnósticos.
- `src/Frontend/ProjectOopHirModel.cpp`: lowering físico de operandos por referencia.
- `src/Parser/Parser.cpp`: nombres canónicos, restricciones y políticas inline.
- `Language3Surface`: contrato mecánico de disponibilidad.
- `tests/unit/NativeSharp3T05OperatorTraitTests.cpp`: batería focalizada.
- `tests/cc1/c70_ns3_operator_traits/`: fixture permanente O0/NSLink, exit 42.
- `docs/3.0/language/operator-traits.md`: contrato bilingüe y tabla normativa.

## Mejora obtenida

Una API numérica o algebraica puede expresar sus operaciones como contratos estáticos genéricos,
obtener selección coherente y mantener costo de llamada estática/inlinable. El lenguaje ya no
depende de que varios subsistemas recuerden convenciones `op_*` separadas, y los errores de
ambigüedad o referencia aparecen en Sema antes del backend.

## Validación dirigida

- `nsc_ns3_t05_operator_trait_tests`: 20/20, incluido productor/consumidor `.lib/.nsmeta` 1.32.
- `cc1-c70-ns3-operator-traits`: 6/6, O0/NSLink, exit 42.
- Regresiones focalizadas: traits 9/9; declaraciones trait 14/14; coherence 11/11; extensiones
  12/12; operadores Stage 38 96/96; forceinline 14/14; superficie 3.0 27/27.
- `nsc` construido; contrato de runtime host estático: passed.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
