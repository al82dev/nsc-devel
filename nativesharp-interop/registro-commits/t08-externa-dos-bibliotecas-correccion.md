# Conciliación de global C externa entre bibliotecas

Dos productores publican plantillas que usan la misma global C externa. Antes,
el análisis veía dos candidatos y rechazaba la referencia como ambigua.

El modelo conserva en memoria el tipo canónico C y si el enlace es interno.
Solo se concilian candidatos C externos con el mismo USR, símbolo, tipo canónico,
tipo proyectado y propiedades de acceso, almacenamiento y enlace. Se mantienen
separadas las globales internas y los candidatos sin información suficiente.
No se modifica la representación persistida de la metadata.

En O0, el caso compartido compiló y ejecutó con lecturas y escrituras visibles
desde ambos productores. Un control `int` frente a `long` fue rechazado como
ambiguo. Pasaron también los controles de globales internas independientes,
ambigüedad de global interna y global externa de una plantilla.

La biblioteca C se enlazó una sola vez desde el consumidor; el primer fixture,
que la propagaba en dos copias, llegó a un símbolo duplicado en NSLink. No se
modificó NSLink ni se afirma que esa variante de enlace esté resuelta. La
compilación incremental terminó con advertencias en inicializadores fuera del
cambio. Falta verificar calificadores incompatibles y otros modos de enlace.
