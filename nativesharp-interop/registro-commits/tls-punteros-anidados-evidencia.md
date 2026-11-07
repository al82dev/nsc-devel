# Evidencia de punteros TLS anidados y void

Con NSC `8fa4034ae7c994a3`, la construcción incremental y la suite de
sesiones pasaron. Los controles C y NativeSharp para `int**` y `void*` TLS
pasaron asignación, conversión y lectura con aislamiento entre hilos, en
biblioteca estática y DLL O0–O3. Las regresiones dirigidas de records, enums,
callbacks, calificadores y arreglos TLS pasaron; un record TLS `volatile`
sigue rechazado como control negativo.

La evidencia cubre las formas del fixture, no todos los niveles y tipos de
puntero. Las DLL mínimas y el gate integral de la tanda siguen pendientes.
No se modificó NSLink ni se desplegó en C:.
