# Tanda universal 46: igualdad de `string` devuelto por indexadores cerrados

## Alcance

Esta tanda corrige la clasificación de expresiones `IndexExpression` en el backend LLVM del
compilador self-hosted. No modifica proyectos, pruebas existentes, NSMETA, el runtime instalado ni
ningún archivo de `C:`.

## Causa confirmada

La reproducción mínima `Core + LLVM.C` fallaba al construir el grafo de fuentes. `LLVM.C` vuelve a
referenciar `Core`, pero `SourceGraph.ContainsProject` no reconocía que la ruta ya estaba presente.
La expresión `projects[index] == normalized` comparaba los punteros de dos strings con el mismo
contenido.

El backend ya distinguía strings cuando el hecho semántico contenía directamente `string` o cuando
una llamada conservaba su declaración. Un indexador de `List<string>`, sin embargo, conserva en la
firma borrada el retorno `T`; el tipo cerrado vive en el receptor `List<string>`. Por ello la
clasificación anterior veía `T`, emitía `icmp` entre punteros y omitía la comparación de contenido.

## Cambio

- `GenericResultPosition` relaciona los placeholders genéricos usados por el bootstrap con la
  posición del argumento cerrado correspondiente.
- `StringIndexExpression` reconoce arrays de string y retornos genéricos de indexadores; recupera
  el argumento cerrado desde el tipo del receptor.
- `StringExpression` consulta esa clasificación antes de bajar `==` o `!=`.

La corrección pertenece al backend y no al fixture: cualquier `List<string>` o colección cerrada
equivalente obtiene igualdad por valor sin exigir cambios al programa fuente.

## Complejidad

La clasificación realiza consultas directas a hechos, símbolos y tipos y analiza una sola lista de
argumentos genéricos. Su costo es lineal en la longitud del nombre de tipo, `O(n)`, con memoria
temporal `O(n)`. No introduce recursión.

## Evidencia previa al cambio

- `NativeSharp.Core` aislado: 43 unidades, 0 errores.
- `NativeSharp.Core.Collections.Generic` aislado: 25 unidades, 0 errores.
- `NativeSharp.Core.IO` aislado: 4 unidades, 0 errores.
- `NativeSharp.LLVM.C` aislado: 3 unidades propias, 0 errores.
- combinación `Core + LLVM.C`: `include matches no file: **/*.ns`.

La falla combinada demuestra que el glob y los hooks Win32 sí funcionaban; el defecto estaba en la
deduplicación por contenido de los nombres de proyecto.

## Validación prevista

Después del commit se ejecutará únicamente la reproducción combinada y, si pasa, la etapa dirigida
SH41. Ninguna de las 19 pruebas residuales se considerará cerrada hasta la batería integral O0 con
NSLink.

## Evidencia O0 posterior al commit

- La combinación `Core + LLVM.C` produjo `status=ok`, 3 proyectos y 46 unidades.
- `nsc2-o0-nslink.exe --check-project selfhost/nsc/NativeSharp.Compiler.nsproj` revisó 27
  unidades propias dentro de un grafo de 102 y terminó con 0 errores.
- `nsc3-o0-nslink.exe` repitió el mismo resultado: 102 unidades de grafo, 27 revisadas y 0
  errores.
- `nsc2-o0.obj` y `nsc3-o0.obj` produjeron el mismo SHA-256, lo que demuestra punto fijo O0 para
  esta generación.

El verificador SH41 intentó continuar automáticamente con O1 después de producir la evidencia O0.
Se canceló en ese límite para respetar la restricción vigente de ejecutar únicamente O0. Por esta
razón la evidencia es dirigida y parcial: no equivale al cierre de SH41 ni reduce por sí sola la
lista integral de 19 pruebas fallidas.
