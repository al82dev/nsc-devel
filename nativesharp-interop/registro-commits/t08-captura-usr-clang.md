# Captura del USR de Clang

T08-002: la sesión solicita a la API oficial el USR de la declaración canónica
al crear cada snapshot y conserva ausencia si Clang no entrega uno. Incluye
declaraciones C/C++ y sus miembros; la identidad es un dato de Clang y no una
garantía de ABI. La selección existente permanece sin cambios. La prueba y la
validación agrupada siguen pendientes.
