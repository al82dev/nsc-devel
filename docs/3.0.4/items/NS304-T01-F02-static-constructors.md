# NS304-T01-F02 — Constructores estáticos por tipo

## Estado

`CERRADO` el 2026-10-12. La declaración llega desde Parser hasta MIR como una
función estática sin receptor y queda separada de los constructores invocables
por `new`, `base(...)` y `this(...)`.

## SCOPE LOCK

- Objetivo: gramática y semántica de un constructor estático por tipo.
- Productores: Parser, modelo de miembros, construcción, cuerpo y HIR/MIR.
- Contrato/prueba: `CL-LANG-012` / `TL-LANG-295`.
- Revisión base: `b2a933a20b` en `development`.
- Protegido: protocolo once, triggers de inicialización, inicializadores de
  módulo, fallo terminal, metadata, runtime ABI, CMake, toolchain y Builder.

## Contrato cerrado

La forma válida es `static Tipo() { ... }`. Puede coexistir con constructores
de instancia, conserva cuerpo obligatorio y puede declarar `noheap`. No admite:

- accesibilidad;
- parámetros;
- initializer `base(...)` o `this(...)`;
- dos constructores estáticos en el mismo tipo;
- `this` ni acceso implícito a estado de instancia.

El AST conserva `ConstructorDeclaration` más `KwStatic`. El símbolo semántico
lleva `isStatic=true`, `hasImplicitThis=false`, queda fuera de
`constructorsOfType` y usa identidad física `__nsc_cctor_*`. HIR/MIR lo bajan
como `void` estático y no insertan chaining ni inicializadores de campos de
instancia.

F02 no define cuándo se invoca la función. Los triggers, once thread-safe,
reentrancia, ciclos y fallo terminal pertenecen a F04/F05; por eso
`static-constructors` continúa `PlannedFor304` en el manifest de superficie.

## Evidencia puntual

- Reconstrucción enfocada de `nsc_component_tests`: 73/73 pasos derivados por
  Ninja debido al cambio de firma privada en `Parser.h`; sin reconfigurar.
- `TL-LANG-295`: `PASS`, 102329 microsegundos. Valida positivo hasta MIR,
  coexistencia con ctor de instancia y cinco negativos exactos.
- Guard `TL-LANG-160`: `PASS`, 59149 microsegundos; impide publicar antes de
  tiempo la feature completa como `Implemented`.

## Gate F02

```text
grammar = PASS
member_identity = static, no-this
construction_overload_set = instance-only
hir_mir_body = PASS
negative_contracts = 5/5
surface_promotion = deferred-to-F08
```

Siguiente acción determinista: `NS304-T01-F03`, inicializadores de módulo con
orden y dependencias explícitos.
