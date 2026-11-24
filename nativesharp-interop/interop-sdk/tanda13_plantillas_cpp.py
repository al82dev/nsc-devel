"""Tanda 13: plantillas C++ (items 13.1-13.13).

13.4 llamada constexpr evaluada para un local const y llamada de plantilla en ejecucion; argumentos de plantilla
explicitos; 13.6 NTTP entero y `unsigned long long` mayor que INT64_MAX.
Limitaciones documentadas: 13.1/13.2 (plantillas miembro y especializaciones explicitas de metodos), 13.3 (literal
en deduccion, fmt), 13.5 (concepts), 13.6 (NTTP flotantes y de puntero), 13.7 (plantillas de variable), 13.8
(constructores plantilla), 13.9 (struct NativeSharp con campo C++), 13.10/13.11 (plantillas precompiladas: metodos,
constructores, indexadores y ajustes a la base en el consumidor), 13.12 (ordenacion parcial), 13.13 (tipos C++ no
triviales en genericos NativeSharp).
"""
from comun import Caso, positivo, fin

H = r'''
template<class T> constexpr T sq(T x) { return x * x; }
template<class T> T twice(T x) { return x + x; }
template<int N> struct Fixed { static constexpr int value = N; };
template<unsigned long long N> constexpr unsigned long long big() { return N; }
'''
BODY = r'''
    const int k = sq<int>(6);
    if (k != 36 || twice<int>(21) != 42) return 1;
    if (Fixed<42>.value != 42) return 2;
    ulong v = big<18446744073709551615ul>();
    if (v != 18446744073709551615ul) return 3;
    return 42;
'''
# EN: C++ templates inside a precompiled NativeSharp generic body do not reach the consumer (item 13.11, T19-016).
# ES: Las plantillas C++ dentro del cuerpo de una plantilla precompilada no llegan al consumidor (item 13.11).
# NSMeta X4-06: item 13.11 no longer holds in lib; the consumer without the header imports these records through the
#     producer's source session and runs the whole body. / El item 13.11 ya no aplica en lib: el consumidor importa
#     estos records por la sesion de fuente del productor y ejecuta el cuerpo completo.
positivo(Caso('t13-cpp', H, None, language='C++'), BODY, modes=['directo', 'dll', 'lib'])
fin()
