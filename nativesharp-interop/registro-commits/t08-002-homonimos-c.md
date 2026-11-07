# T08-002: encabezados homónimos en directorios distintos

Sonda 5 de `nsc_cpp_c_identity_tests` y casos reales con `nsc`. Escribe
`dirA/same.h` y `dirB/same.h` con el mismo texto y `dirC/same.h` con los mismos
nombres y declaraciones distintas (typedef, función `static inline` y global
`static`).

Comprueba que Clang da el mismo USR en los tres directorios para las tres
categorías (`c:same.h@T@CSameInt`, `c:same.h@F@c_same_inline`,
`c:same.h@c_same_static`): el USR interno solo usa el nombre base del archivo.
En NSC, el typedef del mismo tipo de `dirA` y `dirB` enlaza y el de `dirC`
(otro tipo) se rechaza; las funciones `static inline` de `dirA` y `dirC`, con el
mismo USR, quedan como dos grupos distintos.

El fixture `pruebas-tanda-08-identidad-c/homonimos` compila con `nsc`:
`AliasAB` e `InlineA` compilan y ejecutan; `AliasAC` se rechaza con
`conflicting C/C++ type alias 'CSameInt'`; `InlineAB` e `InlineAC` se rechazan
con `ambiguous namespace function call to 'c_same_inline'`. La global `static`
no se importa en NSC (`unknown identifier`), así que solo se comprueba su USR.
