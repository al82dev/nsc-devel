"""Tanda 8: contratos de vida, nulidad y atributos C (items 8.0-8.7).

8.3 `nonnull` (en la funcion, en un parametro listado) y `_Nonnull`: un `null` explicito se rechaza y un literal o un
puntero valido pasan; 8.5 parametro con `pass_object_size` llamado desde NativeSharp; 8.7 una funcion
`returns_twice` (setjmp) no se importa.
Pendientes documentados en implementacion-plan/CONTRATOS-PROPUESTA.md: 8.1/8.2 elemento <Contract>, 8.4 `format`,
`alloc_size` y `noreturn`, 8.6 `restrict` y espacios de direcciones.
"""
from comun import Caso, positivo, rechazo, fin

H = r'''
int len_nn(const char* s) __attribute__((nonnull));
int len2(const char* _Nonnull s, const char* opt);
int len3(const char* a, const char* b) __attribute__((nonnull(2)));
int sized(char* const buf __attribute__((pass_object_size(0))));
int my_setjmp(void* buf) __attribute__((returns_twice));
'''
S = r'''
int len_nn(const char* s) { return s[0] == 97 ? 10 : 0; }
int len2(const char* s, const char* opt) { return opt == 0 ? 20 : 0; }
int len3(const char* a, const char* b) { return a == 0 ? 10 : 0; }
int sized(char* const buf __attribute__((pass_object_size(0)))) { buf[0] = 2; return 0; }
int my_setjmp(void* buf) { return 0; }
'''
BODY = r'''
    sbyte[4] buf; buf[0] = 0;
    sized(&buf[0]);
    return len_nn("a") + len2("b", null) + len3(null, "c") + buf[0];
'''
c = Caso('t8-c', H, S)
positivo(c, BODY)
rechazo(c, 'return len_nn(null);', 'len_nn')
rechazo(c, 'return len3("a", null);', 'len3')
rechazo(c, 'return my_setjmp(null);', 'it returns twice (setjmp)')
fin()
