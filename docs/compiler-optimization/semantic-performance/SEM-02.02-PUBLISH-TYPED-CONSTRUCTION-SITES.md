# SEM-02.02 - Publicación de sitios tipados de construcción

## Problema cerrado

`ProjectMethodBodyModel` ya analiza cada argumento de `new T(...)` y placement construction, pero
esa decisión se perdía como frontera de fase. Construction volvía a recorrer cuerpos y consultaba
los bindings uno por uno después.

## Implementación

- Cada construcción publica un `ProjectConstructionSite` al terminar de analizar sus argumentos.
- El sitio contiene los bindings completos de argumentos, tipo objetivo e identidad del callable.
- Una clave `(syntax, callable)` impide duplicados si un nodo se reanaliza; la actualización
  conserva su secuencia original.
- La secuencia se asigna por callable en postorden: destino placement, argumentos y finalmente la
  construcción exterior. La publicación global no se usa como contrato de orden.
- La creación de `MethodExpressionTypeBinding` quedó centralizada para que el vector general y el
  sitio tipado no puedan divergir campo por campo.

## Mejora

El costo de capturar datos de Construction queda incluido en el único análisis semántico que ya
era obligatorio. El siguiente cambio podrá resolver constructores consumiendo este vector sin
recorrer otra vez todos los cuerpos.

## Validación dirigida

- `nsc_project_construction_model_tests` comprueba tres sitios, tipos `int`/`float`, aridad vacía y
  ausencia de pérdida en el modelo de Construction existente.
- Sólo O0; sin baseline, CTest general ni otras optimizaciones.
