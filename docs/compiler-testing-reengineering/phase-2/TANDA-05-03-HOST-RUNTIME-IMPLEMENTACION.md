# Tanda 05.03 — Materialización de Host.Runtime

Fecha: 2026-10-09

## SCOPE LOCK operativo

- **Objetivo autorizado:** materializar exclusivamente la suite nueva `Host.Runtime` definida por los inventarios,
  contratos, matrices y plan de pruebas cerrados de Fase 1.
- **Naturaleza:** infraestructura de pruebas y documentación de implementación; no cambia el producto.
- **Contrato preservado:** los `48` contratos `CF-RUNTIME-001..048` y los `384` casos
  `TF-RUNTIME-001..384`, con ocho casos por contrato y trazabilidad bidireccional caso-contrato-oráculo.
- **Referencia conocida:** inventarios de Fase 1 congelados en la revisión
  `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Archivos autorizados:** módulos nuevos `Runtime*` bajo `tests/compiler-suite/host/modules`, fragmento nuevo
  `tests/compiler-suite/manifest/fragments/host-runtime.jsonl`, gate nuevo
  `tests/compiler-suite/controller/integration/run_host_runtime_gate.py` y este Markdown.
- **Archivos protegidos durante la materialización:** producto bajo `include/` y `src/`; batería histórica;
  todo `C:` y todo repositorio remoto. La integración posterior conectó los archivos centrales sin cambiar producto.
- **Pruebas protegidas:** las `684` pruebas históricas y su infraestructura permanecen intactas.
- **Acciones prohibidas:** compilar, ejecutar pruebas, ejecutar gates, validar, desplegar, usar remotos,
  modificar producto, simular `PASS`, usar recursión o integrar archivos centrales compartidos.
- **Criterio de terminación:** cada ID diseñado existe en un módulo o catálogo explícito; sólo los casos con
  oráculo autocontenido aparecen en el fragmento ejecutable; toda infraestructura faltante queda visible como
  `InfrastructurePending`; todo defecto real demostrado conservaría `KnownProductDefect`.

## Inventario canónico

La fuente canónica de casos es
`docs/compiler-testing-reengineering/inventory/test-case-design.csv`, contrastada con
`inventory/contracts.csv` y `test-plans/HOST-RUNTIME-TEST-PLAN.md`.

| Dimensión | Conteo exacto |
|---|---:|
| Contratos `CF-RUNTIME-001..048` | 48 |
| Casos `TF-RUNTIME-001..384` | 384 |
| Casos por contrato | 8 |
| Capa `unit` | 144 |
| Capa `integration` | 240 |
| Perfil `PR` | 336 |
| Perfil `NIGHTLY` | 48 |

## Clasificación de materialización

| Clasificación | Contratos | Casos | Registro ordinario | Fragmento ejecutable |
|---|---:|---:|---|---|
| Autocontenidos | 10 | 80 | Sí | Sí |
| `InfrastructurePending` | 38 | 304 | No | No |
| `KnownProductDefect` | 0 | 0 | No aplica | No aplica |
| Total diseñado | 48 | 384 | — | — |

Los contratos autocontenidos son `CF-RUNTIME-001..010`. Cubren identidad ABI, manifiesto, huella, archivo de
contrato, lectura estructural de IR, particiones de imagen y reporte de capacidades. Sus oráculos usan valores
congelados, cálculo FNV-1a independiente, IR sintético controlado y archivos temporales aislados.

`CF-RUNTIME-011..048` conserva sus `304` IDs como `InfrastructurePending`. Estas superficies requieren una o
varias de las siguientes capacidades todavía no integradas en el arnés aislado: construcción LLVM completa,
ejecución de módulos runtime Win32, child processes, redirección, SEH, TLS, sincronización determinista,
inyección de fallos, matrices de handles/filesystem, ASAN u observación concurrente. Registrarlas en el flujo
ordinario falsearía el contrato o convertiría una carencia de infraestructura en defecto de producto.

Fase 1 no registra un defecto real y reproducible de `Host.Runtime` para estos `384` casos. Por ello no se asigna
`KnownProductDefect` por anticipación. El soporte de registro preserva esa disposición para una integración
posterior únicamente cuando exista evidencia causal de un defecto real.

## Módulos y trazabilidad

- `RuntimeTestSupport.h`: IDs canónicos, registro por contrato, disposiciones y catálogo pendiente.
- `RuntimeAbiIdentityModule.cpp`: `CF-RUNTIME-001..004`, `TF-RUNTIME-001..032`.
- `RuntimeAbiVerificationModule.cpp`: `CF-RUNTIME-005..007`, `TF-RUNTIME-033..056`.
- `RuntimeCapabilityModule.cpp`: `CF-RUNTIME-008..010`, `TF-RUNTIME-057..080`.
- `RuntimeInfrastructureModule.cpp`: `CF-RUNTIME-011..048`, `TF-RUNTIME-081..384` como
  `InfrastructurePending`.
- `manifest/cases.jsonl`: contiene los `80` casos ejecutables ordinarios; el fragmento temporal fue retirado.
- `run_host_runtime_gate.py`: exige exactamente esos `80` IDs y rechaza omisiones, extras o resultados distintos
  de `PASS` cuando la fase de integración autorice ejecutarlo.

## Estado de validación

```text
Inventario diseñado: 384/384 MATERIALIZADO
Casos ejecutables ordinarios: 80
Casos InfrastructurePending: 304
Casos KnownProductDefect: 0
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Integración central: REALIZADA; VALIDACIÓN DIFERIDA
```

Por instrucción expresa no se compiló, ejecutó ni validó ningún artefacto. Los cuatro registros y módulos, las dos
fuentes de producto estrictas, las 80 filas ejecutables, el gate y el estado persistente quedaron conectados. La
conclusión permitida es `IMPLEMENTACIÓN E INTEGRACIÓN MATERIALIZADAS; VALIDACIÓN PENDIENTE`. No se afirma que los
casos pasen.
