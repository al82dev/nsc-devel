# Infraestructura de promoción para carriers importados de C

La promoción existente identifica carriers mediante un nodo de sintaxis NativeSharp;
un campo C importado no tiene ese nodo. Este cambio incorpora una marca explícita al
campo importado y al símbolo de miembro, y permite que la promoción existente la use.
El carrier marcado se oculta en el índice de nombres.

La marca todavía no se produce desde la proyección ni se restaura de metadata. Por
ello este cambio no habilita por sí solo miembros anónimos C11; T08-010 y T08-011
siguen abiertos. La validación agrupada de los cambios está pendiente.
