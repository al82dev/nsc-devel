# T08: fallos inducidos en validadores

**Precisión posterior:** el código cero descrito abajo se observó al lanzar
directamente el script desde la herramienta. Los scripts anteriores, llamados
con CALL como en el gate principal, sí propagaron los fallos en las muestras
comprobadas. Véase t08-validadores-precision-call.md.

Se reprodujeron dos falsos éxitos: un productor inválido en global-static y
una opción inválida durante emisión dentro del bucle de widths. Ambos scripts
mostraban el error pero devolvían cero. Después de la corrección, las copias
aisladas devolvieron uno y mostraron el diagnóstico.

Las rutas normales de global-static y contratos pasaron O0–O3. Widths global y
TLS pasaron O0–O3. Los logs del fallo global-static están en
`A:\nsc-interop-build\tanda08-global-static-6488-20912` y el fallo widths en
`A:\nsc-interop-build\tanda08-atomic-widths-global-6827-24634`.

Se identificaron otros scripts con construcciones similares. No se afirma que
todos fallen del mismo modo; es necesario comprobar y corregir las rutas
afectadas antes de tratar un resultado integral basado solo en el código de
salida como evidencia fiable. Los marcadores y logs previos requieren revisión.
