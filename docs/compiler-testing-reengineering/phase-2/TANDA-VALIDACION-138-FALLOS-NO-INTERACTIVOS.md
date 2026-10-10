# Validación 138 — fallos del host sin diálogos interactivos

## Estado

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.

## Problema

Durante el primer barrido Debug completo, algunos casos del ejecutable
consolidado abrieron cuadros de Microsoft Visual C++ Runtime Error. Un cuadro
modal detiene la automatización: el controlador no recibe el código de salida,
no publica la fila fallida y tampoco puede continuar con las pruebas restantes.

## Corrección localizada

El punto de entrada de `nsc_component_tests` instala, antes de procesar
argumentos, la política Windows no interactiva del proceso:

- desactiva cuadros del sistema para fallos críticos y faults;
- dirige los reportes CRT Debug a `stderr`;
- impide que `abort` abra el mecanismo interactivo de reporte.

La política no consume ni convierte el fallo. El proceso sigue terminando con
su código real y la salida queda disponible para el log y la fila exacta de la
GUI. En plataformas distintas de Windows el punto de configuración es un
no-op.

## Archivos y símbolos

- `tests/compiler-suite/host/src/main.cpp`
  - `configureNonInteractiveFailureReporting`
  - `main`
- `tests/compiler-suite/controller/tests/test_host_noninteractive_failure_policy.py`

No se modificaron casos, manifiestos, oráculos, timeouts, controladores,
compilador, CMake ni batería histórica.

## Evidencia ejecutada

- contrato estático dirigido: `3/3` pruebas aprobadas;
- `git diff --check`: aprobado;
- seis archivos protegidos: hashes sin cambios.

No se compiló ni ejecutó producto por instrucción del usuario. La prueba
funcional queda pendiente de su reconstrucción y de la siguiente corrida.

## Complejidad y seguridad

La configuración tiene costo constante, se ejecuta una vez al inicio y no usa
recursión. No cambia ownership, algoritmos de casos ni resultados terminales.
