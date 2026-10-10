# Validacion 133 - identidad canonica de Sema

## Alcance

Esta tanda corrige una sola divergencia de identidad del registro vivo. No
modifica cuerpos de prueba, contratos, disposiciones, inventarios ni el gate de
paridad.

## Evidencia causal

La conciliacion completa posterior a Validacion 132 produjo:

| Magnitud | Antes | Despues |
|---|---:|---:|
| Filas vivas de `--list` | 8,054 | 8,054 |
| IDs canónicos ausentes del diseño | 1,075 | 1,062 |
| IDs no canónicos inesperados | 13 | 0 |
| Contratos/componentes cambiados | 0 | 0 |

Los trece pares eran biyectivos:

```text
TS-SEMA-087..TS-SEMA-099  <->  TS-SEMA-87..TS-SEMA-99
```

`registerSemaAnalyzerLifecycleModule` era el único productor de esas
identidades y concatenaba el ordinal sin conservar el ancho canónico de tres
dígitos. Los cuerpos, contratos y componentes ya coincidían.

## Cambio

Para ordinales menores que 100, el productor agrega exactamente un cero antes
de `std::to_string(ordinal)`. Los ordinales 100 y 101 permanecen idénticos.

## Validacion dirigida

- reconstrucción incremental de `nsc_component_tests`: `2/2` pasos;
- `nsc_component_tests --list`: salida `0`, `8,054` filas;
- inesperados fuera de infraestructura: `0`;
- diferencias de contrato/componente: `0`;
- gate `nsc.compiler-suite.registry-inventory`: alcanza la comparación en
  `9.17 s` y conserva como fallo únicamente los `1,062` IDs canónicos todavía
  no materializados.

El gate completo todavía no pasa y no se presenta como cerrado. La siguiente
tanda debe clasificar y resolver exhaustivamente los diez grupos restantes sin
eliminar pruebas ni convertir estados pendientes en éxitos ficticios.

No se ejecutó baseline, no se desplegó y no se tocó `C:`.
