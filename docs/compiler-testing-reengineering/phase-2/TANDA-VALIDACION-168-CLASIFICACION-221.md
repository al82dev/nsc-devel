# Tanda de validación 168 — Reconciliación vigente de los 221 fallos

Fecha: 2026-10-10  
Corrida autoritativa: `20261010-170051-1caf1c2f`  
Revisión ejecutada: `ca1ef7f04f6bd924405ea40f0fc37b4efb19c5fc`  
Configuración: `Debug/O0`

## SCOPE LOCK

Esta tanda persiste exclusivamente la clasificación causal de las `221`
variantes que permanecieron en `FAIL` después de la compuerta de Tanda 1. No
modifica producto, pruebas, manifiestos, Builder 3 ni evidencia histórica.

## Integridad del denominador

La reconciliación se verificó contra
`_nscbuilder3/history/20261010-170051-1caf1c2f/results.json`:

```text
FAIL de la corrida                         221
unión de las cuatro categorías            221
faltantes                                   0
sobrantes                                   0
intersecciones                              0
IDs duplicados                              0
```

| Clasificación | Casos |
|---|---:|
| Defecto de prueba/oráculo | 191 |
| Defecto real de producto | 22 |
| Pendiente de evidencia | 2 |
| Consumidor derivado | 6 |
| **Total** | **221** |

## Distribución por componente

| Componente | Prueba | Producto | Pendiente | Derivado | Total |
|---|---:|---:|---:|---:|---:|
| Host.Compilation | 11 | 0 | 0 | 0 | 11 |
| Host.Dominance | 3 | 9 | 0 | 0 | 12 |
| Host.Driver | 18 | 0 | 0 | 0 | 18 |
| Host.Frontend | 66 | 0 | 0 | 0 | 66 |
| Host.HIR | 4 | 1 | 0 | 0 | 5 |
| Host.Interop | 16 | 0 | 0 | 0 | 16 |
| Host.LanguageService | 14 | 0 | 0 | 0 | 14 |
| Host.MIR | 12 | 0 | 0 | 6 | 18 |
| Host.Parser | 4 | 4 | 0 | 0 | 8 |
| Host.Passes | 14 | 0 | 0 | 0 | 14 |
| Host.SSA | 4 | 0 | 1 | 0 | 5 |
| Host.Sema | 24 | 8 | 1 | 0 | 33 |
| SelfHost.Basic | 1 | 0 | 0 | 0 | 1 |
| **Total** | **191** | **22** | **2** | **6** | **221** |

## Defectos reales de producto — 22

### Dominance — 9

`TF-DOM-024`, `TF-DOM-032`, `TF-DOM-040`, `TF-DOM-104`, `TF-DOM-128`,
`TF-DOM-216`, `TF-DOM-224`, `TF-DOM-232`, `TF-DOM-256`.

Los nueve casos derivan de una sola causa: `DominanceVerifier::verifyFunction`
materializa estructuras cuadráticas y ejecuta comprobaciones cúbicas sobre
árboles lineales de 2,048/4,096 nodos.

### HIR — 1

`TF-HIR-120`: `HirBuilder::lowerExpression` no implementa
`ConditionalExpression` aunque Parser y Sema la aceptan.

### Parser — 4

- `TP-PAR-020`: `scanTypeAt` consume `borrowed`, pero no su lifetime.
- `TP-PAR-024`: lookahead genérico no maneja `GreaterGreaterEqual`.
- `TP-PAR-160`: duplicado promoted-field diagnostica declaración esperada.
- `TP-PAR-170`: duplicate variant-case diagnostica declaración esperada.

### Sema — 8

- `TS-SEMA-100`: el modelo semántico no publica el símbolo de
  `IdentifierExpression`.
- `TS-SEMA-116`: cascada `UnknownIdentifier` → `InvalidEnumInitializer`.
- `TS-SEMA-140`: una global `const` no constante recibe dos diagnósticos.
- `TS-SEMA-208`: canonicalización incorrecta de arrays fijos anidados.
- `TS-SEMA-216`: cast sobre `Error` agrega `InvalidConversion` derivado.
- `TS-SEMA-219`: la normalización de `->` agrega un segundo diagnóstico.
- `TS-SEMA-222`: indexación sobre `Error` agrega `InvalidIndex` derivado.
- `TS-SEMA-285`: widening constante signed carece de extensión de signo.

## Pendientes de evidencia — 2

| ID | Evidencia faltante |
|---|---|
| `TS-SEMA-111` | Descomponer el predicado compuesto y registrar el recibo exacto del ejecutable host. |
| `TF-SSA-273` | Persistir `SsaVerifier::error()` y compararlo con controles nominales equivalentes. |

No se autoriza modificar producto ni oráculo para estos dos casos hasta obtener
la evidencia puntual.

## Consumidores derivados — 6

`TF-MIR-152`, `TF-MIR-183`, `TF-MIR-195`, `TF-MIR-198`, `TF-MIR-200` y
`TF-MIR-270` permanecen `BLOQUEADO_POR TF-HIR-120`. No son seis defectos de
producto independientes y sólo se reevalúan después de corregir el productor.

## Defectos de prueba/oráculo — 191

La membresía exacta es el complemento de los `22 + 2 + 6` IDs anteriores
dentro de los `221 FAIL` autoritativos. Su reparto causal es:

- Compilation: 11.
- Dominance: 3.
- Driver: 18.
- Frontend: 66.
- HIR: 4.
- Interop: 16.
- LanguageService: 14.
- MIR: 12.
- Parser: 4.
- Passes: 14.
- SSA: 4.
- Sema: 24.
- SelfHost.Basic: 1.

La lista completa por ID permanece además en el plan correctivo de Validación
161 y se cruza con `results.json`; la conciliación automatizada confirmó cero
faltantes, sobrantes o intersecciones.

## Orden causal vigente

1. Corregir defectos de prueba/oráculo por familia y ejecutar sus casos
   puntuales.
2. Corregir cada productor real en commit separado; `TF-HIR-120` precede a sus
   seis consumidores MIR.
3. Instrumentar `TS-SEMA-111` y `TF-SSA-273` antes de clasificarlos.
4. Al cierre de cada tanda, reejecutar el conjunto correctivo restante.
5. Reservar la corrida integral de 5,695 variantes para el cierre de todas las
   tandas.

## Validación de esta actualización

No se compiló ni se ejecutó ninguna prueba. Este documento registra evidencia
ya obtenida y no promueve la clasificación a corrección terminada.
