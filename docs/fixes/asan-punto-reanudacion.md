# Punto de reanudación: ASan y SDK

El usuario pidió detener el trabajo y retomarlo después. Este documento conserva
el estado para reanudarlo; **la tarea no está terminada**.

## Copia y estado al preparar la pausa

- Copia: `A:/nsc-limitaciones-asan`.
- Rama: `development-fixes-limitaciones-asan`.
- HEAD antes del commit de este documento: `dbc478ed8`.
- Base común de los trabajadores: `d00b3a8698d7750c80887d029d64fda481d67211`.
- El árbol estaba limpio antes de crear este documento. El HEAD definitivo se
  obtiene con `git log -1`; este documento no anticipa su propio hash.

## Pendientes

1. **`SortWith` con Collections binario:** el ejemplo original falla al inferir
   tipos; con Collections fuente pasa y un control separado con tipos explícitos
   también pasa. La causa exacta no está confirmada. El frontend de genéricos es
   compartido con otro frente. `asan-sortwith-binario-handoff.md` y
   `asan-sortwith-binario-regresion.md` conservan la evidencia y las regresiones.
   El usuario aceptó dejar este caso para el final y ofreció ejecutar la prueba
   si se prepara una corrección estática. No se debe declarar corregido hasta
   verificar el ejemplo original y sus controles.
2. **Dos revisiones aplazadas por el filtro:** la revisión adicional del
   adaptador y la revisión adicional de pruebas del SDK no se completaron; el
   usuario pidió dejarlas para el final. `asan-pendientes-filtro-seguridad.md`
   explica el aviso, el alcance conocido y las acciones pendientes. No son
   defectos confirmados ni pruebas registradas como fallidas.
3. **Registro de prueba:** `cmake-registro-realloc-propuesta.cmake` es una
   propuesta separada para el integrador. Pasó una comprobación aislada de
   argumentos, pero no se aplicó ni verificó mediante descubrimiento y
   ejecución real de CTest. No modificar el `CMakeLists.txt` del integrador.

## Evidencia ya obtenida

`asan-entrega-integrador.md` enlaza los resultados y commits del frente. Se
construyeron un compilador y Core en A y pasaron pruebas dirigidas del driver,
runtime, ejemplo original 97 y consumo binario en las configuraciones
documentadas. No se ejecutó la batería completa. Se usó
`ASAN_OPTIONS=detect_leaks=0`; Core y Collections no estaban instrumentados.
Los resultados no demuestran ausencia general de fugas ni cobertura completa.

## Al reanudar

Leer íntegramente `A:/LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md`,
`TAREAS-CHAT-ASAN.md` y este documento; verificar rama, cambios locales y estado
de los otros frentes. Coordinar con el integrador cualquier cambio compartido.
Usar solo compilaciones incrementales y pruebas dirigidas, con build y
temporales independientes en A. No tocar interoperabilidad C/C++, Daikon,
fuentes de LLVM/Clang ni `C:/native-sharp`; no hacer push, merge, rebase ni
despliegue. No reanudar los puntos aplazados hasta que corresponda a lo acordado
con el usuario, y no intentar eludir el filtro.
