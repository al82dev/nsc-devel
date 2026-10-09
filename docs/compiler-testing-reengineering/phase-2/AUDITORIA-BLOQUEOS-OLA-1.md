# Auditoría causal de bloqueos — Ola 1

Fecha: 2026-10-09

Revisión auditada: `446411765`

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Alcance | Los 14 casos no implementados de la ola 1 |
| Naturaleza | Auditoría estática y consolidación causal |
| Producto | Leer y clasificar; no modificar |
| Suite nueva | No inventar oráculos, `SKIP`, UB ni fallos probabilísticos |
| Suite histórica | Conservar intacta; no ejecutar |
| Excluido | Bibliotecas, SDK, bindings y cualquier repositorio externo |
| Prohibido | Baseline, cobertura, `C:`, despliegue y remotos |

## Resultado cuantitativo

```text
casos previstos de la ola 1 = 747
casos implementados y verificados = 733
casos auditados como bloqueados = 14
casos sin clasificar = 0
componentes externos activos = 0
```

Los 14 pendientes no forman un conjunto homogéneo. La segunda pasada contra el
código vigente los divide en cuatro fronteras de factibilidad de prueba, siete
defectos observables del producto, dos huecos de superficie pública y una
decisión contractual todavía no definida.

## Fronteras de factibilidad de prueba

| Caso | Símbolo | Evidencia causal | Acción mínima necesaria |
|---|---|---|---|
| `TS-SRC-008` | `SourceFile::size`, `buildLineStarts` | Alcanzar la saturación exige construir y recorrer un `std::string` mayor que 4 GiB. No existe seam ni representación virtual. | Autorizar el fixture WEEKLY de más de 4 GiB o una frontera verificable de producto. |
| `TS-SRC-040` | `SourceManager::readFileBuffer` | No hay punto coordinable entre `file_size` y `ifstream::read`; una mutación por tiempo sería probabilística. | Autorizar un seam de coordinación o un mecanismo determinista equivalente. |
| `TS-SRC-041` | `SourceManager::readFileBuffer` | No hay punto coordinable entre `file_size`, la lectura y el `peek` final. | Autorizar un seam de coordinación o un mecanismo determinista equivalente. |
| `TS-SRC-049` | `SourceManager::getFile`, `files_` | La desreferencia posterior a realocación es UB; Debug/Release no ofrecen ASan ni aislamiento causal del fallo. | Autorizar un perfil ASan versionado y ejecución aislada en subprocess. |

Ninguno puede resolverse honestamente mediante espera, polling, comparación de
punteros inválidos, acceso privado, duplicación del algoritmo o un oráculo que
acepte la ausencia de evidencia.

## Defectos observables del producto

| Caso | Símbolo | Defecto confirmado | Corrección mínima fuera de Fase 2 |
|---|---|---|---|
| `TL-LANG-019` | `computeWindowsX64AggregateLayout` | `alignUp` y la suma de campos desbordan y publican un layout envuelto. | Aritmética comprobada antes de alinear y sumar. |
| `TL-LANG-035` | `computeClosureLayout` | La suma y el redondeo final aceptan overflow y devuelven tamaño cero. | Aritmética comprobada y fallo transaccional con error. |
| `TL-LANG-099` | `expressionLoweringContract` | Un enum externo indexa fuera del arreglo y produce UB. | Validación de rango y política explícita de rechazo o fallback. |
| `TL-LANG-121` | `specializationStrictlyDominates` | Sólo el candidato se deduplica; duplicados del fallback alteran la relación. | Normalizar ambos conjuntos antes de comparar cardinalidad y contención. |
| `TL-LANG-230` | `decodeEscapeSummary` | Acepta tokens numéricos parciales y conserva output parcial al fallar. | Exigir consumo completo de `from_chars` y decodificar en un temporal. |
| `TL-LANG-250` | `smartPointerFamilyContract` | Un enum externo indexa fuera de `kSmartPointerFamilies` dentro de una función `noexcept`. | Frontera segura de rango y política explícita de rechazo. |
| `TL-LANG-262` | `computeWindowsX64VariantLayout` | El `alignUp` final puede desbordar y devolver tamaño cero. | Comprobar también el redondeo final. |

Estos casos no pueden agregarse como pruebas verdes sin congelar comportamiento
incorrecto. Tampoco pueden agregarse como fallos permanentes porque la Fase 2
exige una suite verificable y prohíbe modificar producto para satisfacerla.

## Huecos de superficie pública

| Caso | Símbolo | Hueco | Decisión necesaria |
|---|---|---|---|
| `TL-LANG-034` | `computeClosureLayout`, `ClosureLayout` | El plan exige offsets exactos, pero `ClosureLayout` sólo expone `size` y `alignment`. | Exponer offsets o corregir expresamente el contrato de prueba. |
| `TL-LANG-201` | `IntegerArithmeticOperation` | El plan exige remainder, pero el modelo sólo contiene Add, Subtract, Multiply, Divide y shifts. | Añadir `Remainder` al producto o reasignar el caso a la superficie real que lo implemente. |

Una prueba parcial de tamaño/alineación o sólo de división no cerraría ninguno de
estos IDs porque ocultaría la porción expresamente exigida por su test plan.

## Decisión contractual pendiente

| Caso | Símbolo | Contradicción | Alternativas explícitas |
|---|---|---|---|
| `TL-LANG-116` | `parseConstGenericArgumentMarker` | El formato no delimita `normalizedValue`; todo byte posterior al segundo `:` pertenece al valor, por lo que “trailing inválido” no es representable. | Versionar el formato con longitud/terminador, validar el valor según su tipo o retirar expresamente esa expectativa del caso. |

La auditoría no elige unilateralmente una alternativa porque cualquiera cambia
el contrato aprobado en Fase 1.

## Gate de cierre de la auditoría

```text
inventariados = 14
revisados = 14
clasificados = 14
sin_revisar = 0
omitidos = 0
desconocidos = 0
implementables_sin_cambiar_alcance = 0
```

La ola 1 permanece `BLOQUEADA` en 733/747. Avanzar a la ola 2, reducir el
denominador, simular los casos o convertir defectos en resultados esperados
violaría el `SCOPE LOCK`. El siguiente paso requiere una decisión explícita que
autorice, por caso o por categoría, ampliar infraestructura, corregir producto o
enmendar el test plan.

## Adenda de decisión — 2026-10-09

La autoridad del producto resolvió posteriormente las cuatro categorías sin
alterar el denominador de 747 casos:

1. Los siete defectos observables conservarán pruebas reales. Esas pruebas pueden
   permanecer rojas hasta que el producto sea corregido en una tarea distinta;
   no se convierten en éxitos ni en exclusiones.
2. `TL-LANG-034` y `TL-LANG-201` se clasificaron inicialmente como capacidades
   de producto pendientes.
3. `TS-SRC-008`, `TS-SRC-040`, `TS-SRC-041` y `TS-SRC-049` quedan pendientes de
   infraestructura especial, con sus alternativas documentadas en
   `PENDIENTES-TESTING-Y-FEATURES.md`.
4. La contradicción de `TL-LANG-116` se resolvió a favor del formato implementado:
   `normalizedValue` es el sufijo completo. La expectativa de un `trailing`
   separable era un defecto del test plan y se retiró únicamente de ese caso.

Esta adenda reemplaza sólo la decisión operativa final de la auditoría original.
La evidencia y clasificación histórica anteriores se conservan. La ola vuelve a
`EN CURSO`; sus pendientes siguen visibles y no se permite declararla completa
mientras haya casos sin verificar.

### Segunda adenda — contrato real de producto

La autoridad aclaró que la suite debe probar sólo capacidades que ya existen en
el compilador. Por ello, la clasificación del punto 2 anterior queda sustituida:

- `TL-LANG-034` conserva `computeClosureLayout`, pero limita su oráculo a los
  campos públicos `size` y `alignment`; no exige offsets inexistentes.
- `TL-LANG-201` conserva la división, división por cero y `signed min / -1` con
  las cuatro políticas; retira remainder porque el enum público no lo contiene.

Ambos fueron errores del test plan y vuelven a ser casos implementables. No se
agrega superficie al compilador y no se altera ningún contrato de producto.
