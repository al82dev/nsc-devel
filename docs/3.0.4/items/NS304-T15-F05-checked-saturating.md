# NS304-T15-F05 — Checked/wrapping/saturating decimal

- **Estado**: IMPLEMENTADO (`checked(expr)`/`saturating(expr)`; wrapping/unchecked rechazados)
- **Rama**: development

## Resultado (2026-10-14)

- **Surface**: `checked(expr)` / `saturating(expr)` como forma de expresión (nodo `OverflowPolicyExpression`
  AST→HIR→MIR→LLVM). Sin bloques, sin estado global.
- **Predeterminado = checked**: el producto decimal `coeficiente > 2^96−1` atrapa `DecimalOverflow`
  (exit 145); `checked(...)` idéntico.
- **saturating**: el overflow del producto devuelve `MaxValue`/`MinValue` según el signo (bit 127 = xor
  de los signos de los operandos).
- **wrapping/unchecked**: rechazados con diagnóstico `overflow policy '...' is not allowed for decimal
  arithmetic` (el decimal no define aritmética modular).
- **Literal fuera de rango**: `1e30m` ya es error de compilación (`coefficient exceeds 64 bits`).
- **Menos unario decimal**: ahora voltea el bit de signo 127 (`XOR`), no el complemento a dos; y la
  comparación relacional decimal usa predicados con signo.
- Verificado: `TL-LANG-481` (saturating ±, exit 0), `TL-LANG-482` (checked, exit 145), `TL-LANG-483`
  (unchecked rechazado). Guards 429/423/424 PASS.

## Defecto corregido (2026-10-15)

- La extracción de la escala decimal en `ProjectOopLlvmLowerer.cpp` (`value >> 96`) no enmascaraba los
  bits 96..101: el bit de signo (127) y el padding (102..126) se filtraban en la escala. Un operando
  negativo leía `2^31 + escala` y disparaba el trap de `decimal.mul.scale` incluso bajo `saturating`
  (`TL-LANG-481` caía a exit 145 en el caso `neg`).
- **Corrección**: `(value >> 96) & 0x3F` en las tres extracciones de escala (mul + add/sub).
- Verificado: `TL-LANG-481/482/483` + barrido completo de `nsc_component_tests` (5748 casos) en verde.

## Residual

- Aplicar la política a add/sub/div (hoy el overflow del coeficiente sólo trapea/satura en el producto);
  el resto queda en T15-F04 (escala 0–28) y T15-F08 (formato).
