# Evidencia de método — Tanda 03, Host.Source

## Alcance cerrado

Se leyeron completos, con numeración de línea, los cinco archivos asignados a `Host.Source` en el manifiesto
congelado. No se usaron muestras ni búsquedas como sustituto de la lectura.

## Procedimiento

1. verificar revisión y hashes ya congelados en `production-files.csv`;
2. leer headers y cuerpos completos;
3. enumerar API pública, helpers privados, estructuras y campos con efecto contractual;
4. seguir cada productor hasta sus consultas y sentinels;
5. reconstruir algoritmos de EOL, posición, lectura y lookup;
6. separar hechos vigentes, riesgos derivados y motivación histórica no verificada;
7. diseñar positivos, negativos, bordes, estados, ownership y fallos;
8. ejecutar una segunda pasada archivo por archivo contra símbolos, contratos y casos;
9. recalcular hashes documentales y cerrar sólo si no existen omitidos o desconocidos.

## Exclusiones comprobadas

No se modificó ni ejecutó código. No se compilaron pruebas, no se tocó la suite histórica, NSC Builder, C:,
deploy ni remotos. Los seis cambios preexistentes quedaron fuera de este trabajo.

## Resultado

El resultado cuantitativo y las discrepancias de la segunda pasada se registran en
`audits/TANDA-03-HOST-SOURCE-AUDIT.md`.
