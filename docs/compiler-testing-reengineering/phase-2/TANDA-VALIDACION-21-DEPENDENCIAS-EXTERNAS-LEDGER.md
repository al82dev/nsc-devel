# Tanda de Validación 21: dependencias externas en el ledger

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA DIRIGIDA

## Alcance causal

La carga y la selección ya distinguían las dependencias entre casos de los
productores externos, pero `ResultLedger.dependency_decision` todavía recorría
todos los valores de `dependencies`. Por ello, un caso que declaraba `CMake`,
`LLVM 23`, `NSLink` u otro requisito externo quedaba esperando resultados que
ningún caso podía producir y el runner terminaba sin progreso.

La corrección filtra únicamente las dependencias con identidad de caso al
calcular espera y bloqueo. El manifiesto permanece intacto: los productores
externos siguen disponibles como evidencia y los productores internos siguen
participando en el orden y bloqueo causal.

## Evidencia

- `11/11` pruebas dirigidas de `test_result.py` y `test_runner.py`: PASS.
- El runner ejecuta un caso que declara `CMake`, `LLVM 23` y `NSLink` y termina
  con `PASS`, sin espera ficticia.
- Los productores que sí son casos continúan ordenándose, bloqueando y
  propagando su identidad cuando fallan.
- El manifiesto real contiene `213` casos con productores externos y `11`
  nombres externos distintos; ninguno se convierte en resultado interno.
- `py_compile` de ledger, runner y sus pruebas: PASS.

## Exclusiones verificadas

No se modificaron casos, dependencias declaradas, oráculos, resultados
esperados, selector, producto ni batería histórica. No se configuró, compiló
ni ejecutó producto; no hubo baseline ni despliegue.
