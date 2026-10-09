# Pendientes de testing y capacidades — Fase 2

Fecha: 2026-10-09

## SCOPE LOCK local

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Producto | Documentar; no corregir el compilador en esta tanda |
| Suite nueva | Conservar los IDs y oráculos; no simular éxito |
| Suite histórica | Intacta; no ejecutar |
| Componentes externos | Excluidos: bibliotecas, SDK, bindings y otros repositorios |
| Infraestructura prohibida ahora | Baseline, cobertura, despliegue y `C:` |

## Capacidades de producto pendientes

| Caso | Capacidad ausente | Estado | Condición para retomarlo |
|---|---|---|---|
| `TL-LANG-034` | `ClosureLayout` no publica los offsets exactos exigidos por el contrato. | `FEATURE PENDIENTE` | Definir y exponer los offsets como superficie estable, o aprobar un contrato distinto. |
| `TL-LANG-201` | `IntegerArithmeticOperation` no representa remainder. | `FEATURE PENDIENTE` | Incorporar `Remainder` al modelo y a su semántica antes de implementar la prueba. |

Estos casos no se eliminan del inventario ni reducen el denominador. Tampoco se
implementan parcialmente: tamaño/alineación sin offsets no cerraría
`TL-LANG-034`, y probar división no sustituiría remainder en `TL-LANG-201`.

## Infraestructura especial pendiente

### `TS-SRC-008` — saturación de tamaño superior a 4 GiB

`SourceFile::size` y `buildLineStarts` sólo alcanzan la frontera contractual
después de construir y recorrer contenido real mayor que 4 GiB. El producto no
ofrece una representación virtual ni un seam que permita expresar ese tamaño sin
asignarlo.

- Opción fiel sin cambiar producto: proceso aislado `WEEKLY`, con preflight de
  memoria y disco, límite temporal y evidencia progresiva. Su costo supera el de
  una prueba normal y no pertenece al gate rápido.
- Opción que cambia producto: introducir una representación/seam verificable de
  tamaño. No se autoriza sólo para facilitar la prueba porque ampliaría la
  superficie productiva.
- Recomendación actual: mantenerlo pendiente hasta aprobar explícitamente un gate
  de recursos pesados. No intentar una muestra pequeña, porque no ejercita la
  saturación.

### `TS-SRC-040` y `TS-SRC-041` — mutación coordinada durante lectura

Ambos casos necesitan cambiar el archivo exactamente después de consultar
`file_size` y antes de completar `ifstream::read`/`peek`. Un segundo hilo basado
en tiempos no puede garantizar ese interleaving y produciría una prueba flaky.

- Opción preferida: ejecutable de prueba aislado con intercepción determinista de
  la operación de sistema usada por la lectura. El controlador libera la
  truncación o crecimiento al observar el punto exacto.
- Alternativa: seam de coordinación inyectable en producto, únicamente si una
  auditoría demuestra que la intercepción no es viable y el cambio se autoriza
  como superficie de test.
- Requisitos: subprocess por caso, timeout, log de cada transición y comprobación
  de que la mutación ocurrió en la ventana requerida. No se admite polling por
  tiempo ni repetir hasta que falle.
- Estado: `INFRAESTRUCTURA DETERMINISTA PENDIENTE`; hace falta identificar la API
  efectiva importada por la implementación de `std::filesystem`/`ifstream` antes
  de elegir la técnica de intercepción.

### `TS-SRC-049` — invalidación de referencia por realocación

El caso conserva una referencia a un elemento de `std::vector<SourceFile>` y la
usa después de una realocación. Esa desreferencia es comportamiento indefinido;
comparar direcciones sin usar el objeto no demuestra el defecto.

- Infraestructura requerida: target separado instrumentado con AddressSanitizer
  y ejecución en subprocess.
- Oráculo: diagnóstico de use-after-free/use-after-reallocation y terminación
  controlada del proceso hijo; el proceso principal de la suite no debe quedar
  contaminado.
- Aislamiento: este perfil no forma parte del binario ordinario `/MT` Debug o
  Release y no debe introducir el runtime del sanitizer en esos artefactos.
- Estado: `ASAN PENDIENTE`; se requiere definir y validar el perfil versionado de
  build antes de activar el caso.

## Defectos de producto con prueba conservada

Los siguientes casos pertenecen a la suite y deben materializarse con el oráculo
correcto aunque inicialmente fallen:

| Caso | Defecto que debe revelar |
|---|---|
| `TL-LANG-019` | Overflow en layout de agregado. |
| `TL-LANG-035` | Overflow en layout de closure. |
| `TL-LANG-099` | Enum externo fuera de rango en expresión. |
| `TL-LANG-121` | Dominancia asimétrica ante duplicados del fallback. |
| `TL-LANG-230` | Escape numérico parcial y salida parcial tras error. |
| `TL-LANG-250` | Enum externo fuera de rango en smart pointer. |
| `TL-LANG-262` | Overflow en alineación final de variant. |

Su estado será `PRUEBA IMPLEMENTADA — DEFECTO ABIERTO` hasta que una tarea de
producto corrija la causa. Está prohibido marcarlos `SKIP`, `XFAIL`, invertir la
expectativa o aceptar el comportamiento defectuoso.

## Corrección del caso mal definido

`TL-LANG-116` era un defecto del test plan. El productor crea:

```text
$nsc_const$<typeLength>:<type>:<normalizedValue>
```

El parser asigna a `normalizedValue` todos los bytes posteriores al segundo
separador. No existe longitud, terminador ni gramática externa que permita
distinguir un supuesto `trailing` del valor. La prueba corregida conserva los
rechazos verificables de prefijo, decimal, overflow y separador, y confirma que
un marker válido consume el sufijo completo. No cambia el compilador.

## Efecto sobre la secuencia de trabajo

Los seis pendientes especiales permanecen visibles y los siete defectos no se
ocultan. La Fase 2 puede continuar por contratos y olas independientes, pero no
puede declarar completa la ola 1 ni verificar los casos rojos o pendientes hasta
que sus condiciones reales se resuelvan.
