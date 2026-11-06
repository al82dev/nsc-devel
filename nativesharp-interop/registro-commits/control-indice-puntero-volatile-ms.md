# Control de índice de global C volatile int*

El fixture prepara una lectura y escritura mediante `volatile_pointee[0]`,
contrasta el valor desde C y exige acquire/release en el IR. Se incluye un
control de la marca de dirección MIR. La construcción y ejecución agrupadas
quedan pendientes.
