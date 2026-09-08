# 07 — Orden de producción y Definition of Done — Revisión 2

> Rev2 incorpora un primer inventario físico de `A:\native-sharp-historicos`. P1 ya no comienza con
> la pregunta de si existen paquetes: comienza con su catalogación criptográfica, deduplicación y
> enlace a capítulos. El catálogo de chats sigue siendo una tarea independiente y obligatoria.

## 1. No escribir doscientas piezas sin una cadena verificable

El índice maestro tiene 211 capítulos propuestos. No se escribirán todos como borradores aislados
y después se intentará conectarlos. La producción avanzará por tramos cerrados, cada uno con
investigación, capítulos, checkpoint y reconstrucción.

## 2. Secuencia de producción

### Entrega editorial P0 — plan maestro

Contenido de esta carpeta en su estado actual:

- propósito;
- metodología de fuentes;
- mapa histórico;
- índice maestro;
- contrato de capítulo;
- protocolo de checkpoints;
- plan de investigación;
- Definition of Done.

P0 termina cuando los archivos existen, los enlaces funcionan y el índice cubre hasta 1.0 sin
confundir hechos con futuro.

### Entrega editorial P1 — archivo de fuentes

- catálogo de chats;
- catálogo de Markdown;
- catálogo de ZIP/manifests;
- inventario reproducible de los 89,580 archivos observados;
- agrupación por hash de copias exactas y equivalencias normalizadas;
- genealogía física separada del orden lógico de stages;
- cronología 00–20;
- recuperación de los documentos 00–05 y libro previo 00–20;
- registro de anomalías de empaquetado y certificaciones parciales;
- preguntas iniciales al autor.

El inventario exploratorio y el mapa preliminar ya existen en los documentos 08–12 de Rev2. P1 no
se considerará cerrado hasta producir los catálogos derivados previstos en el protocolo forense y
procesar los chats relevantes.

### Entrega editorial P2 — Volúmenes I–III

- capítulos 000–070;
- checkpoints desde directorio vacío a Stage 20;
- baseline procedural reconstruido;
- auditoría por una segunda lectura técnica.

### Entrega editorial P3 — Volúmenes IV–V

- capítulos 071–099;
- snapshots Stage 21–39;
- incidente 35/36 Recovery completamente documentado;
- capstone OOP inicial y RTTI.

### Entrega editorial P4 — Volúmenes VI–VII

- capítulos 100–128;
- ABI de valores, delegates, generics y EH;
- bibliotecas, NSMETA y ownership;
- baseline Stage 50.

### Entrega editorial P5 — Volúmenes VIII–IX

- capítulos 129–159;
- Core, colecciones y sistema;
- Stage 56/57, CC1/CC2 y catálogo de bugs;
- baseline de madurez.

### Entrega editorial P6 — Volúmenes X–XII

- capítulos 160–190;
- 0.7.x, 0.8, 0.9 y E1–E4;
- reproducción del baseline G00 instalado.

### Entrega editorial P7 — Volumen XIII

- capítulos 191–199 escritos conforme ocurra el RC y release;
- ninguna sección futura rellenada con predicciones presentadas como hechos;
- acta final 1.0.0.

### Entrega editorial P8 — Atlas y reconstrucción total

- capítulos 200–210;
- índices cruzados;
- reconstrucción `C000 → C199` en entorno limpio;
- paquete y manifest final de la obra.

## 3. Definition of Done de un capítulo

Un capítulo está `VERIFICADO` cuando:

- cumple todas las secciones aplicables del contrato;
- sus fuentes están catalogadas;
- sus nombres de archivos y símbolos se comprobaron;
- su delta se aplica;
- sus comandos se ejecutan;
- su test rojo falla por la razón esperada;
- su test verde pasa;
- los outputs no fueron escritos de memoria;
- sus limitaciones están explícitas;
- el checkpoint siguiente puede comenzar desde su salida.

Está `CERTIFICADO` cuando además:

- pertenece a un baseline completo;
- el baseline se construye limpio;
- se ejecutan los gates requeridos;
- el manifest pasa antes y después de empaquetar;
- una extracción fresca reproduce resultados;
- la identidad del binario está registrada.

## 4. Definition of Done de un volumen

Un volumen no termina sólo porque todos sus archivos tengan texto. Debe tener:

- narrativa continua;
- glosario local;
- índice de símbolos introducidos;
- mapa de archivos que nacen;
- ejercicios acumulativos;
- un programa transversal que evoluciona;
- baseline reproducible;
- auditoría de referencias adelantadas;
- lista de deuda transferida al volumen siguiente.

## 5. Definition of Done histórica hasta el estado actual

La historia 00→E4 estará completa cuando:

- todos los chats relevantes accesibles hayan sido procesados;
- todos los Markdown existentes hayan sido clasificados;
- cada stage tenga estado de entrada/salida;
- los recoveries y contradicciones tengan explicación;
- cada cifra de test tenga nivel de confianza;
- los checkpoints faltantes estén recuperados o marcados `RECONSTRUCTED`;
- el baseline E1–E4 pueda relacionarse con cada delta previo sin saltos mágicos.

## 6. Definition of Done del producto 1.0 en la obra

La obra no declarará 1.0 terminado hasta contar con evidencia de:

1. identidad pública `nsc 1.0.0`;
2. Native ABI 1.0 y Runtime ABI 1.0 sin cambio incompatible silencioso;
3. NSMETA compatible según la política congelada;
4. paquete instalable desde entorno limpio;
5. compilador construido idéntico al instalado o relación reproducible documentada;
6. proyectos source y consumidores precompilados;
7. `.obj`, `.lib`, DLL y EXE;
8. producer/consumer matrices Debug/Release y O0/O2;
9. `lld-link` y NSLink dentro del alcance prometido;
10. ownership, closures y exceptions cruzando módulos;
11. tests negativos de incompatibilidad ABI con diagnostics precisos;
12. full suite, fixtures, bug triage y gates externos;
13. manifests y hashes publicados;
14. known limitations honestas;
15. reconstrucción documental desde `C000` hasta el release.

## 7. Control de calidad contra documentación superficial

Antes de aceptar cualquier lote se responderá:

- ¿Puede el lector escribir el código sin ver el checkpoint final?
- ¿Sabe por qué existe cada estructura?
- ¿Ve al menos un fallo real o una alternativa?
- ¿Puede predecir el output antes de leerlo?
- ¿Puede localizar el bug si el test falla?
- ¿Conoce lo que todavía no funciona?
- ¿La evidencia corresponde al binario correcto?
- ¿El capítulo aporta más que el Markdown histórico fuente?

Si dos o más respuestas son “no”, el capítulo vuelve a redacción.

## 8. Métricas de avance

Se medirán artefactos, no palabras:

| Métrica | Unidad |
|---|---|
| fuentes catalogadas | número y porcentaje del inventario |
| chats leídos completos | conversación/turnos/adjuntos |
| claims corroborados | afirmaciones con dos autoridades |
| contradicciones | abiertas/resueltas |
| capítulos | planificados/borrador/verificados/certificados |
| checkpoints | recuperados/reconstruidos/certificados |
| deltas | aplicables y probados |
| fixtures | positivos/negativos/ejecución/corruption |
| baselines | build limpio + manifest + extraction |
| recorrido total | último checkpoint reconstruido desde C000 |

El conteo de palabras podrá servir para detectar anomalías, nunca como prueba de calidad.

## 9. Qué no debe hacerse en la siguiente fase

- Empezar por reescribir `130` con más palabras.
- Copiar todo el código actual capítulo por capítulo y llamarlo historia.
- Generar los 211 Markdown vacíos para simular avance.
- Crear checkpoints históricos falsamente fechados.
- Borrar documentos duplicados antes de entender su procedencia.
- Convertir el roadmap de 1.0 en acta de cosas ya realizadas.
- esperar a terminar el producto para documentar los cambios futuros.

## 10. Próximo paso recomendado

Comenzar P1: catálogo de fuentes y recuperación 00–20. El primer resultado útil debe ser una tabla
que conecte cada documento/chat/ZIP de esa era con los capítulos 000–070, seguida del checkpoint
`C000` realmente mínimo. Sólo después se redactará el capítulo 000.
