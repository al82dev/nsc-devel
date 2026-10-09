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

### Tanda 12A — Compilador self-host

- Leer semánticamente los 29 archivos de `selfhost/nsc/**`.
- Inventariar símbolos y contratos de sus 16 componentes documentales.
- Diseñar casos contractuales sin reutilizar como oráculo la implementación host.
- Separar pruebas directas del compilador NativeSharp de las propiedades de generación self-host.

**Estado:** `LISTA`. La auditoría cerró 29 archivos, 16 componentes, 874 declaraciones, 138 contratos y 1,027
casos diseñados. La evidencia consolidada vive en `audits/TANDA-12A-SELFHOST-AUDIT.md`; no se implementaron ni
ejecutaron pruebas.

### Tanda 12B — Superficies restantes y disposición explícita

- Tratar los 421 archivos restantes de build, empaquetado, contratos self-host, dependencias distribuidas,
  herramientas de soporte y terceros vendorizados.
- Derivar contratos directos sólo para superficies que pertenecen al producto NSC.
- Registrar una justificación verificable de prueba indirecta, integración, upstream o exclusión para cada
  elemento que no deba producir una prueba directa de NSC.
- Impedir que `NO LEÍDO` o `NO VERIFICADO` se conviertan silenciosamente en cobertura de la suite.

### Tanda 12C — Plan de implementación de la suite nueva

- Consolidar los test plans de los 57 componentes documentales.
- Definir ejecutores futuros sin implementar ninguno.
- Separar ejecución rápida, integración, nocturna y semanal.
- Derivar el conteo real de casos sólo después de cerrar las matrices host, self-host y de producto.

**Estado:** `LISTA`. Los 10,193 casos quedaron asignados exactamente una vez: 6,118 al único ejecutable host,
3,655 al compilador real conducido por manifiestos y 420 a la batería histórica congelada. Los 57 componentes
poseen una ola de implementación o una disposición externa explícita. Los documentos normativos son
`architecture/NEW-SUITE-ARCHITECTURE.md`, `architecture/EXECUTION-PROFILES.md` y
`architecture/IMPLEMENTATION-WAVES.md`. No se implementó ni ejecutó ninguna prueba.

### Tanda 13 — Segunda auditoría

- Reenumerar desde cero el universo de archivos y símbolos.
- Comparar inventario contra universo real.
- Rechazar el cierre si existe algún omitido, desconocido o no tratado.
- Emitir el informe de preparación para la Fase 2.

**Estado:** `LISTA PARA REVISIÓN DEL USUARIO`. La reenumeración independiente verificó 974/974 archivos,
8,418 símbolos únicos, 1,309 contratos con fuente resoluble y 10,193 casos asignados exactamente una vez. La
segunda ejecución cerró con cero hallazgos abiertos. El dictamen y sus límites están en
`audits/TANDA-13-INDEPENDENT-PHASE-1-AUDIT.md`. La puerta final reprodujo `22/22 PASS` y queda documentada en
`audits/PHASE-1-COMPLETION-AUDIT.md`; el cierre de la Fase 1 sigue requiriendo una orden explícita del usuario.
No se implementaron ni ejecutaron pruebas.

## Enmienda de integridad previa a Tanda 12

La matriz host cerrada en Tanda 11 contiene 24 componentes, 5,294 símbolos, 870 contratos y 7,360 casos. Esos
conteos no representan por sí solos todo el universo canónico: `components.csv` conserva además 16 componentes
del compilador self-host y 17 componentes de build, contratos, dependencias y soporte. Las Tandas 12A y 12B
existen para tratar ese universo antes de consolidar la arquitectura. Esta enmienda no amplía el alcance: corrige
una partición incompleta del alcance original fijado en `SCOPE-LOCK.md`.

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
