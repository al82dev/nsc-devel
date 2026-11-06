# Callback almacenado con enum en metadata

Se preparó un productor que publica un callback y un holder C con enum en su
firma, y consumidores con el header y sin referencia al header. El segundo se
compila mientras la copia del header del productor está oculta; el script la
restaura incluso si el compilador devuelve error. La matriz O0–O3 está
incorporada al validador de la tanda, pero aún no se ha construido ni ejecutado.
La muestra no cubre todas las representaciones ni operaciones mixtas de enum.
