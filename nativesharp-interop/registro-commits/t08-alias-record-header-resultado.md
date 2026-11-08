# Resultado del encabezado compartido con alias y enum

Con `c04d0d0299ecbb02`, `validar-referenciados.cmd` compiló el productor O0,
pero el consumidor con el mismo encabezado terminó con código 1 antes de
ejecutarse: `record observation name lookup failed: C/C++ type alias conflicts
with a native type 'NMood'`. El caso usa un alias a record y un enum en la API
del productor. No alcanzó O1–O3, el consumidor sin encabezado ni el control
negativo de alias incompatible en esta ejecución.

La prueba anterior sin encabezado y la regresión de records anónimos habían
pasado, pero no cubrían esta combinación. Falta resolver la conciliación del
enum, repetir el conjunto y comprobar que el caso negativo se rechaza por la
causa prevista. No se considera cerrada la corrección ni la tanda 8.
