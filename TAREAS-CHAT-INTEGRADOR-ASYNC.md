# Tareas persistentes: async e integración

Esta copia es `A:/nsc-devel-fixes`, rama `development-fixes-limitaciones`.
El checkpoint común para los demás chats es
`d00b3a8698d7750c80887d029d64fda481d67211`; contiene trabajo WIP, no una
certificación de que la campaña esté terminada. Lee íntegramente
`A:/LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md`, las instrucciones del repositorio,
`A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md` y su registro.

## Resultado que debes conseguir

Completa los casos de async y cleanup pendientes y coordina la integración de
los otros cuatro frentes, siempre confirmando cada defecto y conservando los
ejemplos originales. No cierres el objetivo general hasta acreditar todos los
requisitos autorizados. En particular:

1. La cancelación debe realizar la limpieza en el siguiente avance, conforme a
   la decisión del usuario, sin publicar elementos adicionales. Una traza de
   finally anidados mostró `Current=6` y `true` después de `Cancel()`; verifica
   la reproducción actual y corrige sin omitir efectos ni destructores.
2. Preserva correctamente `await`, reanudación, regiones anidadas, excepciones
   y ownership: un valor movido requiere limpieza distinta de una referencia
   prestada. El prototipo limitado a `int` no demuestra el contrato general.
3. Investiga y corrige las rutas reproducidas de reemplazo de excepción y
   finally que generan errores de dominancia o acciones excepcionales inválidas
   en funclets SEH. Mantén el tratamiento de excepciones ajenas; no modifiques
   LLVM ni suprimas el fallback como atajo.
4. Completa pruebas dirigidas de cleanup normal/excepcional, suspensión,
   cancelación y abandono, con clases, structs, owned y préstamos cuando
   corresponda. Diferencia fallos previos, correcciones y límites reales.
5. Revisa e integra selectivamente los commits de las ramas de vistas,
   numéricos, ASan y campos. Esta rama es la única de integración; conserva la
   trazabilidad de origen, pruebas y conflictos con interoperabilidad.

Coordina hasta tres subagentes además del responsable: uno para cancelación y
reanudación, otro para exceptions/finally y otro para ownership y revisión de
regresiones. El responsable combina trabajo y revisa resultados. Este chat es
responsable de `OopLlvmCoroutines.cpp`, `OopLlvmCleanup.cpp`,
`OopLlvmStructuredExits.cpp` y las rutas HIR/LLVM de cleanup async; coordina las
interfaces que otro frente necesite. Centraliza `CMakeLists.txt`,
`src/LLVM/Internal/OopLlvmImplementation.h` y documentos globales de campaña.

## Límites y entrega

- Usa siempre compilaciones incrementales y pruebas dirigidas; no ejecutes la
  batería completa. Cada copia debe tener build, temporales, cachés y resultados
  propios en A. Comprueba cwd y variables de procesos hijos.
- No modifiques `A:/nsc-devel`, `C:/native-sharp`, fuentes de LLVM/Clang,
  builds de interoperabilidad, Daikon ni interoperabilidad C/C++. No hagas
  push, merge, rebase sobre la rama de interoperabilidad ni despliegue.
- Usa herramientas por nombre salvo las rutas expresamente autorizadas para la
  configuración correspondiente. Las autorizaciones de un build no se extienden
  automáticamente a otros builds.
- Toma autónomamente las decisiones de diseño delegadas por el usuario y
  registra evidencia, consecuencias y pruebas. `NativeSharp.Text` permanece
  aplazado; no inventes versiones de producto.
- Conserva commits coherentes, bases y límites de evidencia. Algunos runners
  WIP utilizan cwd temporal; ajústalos antes de usarlos como prueba conforme.

La tarea permanece activa hasta completar el alcance y su validación dirigida;
un prototipo, un checkpoint WIP o una prueba parcial no son cierre.
