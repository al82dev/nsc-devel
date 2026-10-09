# Olas de implementación de la suite nueva

## Regla de avance

La Fase 2 implementará una ola completa antes de avanzar. Cada caso se prueba de forma puntual durante su
implementación; al cerrar la ola se ejecuta el conjunto de esa ola y todas sus dependencias ya cerradas. No se corre
baseline. La batería histórica sólo entra en WEEKLY.

Un fallo de productor detiene sus consumidores con `BLOQUEADO_POR`. No se continúa con artefactos viejos ni se
declara diagnosticado un consumidor que no recibió una entrada válida.

## Resumen

| Ola | Propósito | Componentes | Archivos | Contratos | Casos |
|---:|---|---:|---:|---:|---:|
| 1 | Fundaciones host | 5 | 71 | 87 | 747 |
| 2 | Sintaxis y semántica frontal | 4 | 50 | 106 | 1,197 |
| 3 | IR y optimización | 6 | 128 | 223 | 1,784 |
| 4 | Sesión, frontend y method bodies | 2 | 145 | 115 | 920 |
| 5 | Backend, ABI, runtime y enlace | 4 | 74 | 200 | 1,600 |
| 6 | Producto, driver y servicio | 3 | 56 | 139 | 1,112 |
| 7 | Bibliotecas y SDK | 9 | 110 | 101 | 606 |
| 8 | Compilador self-host | 16 | 29 | 138 | 1,027 |
| 9 | Infraestructura y oráculos self-host | 2 | 122 | 121 | 726 |
| 10 | Build, instalación y paquete | 2 | 79 | 79 | 474 |
| 11 | Disposición externa | 4 | 110 | 0 | 0 |

La asignación fila por fila vive en `inventory/implementation-waves.csv`.

## Ola 0 — Infraestructura mínima

No contiene casos de producto. Implementa primero:

- schema del manifiesto;
- registro de casos del binario consolidado;
- protocolo JSONL común;
- selector compartido;
- modelo de estados y `BLOQUEADO_POR`;
- directorio temporal aislado;
- comparación de oráculos estructurados;
- reportes progresivos y cancelación.

El gate de salida es una prueba de la infraestructura contra fixtures sintéticos del propio framework, sin usar
contratos de NSC como demostración circular.

## Ola 1 — Fundaciones host

Componentes: `Host.Basic`, `Host.Source`, `Host.Target`, `Host.Language`, `Host.Diagnostics`.

Establece valores, ubicaciones, transacciones, diagnósticos, target y vocabulario compartido. Es la base del
ejecutable consolidado y debe cerrar antes de cualquier parser o pipeline.

## Ola 2 — Sintaxis y semántica frontal

Componentes: `Host.Lexer`, `Host.AST`, `Host.Parser`, `Host.Sema`.

Implementa positivos, negativos, recuperación y límites con entradas pequeñas. Los dumps sólo son oráculo cuando el
contrato los declara; en los demás casos se comparan tokens, nodos, IDs y diagnósticos estructurados.

## Ola 3 — Representaciones y optimización

Componentes: `Host.HIR`, `Host.MIR`, `Host.CFG`, `Host.Dominance`, `Host.SSA`, `Host.Passes`.

Los casos separan construcción, verificación y transformación. Cada pass prueba precondición, cambio esperado,
idempotencia cuando aplica, rechazo de IR inválido y preservación de invariantes.

## Ola 4 — Sesión y frontend

Componentes: `Host.Compilation`, `Host.Frontend`.

Aquí vive `ProjectMethodBodyModel`. Sus casos se agrupan por contrato semántico y no por método privado, para que una
futura modularización conserve la suite. La implementación puede introducir adaptadores de test estrechos, pero no
exponer todo el estado interno ni congelar la clase monolítica actual.

## Ola 5 — Backend y producto nativo

Componentes: `Host.Interop`, `Host.LLVM`, `Host.Runtime`, `Host.Linker`.

Usa IR normalizado, objetos, metadata, PE/COFF, ABI y ejecución mínima. Las pruebas NativeSharp enlazables usan
NSLink. Los recursos temporales, procesos e imports se verifican y liberan por caso.

## Ola 6 — Comandos y servicio

Componentes: `Host.Driver`, `Host.LanguageService`, `Host.nsc`.

Prueba parsing de comandos, selección de acciones, proyectos, artefactos, servidor de lenguaje y códigos de salida.
No compara banners o strings incidentales salvo que sean parte de la interfaz pública declarada.

## Ola 7 — Bibliotecas y SDK

Componentes: las siete bibliotecas NativeSharp, procedencia y SDK Win64.

Se prueban con programas `.ns` mínimos compilados por el producto de las olas anteriores. Los stubs intrínsecos se
validan por lowering y resultado, nunca ejecutando su cuerpo fuente como falso oráculo.

## Ola 8 — Compilador self-host

Componentes: los 16 módulos `SelfHost.*` escritos en NativeSharp.

Primero valida sus contratos como código NativeSharp normal; no inicia todavía una cadena completa de generaciones.
Los casos de lexer, parser, semántica e IR self-host deben tener oráculos independientes del host cuando la
equivalencia sea precisamente lo que se intenta demostrar.

## Ola 9 — Bootstrap self-host

Componentes: `SelfHost.Bootstrap.Infrastructure` y `SelfHost.Contracts`.

Implementa generaciones, equivalencia, fixed point, determinismo y promoción sólo después de que el compilador
self-host individual ya está probado. Los contratos no congelan hashes o versiones de herramientas incidentales.

## Ola 10 — Construcción y paquete

Componentes: `BuildSystem` y `Packaging`.

Valida configuración, instalación, paquete y cierre de artefactos. Los adaptadores históricos permanecen weekly. La
prueba del compilador no depende de una versión exacta de Python, CMake o Ninja cuando la conducta observable es la
misma.

## Ola 11 — Disposición externa

Componentes: `Tooling`, `ThirdParty.Documentation`, `ThirdParty.zlib`, `ThirdParty.zstd`.

No genera casos propios de NSC. Tooling tendrá su suite si se mantiene; zlib y zstd conservan autoridad upstream y
reciben únicamente smoke de integración a través del build que los consume.

## Gate de cada ola

1. todos los casos asignados están implementados;
2. no existen `SKIP` temporales ni oráculos vacíos;
3. los positivos pasan y los negativos fallan por la causa esperada;
4. los bloqueos señalan al productor real;
5. el reporte progresivo sobrevive a cancelación;
6. el documento y la matriz reflejan el estado real;
7. el commit incluye código, pruebas y Markdown del cambio;
8. no se afirma cierre global por una ejecución enfocada.
