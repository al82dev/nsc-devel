"""Tanda 16: efectos y vida en C++ (items 16.1-16.8).

16.8 macros sensibles al sitio (`__FILE__`, `__LINE__`): la macro objeto no se importa con la ubicacion de la unidad
sintetica y la de funcion se rechaza con nota; una macro constante ordinaria sigue funcionando.
Pendientes por la sintaxis de contratos (D6, implementacion-plan/CONTRATOS-PROPUESTA.md): 16.1 transferencia, 16.2
prestamo retenido, 16.3 prestamo dentro de un valor, 16.4 invalidacion, 16.5 callbacks persistentes, 16.6 `const&`
mutado por la API. 16.7 (temporales de wrappers de macro, C_STR) queda documentado.
"""
from comun import Caso, positivo, rechazo, fin

H = 'int log_at(const char* f, int line);\n#define HERE_LINE __LINE__\n#define LOG() log_at(__FILE__, __LINE__)\n#define TWO 2\n'
S = 'int log_at(const char* f, int line) { return line; }\n'
c = Caso('t16-c', H, S)
positivo(c, 'return TWO + 40;', modes=['directo', 'dll'])
rechazo(c, 'return HERE_LINE;', "unknown identifier 'HERE_LINE'")
rechazo(c, 'return LOG();', 'the macro uses __FILE__')
fin()
