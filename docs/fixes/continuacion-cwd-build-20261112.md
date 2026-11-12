# Continuación: build aislado y casos pendientes

Rama `development-fixes-limitaciones`, copia `A:/nsc-devel-fixes`. Los
resultados de esta tanda se ejecutaron sobre un compilador y SDK con trabajo
local aún sin consolidar; ninguno certifica HEAD limpio. El compilador actual
se restauró tras el contraste de capturas, pero su build utilizó una
configuración que contiene rutas absolutas adicionales cuya autorización está
pendiente. Ver `procedencia-herramientas-y-evidencia.md` antes de contar esas
pruebas como conformes. No se ha ejecutado la batería completa.

## Trabajo pendiente de aclaración de herramientas

- El build incremental llama automáticamente a `A:/apps/cmake/bin/cmake.exe`.
- La generación de runtime e import libraries configura
  `C:/native-sharp/bin/nslink.exe` y usa el auxiliar generado
  `A:/nsc-fixes-evidencia/build/bin/nsc_windows_runtime_imports.exe`.
- Se solicitó al usuario autorización específica para esas rutas; no se
  ejecutarán más builds que dependan de ellas mientras no se aclare. No se
  ha demostrado escritura en C. Los artefactos y temporales previstos están
  en A.
- Actualización: el usuario autorizó expresamente las tres rutas anteriores
  para continuar con el build aislado. Procede repetir las comprobaciones
  dirigidas con temporales en A desde el primer proceso y documentar los
  resultados posteriores sin borrar las incidencias previas.
- Varias invocaciones anteriores heredaron el TEMP predeterminado de C o
  iniciaron hijos desde temporales. Sus resultados se conservan como evidencia
  técnica no conforme; las repeticiones y límites están señalados en cada nota.

## Casos abiertos identificados

- La captura de tuplas con vistas tiene una protección registrada y un
  contraste antes/después con el compilador WIP; la procedencia del build de
  ese contraste está pendiente. Véase `borrowed-tuple-capture-boundary.md`.
- Las llamadas a `FnOnce` mediante campos y algunas alternativas mutables se
  aceptaron en análisis semántico provisional aun después de consumir el
  entorno. Los casos inseguros no se ejecutaron. Los controles de reasignación
  y las salidas están en `A:/nsc-fixes-agent1-temp/` con prefijos
  `fnonce-field-regression-provisional-20261112` y
  `fnonce-conditional-provisional-20261112`. La implementación aún no tiene
  solución demostrada que preserve los reemplazos válidos.
- La procedencia y transferencia de vistas dentro de tuplas de bibliotecas
  requieren un contrato verificable; la barrera conservadora sigue activa y
  también rechaza lecturas válidas. `binary-view-provenance-observation.md`
  distingue los prototipos y sus límites.
- La publicación de `this` desde constructores o destructores de cargas de
  punteros compartidos se aceptó en análisis semántico; los accesos inseguros
  no se ejecutaron. Una comprobación posterior de emisión O0 rechazó las
  publicaciones en constructor para Rc/Arc. Las reproducciones originales de
  destructor se detuvieron antes por un constructor implícito inaccesible; al
  agregar un constructor público vacío a copias separadas, las cuatro
  variantes de constructor/destructor Rc/Arc fueron rechazadas por la guarda
  del backend y dos controles sin publicación emitieron correctamente. No se
  ejecutó ningún programa. Evidencia técnica con build de procedencia
  pendiente: `A:/nsc-fixes-agente-2-evidencia/receiver-publication-emission-20261112/run-20261112T045936696066Z/results.json`
  y `A:/nsc-fixes-agente-2-evidencia/receiver-publication-explicit-20261112/run-20261112T050034276971Z/results.json`.
  No se infiere una garantía para todas las llamadas, bases o binarios.
  Un sondeo posterior con compilador reenlazado tras la autorización identificó
  una forma indirecta que sí llega a emisión: un helper de biblioteca estática
  guarda su parámetro en un campo estático y el constructor o destructor del
  payload le pasa `this`. Los cuatro cruces Rc/Arc y constructor/destructor
  emitieron en O0, al igual que dos controles sin publicación. Ningún programa
  se ejecutó. Evidencia:
  `A:/nsc-fixes-agente-2-evidencia/receiver-publication-external-helper-20261112/run-20261112T051126230771Z/results.json`.
  El build reutilizó objetos anteriores y Core binario preexistente; la
  corrección debe tratar efectos binarios no demostrados sin afectar llamadas
  que no reciben el receptor.
  `0b6fe8f22` incorpora la guarda conservadora para argumentos derivados del
  receptor en llamadas directas de cargas de clase shared: un destino sin
  cuerpo MIR verificable se rechaza. La compilación incremental y 24 casos
  de emisión O0 pasaron, junto con la regresión de publicación directa; no se
  ejecutaron los consumidores. Un helper binario benigno con receptor se
  rechaza también. Receptores implícitos de instancia, despacho virtual o de
  interfaz, retornos opacos, alias no reconocidos y estructuras no quedan
  cubiertos por esta guarda; ver `shared-receiver-binary-helper.md`.
- El caso 74 solicita enumeración de campos; la lectura del ejemplo y del
  pipeline muestra que la API y la expansión tipada no existen actualmente.
  El diseño preliminar está en
  `A:/nsc-fixes-auditoria-evidencia/caso-74-diseno-minimo.md`; no se implementó.
- `NativeSharp.Text` permanece aplazado por decisión explícita del usuario.
- Una tarea de agente sobre repetición de constructor binario fue interrumpida
  por el filtro automático de la plataforma; su repetición con la versión
  estricta de temporales quedó pendiente, sin atribuir el bloqueo a NativeSharp.

Tras la autorización ya recibida, se reenlazó incrementalmente el compilador y
se repitieron cinco pruebas dirigidas; la procedencia y los objetos reutilizados
se detallan en `procedencia-herramientas-y-evidencia.md`. Continuar con los
casos abiertos y únicamente las pruebas dirigidas necesarias; conservar los
originales y mantener fuera la interoperabilidad C/C++.
