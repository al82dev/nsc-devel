# Tipo C++ enum class: avance parcial

La declaración local de `Space.Mode` para un `enum class Mode : int` no era
reconocida. Reproducción: `A:/temp/t10-enum-scoped-sonda/resultado.log`.
El tipo nombrado se incorpora con el namespace y USR que describe Clang; el
proveedor de valores sigue sin publicar enumeradores scoped en el namespace
que contiene al enum.

`pruebas-tanda-10-enum-cpp-scoped/validar.py` comparó declaración, conversión
explícita y copia con Clang 23.1.0, en O0–O3. También verificó el rechazo de
`Space.Value` y de la conversión implícita del enum a `int`. Registros:
`A:/temp/nsc-t10-enum-cpp-scoped-wktr4ebl`. Pasaron las regresiones C y de
enums globales (`A:/temp/nsc-t10-enum-cpp-global-ohyjv23s`) y de namespaces
(`A:/temp/nsc-t10-enum-cpp-namespace-9pzia39f`).

El acceso a `Space.Mode.Value` se comprobó después en
[enumeradores scoped](t10-enum-cpp-scoped-valores.md); campos, ABI y
precompilación siguen pendientes. El control negativo anterior de scoped se trasladó a
un enum scoped de base `long long`, todavía sin implementar. T10-002 y T09-003
permanecen abiertos.
