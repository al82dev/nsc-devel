# T08: contraste Clang del apuntado volatile adicional

El control de NSC mostró que el apuntado `volatile float` usa carga y escritura
volatile ordinarias, mientras long y unsigned long usan acquire/release. Se
prepara una sonda C para determinar qué emite Clang para los tres tipos. Aún
no se ha ejecutado; el hallazgo no atribuye defecto sin esa comparación.
