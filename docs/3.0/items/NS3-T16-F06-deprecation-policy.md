> **Vigencia:** NativeSharp tiene un único lenguaje acumulativo. Las etiquetas históricas de edición no seleccionan sintaxis ni semántica; la versión del producto es 1.0.

# NS3-T16-F06 — Política de deprecación / Deprecation policy

## Español

`nsc --print-deprecation-policy` publica `NSDEPRECATION 1`: siete aliases o comportamientos,
su código de diagnóstico y su estado explícito. Las advertencias admiten supresión localizada y
`nsc --analyze-migration-strict <archivo.ns>` las trata como errores para CI. El estado no depende
de una versión del lenguaje.

`nsc --simulate-deprecation-removal <feature> [--security-ub]` evalúa si una característica ya
deprecada puede retirarse o si aplica una excepción documentada de seguridad o comportamiento
indefinido. La simulación no recibe ni compara una versión del lenguaje.

## English

`nsc --print-deprecation-policy` publishes `NSDEPRECATION 1`: seven aliases or behaviors, their
diagnostic codes, and explicit states. Warnings can be locally suppressed; `nsc
--analyze-migration-strict <file.ns>` treats them as CI errors. State does not depend on a language
version.

`nsc --simulate-deprecation-removal <feature> [--security-ub]` checks prior deprecation or a
documented security/undefined-behavior exception. It does not accept or compare a language version.

## Validación dirigida / Targeted validation

La ejecución histórica de `nsc_ns3_t16_migration_tests` corresponde a la implementación anterior.
Las pruebas actualizadas quedan pendientes de ejecución por el usuario.
