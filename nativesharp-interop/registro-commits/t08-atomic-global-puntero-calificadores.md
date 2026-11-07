# T08: límite de calificadores en puntero atómico global no TLS

La matriz validada empleó `_Atomic(int*)` con apuntado no calificado. El
predicado inicial también admitía apuntados `const` o `volatile` sin haberlos
validado para globales no TLS. Se restringe esa variante no TLS a apuntado sin
calificadores; la ruta TLS conserva su condición original. La construcción y
regresión posteriores están pendientes. La ampliación a apuntados calificados
requiere controles propios.
