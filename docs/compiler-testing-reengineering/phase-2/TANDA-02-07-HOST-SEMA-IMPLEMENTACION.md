# Tanda 02.07 — Implementación de Host.Sema

Fecha: 2026-10-09

## Alcance

Se materializaron los `321` casos `TS-SEMA-001..321` de los contratos
`CS-SEMA-001..030`. El cambio pertenece exclusivamente a la suite nueva:

- no modifica código de producto;
- no toca la batería histórica ni sus seis archivos protegidos;
- no usa `C:`, despliegue, baseline ni remotos;
- no compila ni ejecuta pruebas o gates durante esta etapa.

## Organización implementada

Veinte módulos `Sema*Module.cpp` conservan un caso registrable por ID. Tres
soportes aislados separan los escenarios de análisis general, flujo semántico y
evaluación/conversión. Los escenarios que parten de fuente se detienen después
del análisis semántico: no dependen de HIR, MIR, CFG, dominancia, SSA ni passes.

La integración declarativa incorpora:

- las declaraciones y llamadas de los veinte registros;
- las fuentes de Sema requeridas por el target consolidado;
- `313` filas ejecutables en `cases.jsonl`;
- un controlador futuro `run_host_sema_gate.py`;
- cuatro defectos de producto preservados como `KNOWN_FAILURE`;
- ocho casos materializados fuera del manifiesto ordinario porque requieren
  sanitizer, seam privada o aislamiento de proceso.

## Corrección del plan

`TS-SEMA-089` describía seis clases globales soportadas, pero el contrato y el
producto vigente exponen cinco; `ClassDeclaration` se rechaza por separado en
`TS-SEMA-090`. Se corrigió el plan a cinco clases sin ampliar el compilador ni
inventar una capacidad.

## Estados visibles

Defectos de producto ejecutables:

- `TS-SEMA-106`: field de tipo función no reporta `InvalidType`;
- `TS-SEMA-110`: recursión nominal indirecta no queda rechazada por
  incompletitud;
- `TS-SEMA-246`: `validateObjectType` acepta `Function`;
- `TS-SEMA-317`: `SymbolDumper` omite el payload constante.

Infraestructura especial pendiente:

- `TS-SEMA-077` y `TS-SEMA-094`: perfil sanitizer;
- `TS-SEMA-101`: seam del helper privado de diagnóstico;
- `TS-SEMA-265`, `TS-SEMA-306`, `TS-SEMA-314` y `TS-SEMA-321`: proceso
  aislado para grafos o profundidad no acotada;
- `TS-SEMA-281`: seam aislada para el helper de enlace anónimo.

## Estado de validación

```text
Implementación: 321/321 MATERIALIZADA
Integración declarativa: MATERIALIZADA
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Motivo: validación offline diferida hasta completar toda la Fase 2
```

Los `321` casos aumentan la columna de implementación. Ninguno aumenta la
columna de verificación en esta tanda.
