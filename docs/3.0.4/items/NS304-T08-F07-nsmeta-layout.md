# NS304-T08-F07 — NSMETA y cero layout mutation

- **Estado**: CERRADO (static-extension marker + consumo intermodular del operador)
- **Rama**: development

## Cierre (2026-10-14)

- `NamespaceFunctionSymbol.isStaticExtension` + `kMetadataNamespaceFunctionStaticExtension` (bit 6,
  NSMETA 1.32 addendum) distinguen `ns3-extension-static` de la extensión de instancia.
- El productor publica el bit; el consumidor reconstruye `extension T { public static ... }` con
  todos los parámetros explícitos (sin descartar el primer parámetro como receptor).
- Evidencia O0: `TL-LANG-399` — el operador de extensión desde una librería referenciada resuelve
  en el consumidor (`a + b` = 5, exit 5). Guards `TL-LANG-394/389` PASS.

## Auditoría NSMETA (resumen)

1. Extensión de instancia (F01/F02): `kMetadataNamespaceFunctionExtension` (NSMETA 1.32) → publicada.
2. Extensión estática (F03) + operador (F04): bit 6 "static" → publicada + reconstruida.
3. Tipos opacos (T05/T06/T07): `kMetadataTypeOpaqueResult` (NSMETA 1.30) → publicada.
4. `operatorToken`: no serializado (derivable del nombre `op_Addition`).

## Residual (fuera del cierre)

- La extensión estática **sin parámetros** (`extension Box { static int Twice() }`, 0 params) no
  publica el tipo extendido (el receptor no es un parámetro); el consumidor la rechaza con un error
  claro. Es un caso degenerado; el operador (≥1 param, primer operando = tipo extendido) es el caso
  productivo y queda cubierto.
