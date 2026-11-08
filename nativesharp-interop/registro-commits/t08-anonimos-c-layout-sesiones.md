# Layout de carriers C anónimos observados en dos sesiones

El modelo de `Repeat.nsproj` mostró el propietario sin carriers, aunque sus tipos
internos y campos sí estaban instalados. La proyección exigía `objectLayout`; para
definiciones repetidas verificadas se conserva `memberAccessLayout`. Se permite
ese layout únicamente para carriers anónimos C, tanto al proyectar el campo como
al calcular su layout físico. Las dimensiones siguen comparándose con Clang.

La corrección no se ha construido ni probado aún. El caso de dos sesiones y la
validación completa de la tanda 8 siguen pendientes.
