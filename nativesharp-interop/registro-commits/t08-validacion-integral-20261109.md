# T08: validación integral del 2026-11-09

Fecha: 2026-11-09. Casillas: T08-GATE-01 a T08-GATE-05 y T08-004-05.

## Compilador validado

- Rama `development-cpp-clang`, después de T08-020 (`ed5e643fd`) y con la corrección de T08-GATE-02
  (`ProjectCallableWaveScheduler.cpp`).
- Construcción incremental ON y OFF en `A:/nsc-interop-build`. Nada en `C:`, ni NSLink ni LLVM/Clang.

## 1. Gate histórico: `validar-tanda-08.cmd`

Salida completa: `A:/nsc-interop-build/tanda08-integral-20261109.log`, terminó con código 1.

**Aprobado:** pruebas de sesión Clang (código 0 en repetición aislada), restauración de sintaxis (6 + 4),
restauración de MIR (13), identidad de código MIR (15) y todos los casos agrupados, salvo cuatro.

| Caso | Causa | Clasificación |
|---|---|---|
| `pruebas-tanda-08-identidad-c\validar.cmd` | «The system cannot find the batch label specified - setup_atomic»: el archivo versionado tiene finales LF y cmd no encuentra la etiqueta | Infraestructura (decisión 8). Pasa desde copia CRLF sin otro cambio |
| `pruebas-tanda-08-record-c\precompiled-bool\validar-cache.cmd` | Espera que el productor con `--artifact-cache` falle con «C/C++ project references require --no-artifact-cache until transitive headers are tracked», restricción retirada por `03bf12e42` | Prueba histórica obsoleta. Con el contrato vigente pasa |
| `pruebas-tanda-08-tls-c\atomic\validar.cmd` | La misma expectativa retirada; además, el resto de la suite nunca llegaba a ejecutarse | Prueba histórica obsoleta. Con el contrato vigente pasa completa, con acierto de caché |
| `pruebas-tanda-08-tls-c\directo\validar-relleno-cero.cmd` | El control **C** (Clang + lld, sin NSC) con la DLL parcheada (`SizeOfZeroFill` 0 → 5) devuelve 1 al ejecutarse justo tras el parche. Los mismos binarios devuelven 0 en tres repeticiones posteriores (`relleno-repetir.cmd`) | Entorno: carrera del control C, anterior a cualquier paso de NSC. No es un fallo del compilador |

## 2. Ejecuciones derivadas: `validar-tanda-08-derivados.py`

Ejecuta los tres primeros validadores desde una copia CRLF de los árboles de prueba. En la copia solo cambia lo
documentado, y los archivos históricos no se tocan:

- **identidad:** sin cambios; **correcto**.
- **caché bool:** el productor con caché debe compilar; **correcto** O0–O3, con acierto de caché del consumidor.
- **TLS atómico:** la compilación con caché debe ejecutar y acertar en la segunda; **correcto**, con toda la suite
  posterior (O0–O3, rechazos e IR).

## 3. Gate complementario: `validar-tanda-08-complementaria.cmd`

Salida: `A:/nsc-interop-build/tanda08-complementaria-20261109.log`. Ejecuta las suites que el gate histórico no
cubre:

- las 59 suites Python de T08-002 a T08-020;
- las ejecuciones derivadas;
- nueve validadores `.cmd` de bloque: plantillas, `layout-clang`, `flexible-c\validar-decay.cmd` y sombras.

Resultado: ver la sección «Resultado final».

## 4. Otras pruebas unitarias

- `nsc_project_method_body_model_tests`: 120/120 (T08-GATE-02).
- `nsc_project_callable_wave_scheduler_tests`: 12/12.
- `nsc_project_oop_lowering_tests` completo: 26 de 660 fallos previos y ajenos. Son idénticos sin las correcciones
  de esta sesión (`t08-gate-ondas-paralelas.md`). El gate solo usa su foco `mir-cache-restore-depth` (13/13).

## Estado de los gates

- **T08-GATE-01:** las casillas de T08-002 a T08-020 están terminadas o bloqueadas con evidencia. Las bloqueadas
  dependen de las decisiones pendientes del tracking (sobre todo T08-010-05a) o son características nuevas
  registradas (T08-016-03d1, T08-017-03d1).
- **T08-GATE-02:** resuelto (`t08-gate-ondas-paralelas.md`).
- **T08-GATE-03:**
  - el compilador pasa todo lo verificable;
  - el gate histórico sigue en código 1 por tres validadores que necesitan cambiarse (LF y la restricción de
    caché retirada) y por la carrera del control C de relleno TLS;
  - modificar pruebas existentes requiere autorización.
- **T08-GATE-04:** el tracking refleja esta revisión. Ninguna casilla se cerró solo por una muestra parcial.
- **T08-GATE-05:**
  - respaldo del historial creado sin modificarlo: `A:\nsc-interop-build\claude\respaldo\` (bundle de git de la
    rama);
  - la reagrupación de commits (reescritura) no se hizo: el criterio acordado no está en el repositorio.
- **T08-004-05:** los controles TLS y DLL se ejecutaron en el gate integral:
  - pasan todos menos `relleno-cero` (entorno) y `atomic` (prueba obsoleta);
  - `atomic` pasa en su ejecución derivada.

## Resultado final

- **Gate complementario:** código 0, «Gate complementario de la tanda 08 terminado sin fallos». Pasaron las 59
  suites Python, las tres ejecuciones derivadas y los nueve validadores de bloque.
- **Gate histórico:** código 1 por los cuatro casos clasificados arriba. Ninguno es un fallo del compilador.
  Cerrarlo en verde exige una de dos cosas, y las dos son decisiones del usuario:
  - autorizar la actualización de esos validadores históricos (CRLF y la expectativa de caché vigente);
  - aceptar las ejecuciones derivadas como sustitutas.
