# Tanda 08: captura de firma de tipo función

Al describir un tipo función, Clang aporta resultado, convención, prototipo,
parámetros y variadicidad. La captura respeta el límite de profundidad ya usado
por tipos recursivos. Un puntero a función conserva así la firma de su elemento;
el uso desde NativeSharp y las llamadas indirectas siguen pendientes.
