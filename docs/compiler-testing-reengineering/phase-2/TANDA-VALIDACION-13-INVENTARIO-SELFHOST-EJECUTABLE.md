# Tanda de Validación 13 — Inventario self-host ejecutable y mixto

## Alcance causal

Se retiraron listas y cantidades rígidas de cinco controladores que compilan o
ejecutan fixtures NativeSharp:

- Basic;
- Diagnostics;
- Lexer;
- Source;
- Types.

No se cambió ningún comando de compilación, fixture, caso u oráculo.

## Corrección

El helper común valida ahora cada fila ejecutable contra el inventario vivo:

1. ID único y perteneciente al componente;
2. contrato idéntico al diseño;
3. capa idéntica al diseño y asignación;
4. perfil idéntico a la asignación;
5. partición exacta entre manifiesto y disposiciones no ejecutables;
6. defecto conocido ejecutable y etiquetado de forma consistente;
7. cero clasificaciones extranjeras o no reconocidas.

Basic deriva su contradicción del catálogo correspondiente. Source y Types
derivan `PlanContradiction`, `InfrastructurePending` y `KnownProductDefect` de
sus catálogos. Diagnostics y Lexer exigen que `KNOWN_FAILURE` y la etiqueta
`known-product-defect` describan exactamente el mismo conjunto.

## Evidencia estática

| Componente | Canónicos | Ejecutables | Contradicciones | Infraestructura | Defectos |
|---|---:|---:|---:|---:|---:|
| Basic | 28 | 27 | 1 | 0 | 0 |
| Diagnostics | 28 | 28 | 0 | 0 | 2 |
| Lexer | 53 | 53 | 0 | 0 | 1 |
| Source | 61 | 48 | 11 | 2 | 1 |
| Types | 47 | 36 | 10 | 1 | 2 |
| **Total** | **217** | **192** | **22** | **3** | **6** |

Los cinco inventarios y la sintaxis Python pasaron. No se compilaron ni
ejecutaron fixtures; los resultados funcionales permanecen pendientes de la
corrida externa.

## Límites preservados

- ejecución y oráculos intactos;
- fixtures y proyectos NativeSharp intactos;
- batería histórica y archivos protegidos intactos;
- sin baseline, compilación local, `C:`, despliegue ni `push`.
