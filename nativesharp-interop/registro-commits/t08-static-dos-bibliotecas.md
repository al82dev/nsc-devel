# Identidad de globales internas de dos bibliotecas

Dos bibliotecas independientes importan sendos encabezados `same.h` que definen
`static int internal_value` con valores 11 y 29. Cada una publica una plantilla
que lee su global. El consumidor llama ambas plantillas y espera esos valores.

La reproducción en O0 construyó ambas bibliotecas, pero el consumidor falló con
`ambiguous namespace state reference 'internal_value'` en ambas plantillas.
El caso sigue pendiente; la prueba no se añade al gate hasta corregirlo.

En `ProjectNamespaceFunctionModel::bindStateReference` se registran los candidatos
de `resolveStateCandidates`; `ProjectMethodBodyModel::analyzeNamespaceState`
emite el diagnóstico cuando queda más de uno. La causa de la pérdida de ámbito
todavía debe determinarse antes de modificar la resolución compartida.
