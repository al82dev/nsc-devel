# Caso 74: inspección de campos

El informe pide investigar la superficie prevista; el ejemplo no reclama ser
un enumerador universal. La copia del ejemplo compila y ejecuta en O2 con
salida 0 y campos escritos explícitamente. El compilador aislado informó build
hash `dbe260b6b7e51aed`; comando y salidas en
`A:/nsc-fixes-evidencia/74-example-current.cmd`, `-build.log` y `-run.log`.
El proyecto referencia Core desde fuentes y la opción `--no-artifact-cache`
provocó la compilación de sus 71 archivos junto con los dos del ejemplo. Esa
invocación no fue incremental; contradijo la instrucción posterior del usuario
y no debe repetirse de esa forma. La salida no demuestra enumeración general.

Existe un catálogo **interno** en `ProjectMetaprogrammingModel`: `findType` y
`visibleMembers` proporcionan información de miembros al código del compilador.
`StaticMemberInfo` contiene, entre otros, nombre, tipo y visibilidad. El diseño
existente indica orden semántico y filtrado por proyecto, sin registro runtime.
La búsqueda de `visibleMembers` en `src` encontró su definición, no una llamada
desde las rutas de frontend o CLI examinadas. No se ha identificado una API
NativeSharp para invocarlo o enumerar campos arbitrarios desde `#run`.

La prueba preexistente `nsc.ns3.t11.f01.typed-introspection` terminó con 7
comprobaciones aprobadas (`A:/nsc-fixes-evidencia/74-static-introspection-existing.cmd`
y `.log`). Su fuente comprueba orden, privacidad, tamaño y alineación sobre un
proyecto pequeño y ausencia de registro runtime. Se ejecutó el binario ya
construido sin recompilarlo: no se ha demostrado que corresponda a todas las
fuentes actuales. La prueba valida la API interna del compilador, no una API
accesible a un programa NativeSharp.

La recomendación es preservar la introspección estática sin metadatos runtime
por defecto y definir una superficie de lenguaje comprobable que exponga
identidad, nombres, tipos y visibilidad, con los límites de acceso del proyecto.
Esto es diseño pendiente, no una API implementada; requiere especificar cómo
se consumen la lista y los valores en tiempo de compilación, su comportamiento
con genéricos, bibliotecas y miembros privados, y las pruebas correspondientes.
