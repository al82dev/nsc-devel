# T08: reproducción de global C escalar `volatile _Bool`

El fixture aislado `A:/nsc-interop-build/t08-bool-scalar-repro` declara un
global C `extern volatile _Bool flag`, lo lee y escribe desde NativeSharp y
lee su valor desde C. Con NSC de desarrollo y Clang 23 Windows x64 MSVC, el
IR NSC generado contiene `@flag = external global i1`, `load volatile i1` y
`store volatile i1`; el IR Clang de la fuente C declara i8 y usa lectura
`atomic volatile i8 ... acquire, align 1`. Los IR se conservan como `nsc.ll`
y `clang.ll` junto al fixture.

La discrepancia afecta la representación y el orden del caso reproducido. No
demuestra por sí sola una falla observable de ejecución ni cubre bool nativo,
C++, `_Atomic` u otros modos. La corrección y la validación están pendientes.
