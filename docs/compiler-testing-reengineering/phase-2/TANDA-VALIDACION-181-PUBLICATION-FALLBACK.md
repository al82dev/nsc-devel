# Tanda de validación 181 — `publicationFallbackContract`

## Alcance

- Componente: `Host.LanguageService`.
- Contrato: `CF-LANGUAGE-SERVICE-042`.
- Caso afectado: `TF-LANGUAGE-SERVICE-334`.
- Archivo: `tests/compiler-suite/host/modules/LanguageServiceAnalysis313To336Module.cpp`.

## Causa

La variante de cancelación exigía diagnósticos de compilación como evidencia de
fallo actual. Una cancelación determinista termina antes de producir esos
diagnósticos y publica su estado mediante `cancelled()` y el error asociado.

## Corrección

El oráculo distingue las dos rutas válidas de fallo actual:

- cancelación: estado cancelado y mensaje de cancelación;
- error ordinario: estado no cancelado y diagnósticos actuales no vacíos.

En ambas rutas se conservan las exigencias del contrato de fallback: análisis
actual fallido, índice semántico anterior disponible, marca de reutilización y
generación posterior. No se modificó el producto.

## Estado

Implementado. **PENDIENTE DE VALIDACIÓN FINAL** junto con el resto de la tanda;
no se compiló ni se ejecutó ninguna prueba durante este cambio.
