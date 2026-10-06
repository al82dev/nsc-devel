# SEM-02.04 - Aseguramiento de la frontera Construction

## Contadores deterministas

`ProjectConstructionStatistics` publica solamente cantidades estructurales:

- sitios tipados consumidos;
- argumentos tipados consumidos;
- nodos declarativos revisados por el validador externo;
- nodos de cuerpos recorridos nuevamente.

El último contador debe permanecer en cero. No es una medición temporal y no participa en
diagnósticos, dumps, metadata, hashes ni salida ordinaria.

La prueba dirigida comprueba que tres construcciones y dos argumentos se consumen desde sitios
tipados con `bodyAstNodesRescanned == 0`. Si alguien reintroduce el recorrido, deberá actualizar
deliberadamente el contador y romperá la prueba.

## Diagrama versionado

`construction-site-handoff.dot` documenta la frontera real: un único recorrido de cuerpos en
MethodBody, publicación postorden, ordenación estable y consumo directo en Construction. El SVG se
genera con `tools/generate_semantic_performance_diagrams.py` y Graphviz de `A:`.

## Validación

- Regenerar todos los SVG dos veces y comparar SHA-256.
- Compilar y ejecutar sólo `nsc_project_construction_model_tests` en O0.
- Sin baseline, CTest general, O1, O2 ni O3.
