# Tanda de Validación 14 — Inventario de infraestructura self-host

## Alcance causal

Se retiraron cantidades, extremos ordinales y tablas de rangos duplicadas de
los controladores de:

- `SelfHost.Bootstrap.Infrastructure`;
- `SelfHost.Contracts`.

También se eliminó del cargador de Bootstrap Infrastructure la expectativa de
que una revisión siempre tenga 73 contratos, 438 casos, extremos específicos y
seis casos por contrato. No se modificaron módulos C++, contratos de producto,
casos ni oráculos.

## Corrección

Los controladores exigen ahora relaciones estables:

1. paridad exacta entre diseño, asignación y contratos canónicos;
2. identidades únicas de contrato, archivo y caso;
3. pertenencia caso-contrato idéntica al inventario;
4. cobertura completa y sin traslape de los catálogos;
5. rangos contiguos derivados de los casos vivos de cada contrato;
6. fuentes catalogadas existentes y coincidentes con los módulos C++;
7. disposición `InfrastructurePending` preservada;
8. contradicciones abiertas sólo mientras exista una diferencia de perfil viva;
9. ausencia de ambos componentes en el manifiesto ejecutable.

## Evidencia estática

| Componente | Casos canónicos observados | Contratos observados | Gate |
|---|---:|---:|---|
| SelfHost.Bootstrap.Infrastructure | 438 | 73 | PASS |
| SelfHost.Contracts | 288 | 48 | PASS |
| **Total** | **726** | **121** | **PASS** |

Los dos gates y la compilación sintáctica de los tres controladores Python
modificados pasaron. Estos conteos documentan el corte actual; ya no son
precondiciones codificadas.

## Límites preservados

- módulos C++, contratos de producto, casos y oráculos intactos;
- batería histórica y archivos protegidos intactos;
- sin baseline, compilación local, `C:`, despliegue ni `push`;
- ejecución funcional diferida a la corrida externa del usuario.
