# Plan maestro — Reingeniería de pruebas de NSC

## Resultado de la Fase 1

Esta fase entrega el conocimiento verificable necesario para construir una suite nueva. No implementa pruebas. El número final de casos no se fija por cuota: se deriva del inventario completo de contratos, variantes, errores y bordes.

## Modelo de trazabilidad

La cadena mínima será:

`archivo -> símbolo -> responsabilidad -> contrato -> caso diseñado -> oráculo -> capa de prueba`

Capas futuras admitidas:

- **unitaria directa**: lógica aislable con dependencias controladas;
- **contrato de componente**: interacción interna que no debe falsificarse con mocks artificiales;
- **integración**: contrato entre dos o más componentes;
- **programa `.ns` mínimo**: comportamiento observable de punta a punta;
- **self-host**: propiedades que sólo existen al recompilar el compilador;
- **no directa**: símbolo cubierto mediante una superficie estable superior, con justificación explícita.

## Campos del inventario de símbolos

Cada símbolo inventariado debe contener, como mínimo:

| Campo | Propósito |
|---|---|
| Identificador estable | Ubicarlo sin depender de un número de línea mutable |
| Archivo y componente | Propiedad y trazabilidad |
| Tipo de símbolo | Clase, estructura, función, método, enum, constante o plantilla |
| Responsabilidad | Qué debe hacer |
| Entradas | Parámetros, configuración y estado consumido |
| Precondiciones | Qué debe ser cierto antes de invocarlo |
| Salidas | Retorno, efectos y artefactos |
| Poscondiciones | Qué debe ser cierto después |
| Estados e invariantes | Propiedades que deben conservarse |
| Errores | Fallos, diagnósticos y recuperación |
| Dependencias | Productores y consumidores |
| Observabilidad | Cómo se comprueba sin acoplarse a detalles accidentales |
| Capa futura | Tipo de prueba recomendado |
| Casos diseñados | Válidos, inválidos y límites |
| Estado | Inventariado, revisado, tratado o bloqueado |

## Tandas documentales

### Tanda 01 — Alcance e inventario canónico

- Congelar la batería histórica y la infraestructura previa.
- Enumerar archivos de producción y archivos de construcción relevantes.
- Identificar blancos, ejecutables, bibliotecas y fuentes generadas.
- Crear el cursor reproducible y los conteos base.

### Tanda 02 — Arquitectura y dependencias

- Derivar el grafo real desde archivos de construcción e inclusiones.
- Definir propiedad de componentes y fronteras públicas/internas.
- Ordenar las tandas posteriores por dependencia, no por conveniencia.

### Tandas 03 a 10 — Auditoría por componente

Para cada archivo y símbolo:

- documentar contrato completo;
- registrar estados, errores, invariantes y dependencias;
- decidir la capa futura de prueba;
- diseñar positivos, negativos, bordes y combinaciones relevantes;
- no crear código de prueba.

### Tanda 11 — Contratos transversales

- Diagnósticos y recuperación.
- Identidad, determinismo y cachés.
- Ownership, lifetime y transferencia de estado.
- Contratos de ABI, metadata y artefactos.
- Cancelación, concurrencia y ordenamiento.
- Opciones, modos y matrices de configuración.

### Tanda 12 — Plan de implementación de la suite nueva

- Consolidar los test plans por componente.
- Definir ejecutores futuros sin implementar ninguno.
- Separar ejecución rápida, integración, nocturna y semanal.
- Derivar el conteo real de casos desde la matriz cerrada.

### Tanda 13 — Segunda auditoría

- Reenumerar desde cero el universo de archivos y símbolos.
- Comparar inventario contra universo real.
- Rechazar el cierre si existe algún omitido, desconocido o no tratado.
- Emitir el informe de preparación para la Fase 2.

## Reglas para diseñar casos

Cada contrato debe cubrir, cuando aplique:

- caso nominal mínimo;
- variantes válidas significativas;
- entradas vacías, mínimas y máximas;
- valores límite y transiciones entre clases de equivalencia;
- sintaxis o estado inválido;
- error esperado y recuperación posterior;
- repetición, orden y determinismo;
- interacción con dependencias reales relevantes;
- configuración o plataforma que cambie el contrato;
- regresiones históricas sólo cuando expresen todavía un contrato vigente.

No se duplicará manualmente la estructura de control del código. La cobertura de ramas futura medirá automáticamente la implementación que exista en ese momento.

## Criterio de paso a Fase 2

No se implementará una sola prueba nueva hasta que la matriz documental permita responder, para cada caso:

1. qué contrato valida;
2. por qué existe;
3. qué entrada usa;
4. cuál es su oráculo exacto;
5. a qué componente pertenece;
6. qué dependencias requiere;
7. en qué frecuencia debe ejecutarse.
