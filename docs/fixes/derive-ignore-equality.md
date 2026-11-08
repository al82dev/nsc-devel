# DeriveIgnore en igualdad y hash

La copia del ejemplo 75 compilaba, pero salía con código 1 y decía
`Equal despite different cache=False`. El modelo de derivación excluía el
campo marcado del listado de campos participantes; la generación ejecutable
de igualdad y hash recorría, en cambio, todos los campos físicos.

Para un tipo que declara conjuntamente `derive(Equality, Hash)`, la selección
de hojas de igualdad, operadores y hash omite los campos del tipo exterior
marcados con `DeriveIgnore`. Se mantiene la selección por campos físicos para
estructuras ordinarias. No se amplía el formato de `derive(Print)`.

La regresión nueva comprueba igualdad, desigualdad, hash coherente y una
estructura ordinaria en O0 y O2; pasó como `nsc.compiler.derive-ignore-equality`.
El ejemplo original copiado compiló y terminó con código 0; su salida sigue
siendo `Text=GridPoint`. También pasaron `nsc.ns3.t11.f03.derive` y
`nsc.stage56.c8-type-root`. Registros en `A:/nsc-fixes-evidencia`.

La evidencia se limita a derivación conjunta de igualdad y hash sobre el
registro exterior. No establece semántica para derivarlos por separado ni
para propagación de atributos en tipos anidados o módulos precompilados.
Se agregó `nsc_precompiled` al objetivo de la prueba existente de derivación
para resolver el codec que requiere el frontend.
