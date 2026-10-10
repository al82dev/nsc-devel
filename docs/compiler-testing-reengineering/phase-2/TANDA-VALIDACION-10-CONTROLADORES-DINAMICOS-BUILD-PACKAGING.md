# Tanda de Validación 10 — Denominadores vivos de BuildSystem y Packaging

## Alcance causal

Se corrigió una sola clase de defecto: los dos controladores de la ola 9
rechazaban una revisión válida cuando cambiaban cantidades, rangos ordinales o
listas de IDs congeladas en código.

No se modificaron casos, contratos, oráculos, manifiestos, disposiciones,
producto ni batería histórica.

## Causa confirmada

`run_buildsystem_gate.py` fijaba en código 76 contratos, 456 casos, 36 casos
ejecutables, 420 delegados, dos defectos conocidos y la distribución de
perfiles. `run_packaging_gate.py` fijaba tres contratos, 18 casos, 12
ejecutables, seis pendientes, cuatro defectos y una contradicción.

Esas cifras describen el corte actual, pero no son contratos funcionales. Una
alta o baja legítima obligaba a editar el controlador aunque todas las fuentes
canónicas fueran coherentes.

## Corrección

Ambos controladores ahora descubren y cotejan:

1. casos únicos en diseño y asignación;
2. contratos únicos y existentes;
3. mismo contrato para cada caso en todas las fuentes;
4. catálogo completo, sin traslapes ni IDs extranjeros;
5. partición ejecutable y no ejecutable derivada del catálogo;
6. clasificaciones únicas y compatibles con esa partición;
7. manifiesto exactamente igual al conjunto ejecutable;
8. etiquetas de defecto iguales a las clasificaciones vivas;
9. denominador completo, sin omisiones ni traslapes.

Packaging deriva además la existencia de la contradicción de perfiles al
comparar `frequency` con `default_profile`; no exige que exista exactamente un
registro por una cifra histórica.

Los oráculos semánticos permanecen. En particular, el caso representativo de
paquetizado continúa exigiendo exactamente un archivo `.7z`, porque esa
cardinalidad pertenece al comportamiento observable del producto.

## Evidencia del corte

La validación dirigida cargó las fuentes reales sin compilar ni ejecutar casos:

| Componente | Canónicos | Ejecutables | No ejecutables | Defectos conocidos |
|---|---:|---:|---:|---:|
| BuildSystem | 456 | 36 | 420 delegados | 2 |
| Packaging | 18 | 12 | 6 pendientes | 4 |

Resultado: ambos validadores aceptaron la paridad completa. También se comprobó
que ambos rechazan un ID duplicado y una omisión del manifiesto. La sintaxis
Python, el JSON del cursor y la limpieza del diff también pasaron. La ejecución
funcional queda pendiente de la corrida externa; esta tanda no declara casos
funcionales como `PASS`.

## Límites preservados

- sin baseline;
- sin compilación local;
- sin despliegue ni escritura en `C:`;
- sin cambios a los seis archivos protegidos;
- sin cambios a pruebas, oráculos o producto;
- sin `push` remoto.
