# Invalidación de caché por DLL transitiva

La caché del consumidor consideraba la DLL C declarada directamente, pero no
las DLL hermanas que su tabla de importación incorpora al paquete. Se reprodujo
que cambiar `leaf.dll` de resultado 7 a 9 restauraba el paquete antiguo.

Antes de consultar el bundle, NSC recorre el cierre de DLL hermanas importadas,
agrega las presentes como entradas y registra tanto su presencia como su
ausencia. Si no puede inspeccionar el cierre, no utiliza la caché de bundle.
El empaquetado y los binarios originales no se modifican.

`cache-runtime-transitivo/validar.py` pasó en O0 y O3: compilación inicial,
acierto, modificación de la DLL hija, invalidación, ejecución con el valor
nuevo y nuevo acierto. La prueba de caché C directa y la de rutas Unicode
también pasaron en O0. La construcción fue incremental.

Esto no resuelve la ambigüedad de importaciones transitivas entre DLL homónimas;
ese punto y la validación completa de la tanda siguen pendientes.
