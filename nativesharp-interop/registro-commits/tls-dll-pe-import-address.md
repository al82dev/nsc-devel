# Dirección del dato TLS exportado a través de la importación PE

El experimento declara el dato como importación ordinaria exclusivamente para
comprobar la dirección inicial de la imagen: verifica que coincide con
`GetProcAddress` y que `GetModuleHandleExA` la atribuye a la DLL esperada.
Ningún acceso de hilo usa esa declaración directamente; la dirección se
convierte mediante el directorio TLS y los slots del hilo. La prueba original
de TLS directo de Clang permanece intacta. El experimento compiló, enlazó y
pasó la comprobación de direcciones y de lectura/escritura en dos hilos.
