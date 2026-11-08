# Funciones inline C idénticas en encabezados homónimos

Los archivos `homonimos/dirA/same.h` y `dirB/same.h` contienen la misma
definición `static inline`. La sonda `c-index-test -test-load-source-usrs`
de Clang 23 informa `c:same.h@F@c_same_inline` para ambas. El proyecto
`homonimos/InlineAB.nsproj`, comprobado con `--check-project-method-bodies`,
produce `ambiguous namespace function call to 'c_same_inline'`: NSC conserva
la distinción entre dos definiciones internas aunque coincidan texto y USR.

El fixture existente ya comprueba ese diagnóstico. La evidencia se limita a
este caso y no implica que Clang produzca USR distintos ni que la llamada
ambigua se ejecute. No se ejecutó la batería completa.
