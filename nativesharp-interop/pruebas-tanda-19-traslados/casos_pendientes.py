"""T19-036: casos de traslados que todavia no funcionan; no es un validador de la bateria.

Cada entrada tiene la forma de `CASES` en validar.py y pasa a ese archivo cuando NativeSharp la consume como
Clang (registro t19-traslados-verificacion.md). Por ahora no queda ninguno: los siguientes traslados de T19-036
(`class` en el catalogo precompilado, globales en plantilla precompilada, excepciones a traves de un adaptador y
de una especializacion) entran aqui cuando se verifiquen.
"""
PENDING = {}
