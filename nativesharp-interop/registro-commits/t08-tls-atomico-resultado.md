# Resultado inicial de TLS C atómico

El control C con Clang compiló y ejecutó; su IR contiene lectura, escritura y
actualización atómicas con orden `seq_cst`. NSC rechazó el global TLS atómico
antes de generar IR; el fixture positivo se incorpora al gate sin darlo por
aprobado. Registro: `A:/nsc-interop-build/tls-abi/atomic-reproduccion.log`.
Antes de considerar el soporte completo hay que comprobar el orden de cada
acceso en el IR específico y no solo la presencia global de instrucciones.
