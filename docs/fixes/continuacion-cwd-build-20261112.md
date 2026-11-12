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
  no se ejecutaron. Falta una comprobación acotada que distinga fábrica,
  receptor, llamadas y tipos precompilados sin cambiar usos ordinarios.
- El caso 74 solicita enumeración de campos; la lectura del ejemplo y del
  pipeline muestra que la API y la expansión tipada no existen actualmente.
  El diseño preliminar está en
  `A:/nsc-fixes-auditoria-evidencia/caso-74-diseno-minimo.md`; no se implementó.
- `NativeSharp.Text` permanece aplazado por decisión explícita del usuario.
- Una tarea de agente sobre repetición de constructor binario fue interrumpida
  por el filtro automático de la plataforma; su repetición con la versión
  estricta de temporales quedó pendiente, sin atribuir el bloqueo a NativeSharp.

Continuar solo con lectura, documentación y trabajo aislado que no dependa de
las rutas de build pendientes. Tras recibir autorización, verificar la
configuración y volver a ejecutar únicamente las pruebas dirigidas necesarias;
conservar las fuentes originales y mantener fuera la interoperabilidad C/C++.
