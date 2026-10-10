# Tanda de validación 156 — cierre de residuos de la corrida integral

## Evidencia de entrada

- Corrida: `20261010-122235-304f2829`.
- Selección: `Total`, perfil `Todos`, build `Debug`, NativeSharp `O0`.
- Cardinalidad: `5,695/5,695`, sin cancelación.
- Resultado: `5,187 PASS`, `262 FAIL`, `41 KNOWN_FAILURE`, `18 INFRASTRUCTURE_ERROR`, `187 BLOCKED`.

La corrida confirmó que Packaging y los dos abortos enum dejaron de ser errores de infraestructura. También descubrió tres residuos concretos que no estaban cerrados funcionalmente.

## Residuo 1 — la ruta activa de Builder 3 omitía la materialización self-host

Los cinco gates independientes ya materializaban proyectos contenidos, pero `ExecutionEngine._prepare_selfhost_fixture()` mantenía una segunda ruta que compilaba el `.nsproj` canónico. Los cinco logs conservaron el mismo rechazo de segmentos `..`; por tanto, los `187 BLOCKED` seguían derivados de una omisión del adaptador.

La ruta activa ahora carga el mismo materializador inventariado, genera el proyecto bajo la evidencia de la corrida y entrega ese proyecto —no el canónico— al NSC seleccionado. Una prueba de contrato inspecciona el comando y todos sus `Compile Include`.

## Residuo 2 — dos entradas Host.Basic existentes no se descubrían

`TB-BAS-021` requiere el `BuildId.h` generado por la construcción seleccionada y `TB-BAS-022` un NSC de comparación distinto. Ambos artefactos existen:

- `A:\nsc-devel\build-clang\generated\nativesharp\Basic\BuildId.h`;
- `A:\native-sharp-3.0-baseline\bin\nsc.exe`, distinto del NSC seleccionado en `A:\nsc-devel\build-clang\bin\nsc.exe`.

Builder 3 ahora los descubre sin sustituir una selección explícita válida y nunca acepta el mismo ejecutable como comparación.

## Residuo 3 — once límites de tiempo eran contratos, no fallos de infraestructura

Los once casos NIGHTLY Dominance terminaron exactamente alrededor de 180 segundos. Sus oráculos ya declaran “terminación acotada o limitación vigente reproducida”. El tiempo agotado es, por tanto, el valor observado del contrato de performance: debe ser `FAIL`, no `INFRASTRUCTURE_ERROR`.

Cada uno queda marcado explícitamente con `timeout_is_contract_failure: true`. El esquema sólo permite ese marcador en un caso host `boundary`, `NIGHTLY` y `extended`. Un timeout de cualquier otro caso conserva la clasificación de infraestructura. No se redujo el tamaño, no se aumentó otra vez el límite y no se convirtió ningún fallo en éxito.

## Límites

- Código de producto NSC modificado: no.
- Pruebas retiradas: no.
- Expectativas funcionales relajadas: no.
- Unidad `C:` modificada: no.
- Seis archivos protegidos modificados por esta tanda: no.
- Revalidación dirigida: pendiente después del commit.
