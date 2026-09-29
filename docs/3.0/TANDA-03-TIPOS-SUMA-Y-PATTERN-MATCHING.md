# Tanda 03 — Tipos suma y pattern matching

> **Estado real 2026-09-29:** `IMPLEMENTING`, con una primera vertical productiva integrada por la
> tanda operativa 09. Ya no es únicamente un helper C++: `variant` y el `match` statement seguro
> cruzan parser, generics, names, members, method bodies, layout, HIR, MIR, LLVM, PE y NSMETA.

## Sintaxis integrada

```nativesharp
public variant Result<T, E>
{
    Ok(T),
    Error(E)
}

match (operation())
{
    Ok(value) => { consume(value); },
    Error(error) => { report(error); },
    _ => { recover(); }
}
```

Un caso puede no tener payload o tener exactamente un tipo de payload. Varios valores se expresan
con un tuple estructural, por ejemplo `Pair((int, int))`. Los nombres `variant` y `match` son
contextuales y requieren `<Language>3</Language>`.

## Lo que ya es productivo

- El tag usa `byte`, `ushort` o `uint` según el número de casos.
- Todos los payloads comparten una sola unión física in-place; no hay boxing ni allocation oculta.
- Las fábricas de caso inicializan primero un valor cero, escriben tag y luego el payload activo.
- `IsCase()` y `CaseTag()` son lecturas readonly. El extractor físico es reservado: una llamada
  escrita por el usuario se rechaza y sólo `match`, después de probar el tag, puede invocarlo.
- Variants unit, genéricos, anidados, con tuple payload y con diferentes alineaciones cruzan O0.
- `match` statement evalúa el governing expression exactamente una vez, crea bindings const con
  scope de brazo y baja a un árbol ordinario `if/else` visible en HIR/MIR.
- Se diagnostican casos duplicados, brazos duplicados, wildcard duplicado, brazos posteriores al
  wildcard, guards todavía no materializados y matches sin wildcard seguro.
- `.lib + .nsmeta` transporta un variant genérico y un consumidor precompilado puede construirlo y
  hacer match. Un variant C-compatible usado por `export extern C` genera `struct + union`, tag
  mínimo y `static_assert` de tamaño, alineación y offsets en el host header.

## Invariantes de seguridad

- Esta vertical admite únicamente payloads con lifetime trivial. `owned`/`borrowed` explícitos y
  tipos que poseen storage dentro de la unión son rechazados por las reglas existentes de raw
  union. No se afirma drop seguro para move-only antes de T06 operativo (tandas 13–14).
- Un `match` sin catálogo cerrado disponible desde metadata exige un wildcard final. Esto es más
  estricto que el diseño final, pero nunca acepta silenciosamente un match parcial.
- `pack`, `align`, `size`, `readonly` y `borrowed` no pueden alterar el layout/lifetime canónico de
  un variant.
- El nombre del carrier físico depende del nombre del variant, no de file-id ni byte offset; mover
  la declaración no cambia por sí solo su identidad ABI.

## Pendiente para cerrar T03 completo

- patterns literal/range/record/tuple anidado/or-pattern y bindings `ref`/`move`;
- exhaustividad/usefulness por catálogo cerrado sin wildcard, incluida metadata no genérica;
- `if let`, `while let`, `let else`, guards y narrowing;
- active-payload drop flags, unwind y destrucción exactamente una vez para ownership no trivial;
- presentación semántica del caso activo en debugger/LSP.

Estas piezas conservan sus dependencias maestras: ownership/drop en tandas 13–14, metadata/tooling
en 18/29 y matriz combinatoria en 30. No se trasladan artificialmente a T15 ni se declaran hechas.

## Evidencia dirigida

- fixture `tests/cc1/c59_ns3_variants`: O0, nested match, unit/generic/nested/tuple/alignment variants,
  evaluación única y salida 42;
- dumps de layout: `Result<int,int>` mide 8/align 4 y `Wide` mide 16/align 8 con tag byte;
- consumidor LibraryReference de `Result<T,E>`: `.lib + NSMETA 1.24`, salida 42;
- host header testigo: `uint8_t` tag, payload `union`, offsets 0/8 y asserts C;
- probes negativos de edition 2, duplicado, wildcard ausente y extractor directo.

Sin baseline, sin CTest general, sin O1/O2/O3 y sin instalación/deploy.
