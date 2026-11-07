# Evidencia de puntero TLS C

Con NSC de desarrollo `857519a2727c32a2`, la construcción incremental de
`nsc` y de la suite de sesiones pasó. El control C y el proyecto NativeSharp
asignaron y desreferenciaron el mismo `int*` TLS y comprobaron aislamiento de
otro hilo, con biblioteca estática mediante LLD y DLL mediante NSLink en
O0–O3. La suite de sesiones terminó con código 0.

También pasaron la regresión del global TLS estático y el acceso de un global
TLS entero de DLL desde NativeSharp en dos hilos. Un arreglo TLS sigue
rechazado como control negativo. Se incorpora el fixture del puntero al gate;
la tanda completa y los demás contratos de punteros siguen pendientes.
