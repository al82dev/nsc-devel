# Caso 74: evaluación de inspección estática de campos

## Alcance y evidencia

El informe `A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md`
pide investigar si existe una superficie para enumerar campos arbitrarios; no
afirma que el ejemplo compile mal. El registro detallado identifica el mismo
alcance en `A:/native-sharp-reference/LIMITACIONES-SDK-Y-COMPILADOR.md`.
El `src/Program.ns` original del ejemplo 74 declara expresamente que escribe
los nombres de sus campos y no implementa un inspector universal. Utiliza
`sizeof`, `alignof`, `nameof` y los dos miembros conocidos de `CustomerRecord`.
No se modificó la reproducción.

El trabajo anterior del coordinador, documentado en su copia en
`A:/nsc-devel-fixes/docs/fixes/static-field-introspection.md`, registró que una
copia del ejemplo compila y ejecuta en O2 con salida 0. Los comandos y
resultados están en `A:/nsc-fixes-evidencia/74-example-current.cmd` y sus logs
de compilación y ejecución. Esa compilación incluyó Core completo y no fue
incremental; no se repitió. Es evidencia de ese ejemplo explícito, no de una
enumeración general ni de la revisión actual de esta rama.

## Superficie observada

`include/nativesharp/Frontend/ProjectMetaprogrammingModel.h` declara el
catálogo interno `findType` y `visibleMembers`; `StaticMemberInfo.h` registra
nombre, clave de tipo, clase de miembro y visibilidad. El documento histórico
`docs/3.0/items/NS3-T11-F01-typed-introspection.md` describe orden semántico,
acceso público desde otro proyecto y miembros privados desde el dueño, sin
registro de reflection en el binario. La prueba C++
`tests/unit/NativeSharp3T11TypedIntrospectionTests.cpp` comprueba una parte de
esos contratos internos. La ejecución anterior de esa prueba no identifica
inequívocamente la revisión completa de sus fuentes.

En la revisión de esta copia, la búsqueda de `visibleMembers` en `src` e
`include` encontró su definición y declaración, sin llamada desde una ruta
de frontend que la exponga a código NativeSharp. Las búsquedas de `fieldsOf`,
`GetFields`, `fieldinfo` y `staticreflection` en las fuentes y documentación
seleccionadas no identificaron una API de enumeración invocable por el ejemplo.
Una búsqueda textual no demuestra que toda forma equivalente sea imposible.

## Decisión y pendiente

No se atribuye un defecto vigente al ejemplo ni se considera implementada una
API pública. La recomendación es una consulta de compilación sin registro
runtime por defecto, apoyada en el catálogo existente, respetando identidad y
visibilidad. Antes de implementarla debe definirse cómo se expresan la consulta,
la iteración y el acceso tipado al valor, y comprobarse su comportamiento con
genéricos, bibliotecas y campos privados. Esa superficie aún no está definida;
no se cambió el lenguaje ni el SDK en esta evaluación.
