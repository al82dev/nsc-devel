# Diagnóstico de local C incompleto

El fixture intentó declarar un `Opaque` C por valor y obtuvo el rechazo
general de lifetime de objetos C/C++, aunque el tipo es incompleto. Se prepara
un diagnóstico `IncompleteType` específico antes de esa barrera para structs
y unions C incompletos; la ruta de objetos completos permanece intacta. El
fixture exige el mensaje nuevo. Falta la construcción y validación agrupada.

Con build incremental `6b5448c0a735018f`, el fixture de punteros opacos pasó
O0–O3 y comprobó el mensaje específico. Pasaron además la prueba existente de
tipos incompletos, el fixture de arreglos globales const y la regresión de
layout. Esto cubre un local C, no todos los usos de tipos incompletos.
