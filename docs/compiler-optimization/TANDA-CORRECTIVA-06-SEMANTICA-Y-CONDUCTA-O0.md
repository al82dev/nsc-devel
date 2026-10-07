# Tanda correctiva 06: semantica y conducta O0

## Resultado

La auditoria dirigida no encontro una regresion viva en las rutas modificadas
por la reingenieria de `method bodies`, construccion, ownership, escape o waves
de callables.

Los ficheros `build-test-nslink/error*.log` no son una lista de tests fallidos.
Son artefactos que el propio driver crea para toda invocacion negativa, incluidas
las que una prueba ejecuta deliberadamente. Por ejemplo, en el mismo inventario
aparecen opciones desconocidas, `-O4`, archivos truncados y fuentes hostiles que
deben fallar. Corregir el compilador a partir de la mera existencia de esos logs
eliminaria diagnosticos validos.

## Cobertura dirigida

Se compilaron y ejecutaron exclusivamente los contratos de las rutas alteradas:

- `nsc.project-method-body-model`;
- `nsc.project-construction-model`;
- `nsc.project-callable-analysis-result`;
- `nsc.project-callable-wave-reducer`;
- `nsc.project-callable-wave-scheduler`;
- `nsc.project-callable-binding-index`;
- `nsc.project-callable-wave-driver`;
- `nsc.stage50_7.borrow-escape-analysis`.

Resultado: **8/8 passed**, 2.05 segundos.

Tambien se ejecuto el caso de conducta generada que habia fallado en una corrida
anterior:

- `nsc.ns3.t37-debug-values`: **passed**, retorno 42, O0.

## Conclusion

No se requiere un cambio de codigo para esta tanda. Las rutas calientes
conservan sus contratos y T37 conserva su conducta observable. Los cierres
self-host que dependen de artefactos producidos por etapas anteriores se auditan
por separado en la tanda 7.

No se modificaron pruebas, fixtures, proyectos, bibliotecas ni instalaciones.
No se ejecuto baseline, minibaseline ni matriz general.
