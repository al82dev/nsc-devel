# Tanda universal 51: cobertura dinámica de fuentes en SH48

## SCOPE LOCK

- Objetivo exacto: impedir que SH48 falle porque cambió la cantidad legítima de fuentes de la
  biblioteca estándar, sin sustituir `82` por otro número rígido.
- Naturaleza: corrección de infraestructura de pruebas.
- Comportamiento protegido: SH48 continúa compilando dos veces la biblioteca, comprobando
  determinismo, símbolos prohibidos, hooks runtime, enlace NSLink y ejecución para los perfiles
  seleccionados.
- Revisión conocida como correcta: la corrida externa `20261007-192622-6fa7fec1` ejecutó la matriz
  funcional de SH48 y sólo rechazó el inventario `86/82`.
- Rango causal examinado: `85659b643..2c2c5910f` y el estado vigente después de `19ec3a57e`.
- Archivos autorizados: `selfhost/bootstrap/verify_runtime_library.py`,
  `selfhost/contracts/runtime-library.contract.json` y este documento.
- Archivos protegidos: compilador, bibliotecas, proyectos, fixtures, demás verificadores, NSLink,
  runtime y cualquier contenido en `C:`.
- Pruebas protegidas: todas las pruebas existentes permanecen sin modificaciones.
- Acciones prohibidas: actualizar el literal a 86, aceptar raíces vacías, ocultar duplicados,
  baseline, build, ejecución local de pruebas, deploy o cambios en `C:`.
- Criterio de terminación: SH48 descubre iterativamente los proyectos y fuentes desde el proyecto
  canónico, conserva un manifiesto y falla por problemas estructurales reales, no por un conteo
  histórico.

## Evidencia y causa raíz

El contrato conservaba `source_files: 82`. El verificador descubrió correctamente 86 archivos y
compiló la superficie vigente, pero añadió una falla antes de evaluar el resultado funcional. El
historial confirma que el mecanismo anterior ya había requerido cambiar manualmente el literal de
81 a 82 en `2c2c5910f`; repetirlo con 86 perpetuaría el defecto.

La cantidad no es una fuente de verdad independiente. El proyecto
`selfhost/tests/runtime-2.0/RuntimeLibraries.nsproj` ya declara los proyectos que forman la
biblioteca probada, y cada proyecto consume sus archivos `*.ns`. Mantener además raíces y un total
manual introduce tres inventarios que pueden divergir.

## Corrección

1. Leer los `ProjectReference` del proyecto canónico de SH48.
2. Validar que cada referencia exista, permanezca dentro del árbol fuente y no esté duplicada.
3. Recorrer iterativamente cada directorio referenciado y descubrir todos sus archivos `*.ns`.
4. Rechazar proyectos vacíos, errores de lectura y archivos cubiertos por más de un proyecto.
5. Conservar cantidad, rutas y hashes en el manifiesto y reporte como evidencia, sin tratarlos
   como una expectativa rígida.
6. Eliminar `library_roots`, `source_files` y `expected_executions` del contrato duplicado. El
   tamaño de la matriz se calcula desde los perfiles realmente seleccionados y los linkers
   declarados.

## Complejidad

El recorrido usa una pila explícita de directorios. Tiempo `O(D + F log F)` por ordenamiento del
manifiesto y memoria `O(D + F)`, con `D` directorios y `F` archivos. No utiliza recursión.

## Estado

- Diagnóstico: confirmado por el log externo y el historial del contrato.
- Implementación: aplicada.
- Validación local: no ejecutada por instrucción expresa.
- Validación externa: pendiente.
