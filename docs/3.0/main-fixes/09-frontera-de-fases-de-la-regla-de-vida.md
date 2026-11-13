# Frontera de fases de la regla conservadora de vida (M2-08)

Rama `development-main-fixes`. Evidencia:

- `A:\nsc-main-fixes-evidencia\claude\logs\m2-08-t06.log` y `m2-08-s50.log`: el intento revertido;
- `logs\m2-08-revert-*`: el estado restaurado.

## Pregunta

La regla conservadora (documentos 04 a 08) corre al final de la etapa de construcciones. ¿Debe correr
también en `--check-project-method-bodies`?

## Experimento

Se llamó `diagnoseUnprovenArrayReferences` al final de la fase de cuerpos, después de
`diagnoseDeadArrayReferenceReceivers`. Se difirieron los usos marcados `requiresLateValidation` o
`potentialIndex`.

Resultado:

- `nsc.ns3.t06.escape-summaries`: 50 de 410 comprobaciones fallan;
- `nsc.stage50_7.borrow-escape-analysis`: 1 de 46 falla.

Las fallas son de dos clases:

- **Diagnósticos precisos reemplazados por E2.** La prueba tardía de caminos de la etapa de construcciones
  (`reportArrayReferenceLatePaths`) demuestra destrucciones en ramas, alias, ciclos y parámetros. Si la
  regla corre antes, E2 ocupa esos usos y la prueba tardía ya no puede dar su mensaje exacto. Ejemplos:
  - `construction rejects the feasible branch with a late diagnostic`;
  - `integration_branch`, `integration_loop_alias_dead`.
- **Contrato «method-bodies remains partial».** Las pruebas de integración fijan que la fase de cuerpos sola
  no reporta estos casos y que la etapa de construcciones sí. Cambiarlo rompería ese contrato documentado
  sin ganar seguridad, porque la compilación completa siempre corre ambas fases.

El cambio se revirtió (`git checkout`) y se recompiló el estado de `aaee7c357`.

## Contrato (decisión de esta campaña)

1. **Dónde corre la regla.** En la etapa de construcciones (`--check-project-constructions`) y, por lo
   tanto, en toda compilación completa (`--emit-project-*`), después de toda prueba precisa.
2. **Qué hace la fase de cuerpos sola.** `--check-project-method-bodies` es un análisis parcial: reporta los
   diagnósticos precisos inmediatos (`diagnoseDeadArrayReferenceReceivers`; por ejemplo, P01) y no la regla
   conservadora (por ejemplo, P16 pasa en esa fase y se rechaza en construcciones).
3. **Consecuencia para herramientas.** Una herramienta que necesite la regla completa (un IDE, por ejemplo)
   debe pedir la etapa de construcciones. Es la misma frontera que ya tenían la prueba tardía de caminos y
   los diagnósticos de construcción.

## Verificación

- P01 en `--check-project-method-bodies`: E1 «reference to a destroyed object cannot be used as an instance
  method receiver».
- P16 en `--check-project-method-bodies`: «ok».
- P16 en `--check-project-constructions`: E2 en `Main.ns:27:21`.
- La compilación completa de bibliotecas (`--emit-project-oop-lib`) ejecuta la regla: las versiones
  intermedias de M2-07 que eran demasiado amplias produjeron E2 en `NativeSharp.Core`.
