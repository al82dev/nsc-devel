# Tanda 07.04 — SelfHost.Diagnostics — reconciliación con fixture NativeSharp real

## SCOPE LOCK operativo

- **Objetivo:** materializar los 4 contratos y 28 casos canónicos de `SelfHost.Diagnostics` mediante el
  producto self-host real, sin reimplementar su conducta en C++.
- **Naturaleza:** infraestructura de pruebas de Fase 2; producto sin cambios.
- **Revisión canónica:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Archivos autorizados:** fixture y módulos `.ns`, proyecto fixture, controlador, manifiesto y documentación.
- **Archivos protegidos:** producto, batería histórica, bibliotecas externas, coverage, `TRACKING.md`,
  `CURSOR.json`, `C:`, remotos y cualquier cambio concurrente ajeno.
- **Acciones prohibidas:** build, pruebas, gates, validación dinámica, modificación de producto, despliegue y
  uso de artefactos viejos como fallback.
- **Terminación:** 28 IDs con oráculo real, clasificación visible y ninguna afirmación de validación ejecutada.

## Denominador canónico

El componente inmediatamente posterior a `SelfHost.CodeGen` en
`inventory/implementation-waves.csv` es `SelfHost.Diagnostics`.

| Magnitud | Total |
|---|---:|
| Archivos de producto | 1 |
| Declaraciones físicas | 15 |
| Contratos | 4 |
| Casos | 28 |
| IDs | `TSH-00188..TSH-00215` |
| Capa `unit` | 28 |
| Perfil `PR` | 28 |

## Clasificación real

| Disposición | Casos | IDs |
|---|---:|---|
| `Required` | 26 | todos salvo `TSH-00213` y `TSH-00214` |
| `InfrastructurePending` | 0 | ninguno |
| `KnownProductDefect` | 2 | `TSH-00213`, `TSH-00214` |

La infraestructura de `SelfHost.Basic` ya demuestra la ruta autorizada: el controlador recibe `nsc.exe` y
`nslink.exe`, compila un `.nsproj` externo y ejecuta un caso por ID. Por tanto, clasificar los 28 casos como
`InfrastructurePending` y duplicar `Diagnostics.ns` en C++ es incorrecto.

### Defectos de producto preservados

- `TSH-00213`: `DiagnosticRenderer.JsonEscape` no escapa tabulador ni todos los caracteres de control. El
  fixture emite la salida real y el controlador intenta parsearla como JSON; el oráculo debe fallar.
- `TSH-00214`: `DiagnosticRenderer.Json` fija `severity` a `error`. El fixture construye un diagnóstico
  `Warning` y el controlador exige conservar esa identidad; el oráculo debe fallar.

Ambos casos continúan siendo ejecutables y usan el producto real. El controlador reporta
`KNOWN_PRODUCT_DEFECT`, nunca `PASS`, si el oráculo correcto sigue fallando; un éxito obliga a reclasificar el
caso y se reporta como `UNEXPECTED_PASS`. Ambos estados mantienen el gate rojo para no convertir el defecto en
`XFAIL` ni en éxito esperado.

## Reescrituras del plan genérico

Los contratos declaran `none declared` como canal de error. Las filas generadas como
`missing producer prerequisite` y `malformed relationship or value` no corresponden a una capacidad vigente.
Conforme al SCOPE LOCK, se reescriben contra conducta real y no se convierten en features pendientes:

- `TSH-00190`, `TSH-00197`, `TSH-00204` y `TSH-00211`: entradas vacías o rama pública sin catálogo.
- `TSH-00191`, `TSH-00198`, `TSH-00205` y `TSH-00212`: valores adversariales admitidos y escaping existente.

## Módulos y oráculos

- `SelfHostDiagnosticsRecordsModule.ns`: registro inmutable, copia/lifetime y catálogo; 14 casos.
- `SelfHostDiagnosticsRenderingModule.ns`: render humano, path normalizado y JSON real; 14 casos.
- `SelfHostDiagnosticsMain.ns`: despacho exacto de un ID; IDs desconocidos devuelven `251`.
- `SelfHostDiagnostics.nsproj`: fixture externo con `Diagnostics.ns` y sus productores mínimos.
  La clausura incluye `NativeSharp.Core.IO`, requerida directamente por `SourceBuffer.ns`.
- `run_selfhost_diagnostics_native_gate.py`: compila una vez, ejecuta 28 procesos aislados, parsea JSON y
  conserva estados `PASS`, `FAIL`, `KNOWN_PRODUCT_DEFECT` y `UNEXPECTED_PASS`.
- `manifest/fragments/selfhost-diagnostics.jsonl`: 28 filas ejecutables, incluidas las dos filas de defecto
  conocido con `expected_outcome=KNOWN_FAILURE` y oráculo no vacío.

No se utiliza recursión ni se añade coverage.

## Reconciliación central completada

Con autorización directa se reemplazó atómicamente la clasificación incorrecta:

1. se retiraron `SelfHostDiagnosticsTestSupport.h`, `SelfHostDiagnosticsRecordsModule.cpp` y
   `SelfHostDiagnosticsRenderingModule.cpp`;
2. se retiraron sus dos declaraciones, dos llamadas y dos fuentes del host consolidado;
3. se sustituyó el gate estructural por el controlador NativeSharp real;
4. se fusionaron 28 filas únicas en el manifiesto central y sólo después se retiró el fragmento temporal.

## Estado de validación

No se ejecutaron build, pruebas, gates ni validación. El estado exacto es:

`IMPLEMENTACIÓN E INTEGRACIÓN ESTÁTICAS COMPLETAS; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.
