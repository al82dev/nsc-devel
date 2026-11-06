# Callback almacenado con enum en metadata

Se preparó un productor que publica un callback y un holder C con enum en su
firma, y consumidores con el header y sin referencia al header. El segundo se
compila mientras la copia del header del productor está oculta; el script la
restaura incluso si el compilador devuelve error. La matriz O0–O3 está
incorporada al validador de la tanda, pero aún no se ha construido ni ejecutado.
La muestra no cubre todas las representaciones ni operaciones mixtas de enum.

## Validación del 2026-11-06

Con el compilador de desarrollo en A:, build `00e2f4fcbba05810`, el
productor generó su biblioteca y metadata en O0. El consumidor con referencia
explícita al header emitió `C++ instance field owner, layout or name is invalid`;
la ejecución se detuvo antes de probar la variante sin header y O1–O3. El
script devolvió código cero pese al diagnóstico; no se considera una prueba
aprobada y se debe investigar su propagación de errores. La condición exacta
del rechazo todavía no está identificada.

La prueba directa del callback almacenado con enum sí se ejecutó en O0–O3,
y `nsc.cpp-interop.session` pasó 1/1 con la misma construcción. Estos resultados
no cierran la compatibilidad con bibliotecas precompiladas ni la tanda 08.
