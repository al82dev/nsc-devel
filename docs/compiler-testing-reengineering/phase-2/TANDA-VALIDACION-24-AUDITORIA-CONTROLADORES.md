# Tanda de Validación 24: auditoría residual de controladores

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA Y DIRIGIDA

## Universo barrido

Se escanearon los `118` archivos Python de
`tests/compiler-suite/controller/integration` en busca de:

- listas privadas de IDs de casos;
- cantidades fijas de casos, resultados, eventos, contratos o componentes;
- comparaciones numéricas que pudieran representar una revisión congelada;
- hashes, versiones y cadenas exactas;
- particiones `InfrastructurePending` privadas.

## Clasificación

No queda ninguna lista o cantidad privada de casos dependiente de la revisión.
Las coincidencias restantes se conservaron porque expresan contratos estables:

- forma tabulada del registro del host;
- variantes internas declaradas por los contratos BuildSystem y Packaging;
- un único archivo de paquete por publicación;
- continuidad de rangos derivada del inventario vivo;
- estados no nulos derivados de los resultados esperados;
- secuencia y cardinalidad del protocolo de eventos, derivadas de los casos.

## Prueba permanente

`test_live_manifest_closure.py` carga los inventarios y el manifiesto reales y,
para cada combinación no vacía de componente, perfil y build mode, ejecuta el
modelo causal iterativo del ledger hasta consumir todos los casos. No contiene
conteos esperados de componentes, casos ni selecciones.

## Evidencia

- Controladores escaneados: `118`.
- Casos ejecutables cargados: `5,695`.
- Componentes ejecutables encontrados: `30`.
- Selecciones causales no vacías cerradas: `176`.
- Selecciones acumuladas auditadas: `32,972`.
- Suite Python completa del controlador: `71/71` PASS.
- `compileall` del controlador: PASS.

No se modificaron casos, oráculos, resultados esperados, producto ni batería
histórica. No se configuró, compiló ni ejecutó producto; no hubo baseline ni
despliegue.
