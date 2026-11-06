# Proyección de campos de bits en unions C

Se permite proyectar campos enteros no volátiles de unions C con las coordenadas y límites del record observados por Clang. Los demás miembros de una union comparten almacenamiento por diseño, por lo que no se aplica la prohibición de solapamiento utilizada para secuencias independientes de campos en structs. Se mantienen la exclusión de C++, el rechazo de volatile y las comprobaciones de tamaño y alineación. La ejecución se valida junto con los casos existentes.
