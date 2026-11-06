# Diagnóstico de local C incompleto

El fixture intentó declarar un `Opaque` C por valor y obtuvo el rechazo
general de lifetime de objetos C/C++, aunque el tipo es incompleto. Se prepara
un diagnóstico `IncompleteType` específico antes de esa barrera para structs
y unions C incompletos; la ruta de objetos completos permanece intacta. El
fixture exige el mensaje nuevo. Falta la construcción y validación agrupada.
