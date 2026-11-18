# Pendientes por filtro de seguridad

Fecha de registro: 12 de noviembre de 2026.

El usuario pidió dejar para el final las dos revisiones siguientes, después de
que la plataforma detuviera a los agentes que las realizaban. **No son dos
defectos confirmados ni dos pruebas que hayan fallado**: son dos revisiones que
no pudieron completarse. Su alcance específico todavía debe determinarse.

## 1. Revisión del adaptador de memoria de ASan

- **Estado:** PENDIENTE — detenida por el filtro; aplazada por el usuario.
- **Trabajo solicitado:** revisar la implementación y la evidencia existente
  para identificar si quedaba una comprobación dirigida necesaria sobre las
  operaciones de reserva, reasignación y liberación.
- **Qué se conoce:** el agente `validar_heap` terminó con el mensaje de la
  plataforma sobre un posible riesgo de ciberseguridad. No entregó hallazgos ni
  un inventario de pruebas adicionales; no se puede afirmar que esas pruebas
  existan, que se ejecutaron o que pasaron.
- **Para retomar:** determinar primero el alcance preciso de la revisión y qué
  validación está autorizada y disponible. Conservar la evidencia anterior y
  documentar por separado cualquier resultado nuevo.

## 2. Revisión de las pruebas dirigidas del SDK

- **Estado:** PENDIENTE — detenida por el filtro; aplazada por el usuario.
- **Trabajo solicitado:** revisar la evidencia de consumo del SDK binario y
  determinar si faltaba alguna comprobación dirigida pertinente.
- **Qué se conoce:** el agente `validar_sdk` recibió el mismo mensaje y no
  entregó hallazgos ni un inventario de pruebas adicionales. No se puede afirmar
  que esa revisión se completó o que hay pruebas adicionales aprobadas.
- **Para retomar:** identificar una por una las comprobaciones necesarias y
  sus dependencias, y registrar sus resultados y procedencia sin sustituir la
  evidencia que ya existe.

## Mensaje y alcance del filtro

La plataforma indicó: “This content was flagged for possible cybersecurity
risk.” También sugirió solicitar acceso a Daybreak para trabajo de seguridad
autorizado. El mensaje no identificó una regla concreta ni confirmó una
vulnerabilidad. No se intentó eludir el filtro. La causa exacta del aviso no se
conoce; no se debe atribuir con certeza a un término o a una operación concreta.

El filtro detuvo esas revisiones posteriores. No invalida automáticamente las
pruebas que sí terminaron previamente, cuyos comandos, resultados y límites
constan en `asan-entrega-integrador.md` y los informes allí enlazados. En esas
pruebas se usó `detect_leaks=0`; Core y Collections no estaban instrumentados.
No se demostró ausencia general de fugas ni cobertura completa de bibliotecas.

## Pendiente separado: `SortWith`

El fallo de inferencia al consumir Collections binario sigue abierto y **no se
atribuye al filtro de seguridad**. La reproducción original, sus controles y la
hipótesis aún no confirmada están en `asan-sortwith-binario-handoff.md` y
`asan-sortwith-binario-regresion.md`. El usuario aceptó dejarlo para el final;
un eventual parche estático debe coordinarse con el responsable del frontend
compartido y no debe presentarse como verificado hasta ejecutar la regresión.

## Restricciones al retomar

Trabajar en `A:/nsc-limitaciones-asan`, con build y temporales independientes
en A; usar compilación incremental y pruebas dirigidas. No ejecutar la batería
completa, modificar interoperabilidad C/C++, Daikon, LLVM/Clang ni la
instalación `C:/native-sharp`. No hacer push, merge, rebase ni despliegue.
Los archivos compartidos conservan la propiedad indicada en `TAREAS-CHAT-ASAN.md`.
