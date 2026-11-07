# Alcance del transporte atómico

Se prepara un atributo separado de `volatile` para transportar el contrato de
un global C hacia HIR, MIR y sus cachés. No habilita acceso ni supone que los
punteros, campos o aggregates ya sean representables.
