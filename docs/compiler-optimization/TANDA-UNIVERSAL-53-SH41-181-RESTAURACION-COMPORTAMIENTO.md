# Tanda universal 53: restauración explícita del comportamiento previo para SH41/181

## SCOPE LOCK

- Objetivo exacto: probar la 181 con el comportamiento de `LlvmBackend.ns` anterior a
  `f0fad42ef` y `19ec3a57e`, sin eliminar ni reescribir esos commits.
- Naturaleza: restauración correctiva en el estado actual y reproducción aislada.
- Universo autorizado: exclusivamente `nsc.selfhost.self-hosted-generation` en O0.
- Archivos funcionales autorizados: `selfhost/nsc/CodeGen/LlvmBackend.ns`.
- Archivos documentales autorizados: este documento.
- Archivos protegidos: `nscbuilder.py`, CMake, verificadores, contratos, pruebas, fixtures,
  runtime, bibliotecas, NSLink y cualquier contenido de `C:`.
- Acciones prohibidas: borrar, reescribir o revertir commits históricos; ejecutar otras pruebas;
  baseline; deploy; instalación; instrumentación del compilador; escritura en `C:`.
- Criterio de terminación: nuevo commit local, build exclusivo de `nsc` y ejecución aislada de
  SH41/181 con log persistido.

## Evidencia que motivó la restauración

La instrumentación temporal de la tanda 52 escribió demasiadas líneas a `stderr`. El verificador
SH41 mantiene los pipes del compilador hijo sin drenarlos hasta que éste termina; el buffer se
llenó y `nsc1` quedó bloqueado sin CPU. Esa corrida no es evidencia del comportamiento funcional y
la instrumentación se retiró por completo antes de esta comparación.

## Cambios restaurados

El estado actual deshace funcionalmente, mediante edición explícita nueva, todos los cambios de
`LlvmBackend.ns` introducidos por `f0fad42ef` y `19ec3a57e`:

1. retira `PhysicalTypeName` y `PhysicalTypeIs`;
2. restaura la clasificación nominal original de `ScalarType`, `AbiValueType` y
   `SemanticValueType`;
3. restaura `SemanticIntegerBits`, signedness y flotantes sin normalización global;
4. restaura el tratamiento explícito original de strings, booleanos, bitfields, varargs y
   condiciones;
5. restaura `ConvertScalar` para operar con los nombres semánticos originales;
6. elimina completamente `NSC_SH41_181_DIAGNOSTIC_BEGIN/END` y todas sus salidas.

## Prueba mecánica de equivalencia

Antes del commit se ejecutó:

```text
git diff f0fad42ef^ -- selfhost/nsc/CodeGen/LlvmBackend.ns
```

El resultado fue vacío. Por tanto, el contenido funcional actual de ese archivo coincide con la
revisión previa a ambos commits candidatos, mientras la historia completa permanece intacta.

## Ejecución autorizada

`run_sh41_181.py --label previous` construirá únicamente `nsc` y ejecutará directamente SH41 con
`NSC_SELFHOST_PROFILES=O0`. El script y su formato de log permanecen; no instrumentan el
compilador ni modifican infraestructura.

## Estado

- Procesos residuales de SH41: no encontrados.
- Comportamiento anterior: restaurado y comprobado estáticamente.
- Instrumentación del compilador: eliminada.
- Commit funcional: `f6b7e7930`.
- Ejecución SH41/181: aprobada.

## Cotejo y diagnóstico final

### Estado con la normalización global

La evidencia histórica conservada en:

```text
A:\nsc-devel\_nscbuilder\history\universal-o0-t04-r2-20261007\stages\CTest_NSLink.log
```

registra el fallo primario de la 181 antes de todos sus cierres derivados:

```text
Test #181: nsc.selfhost.self-hosted-generation ***Failed
SH41 verifier failed: nsc1.exe/O0 emission failed (1)
nsc-next: error: bootstrap graph rejected: lex=0 parse=1726 semantic=0
tokens=164489 first-error-token=680 kind=Identifier begin=12974 end=12980
```

La imagen `nsc1` existía y arrancaba; el defecto aparecía cuando ese compilador generado procesaba
el grafo fuente para emitir `nsc2`. Por tanto, CMake, descubrimiento de la prueba y enlace inicial
no eran la causa primaria.

### Estado restaurado

La ejecución exclusiva:

```text
python -B A:\nsc-devel\run_sh41_181.py --label previous --jobs 12
```

terminó con salida 0:

```text
SH41 passed: real nsc1->nsc2->nsc3 source compilation, O0, NSLink
```

Evidencia persistida:

- log: `A:\nsc-devel\sh41-181-previous.log`;
- build aislado: `A:\nsc-devel\sh41-181-previous-build.log`;
- reporte: `A:\nsc-devel\build-sh41-181\previous\sh41-report.json`;
- artefactos: `A:\nsc-devel\build-sh41-181\previous`.

Resultados del reporte:

| Campo | Resultado |
|---|---|
| Perfil | `O0` |
| Linker | `NSLink` |
| Estado | `passed` |
| `nsc1 -> nsc2` | `120.389360 s` |
| `nsc2 -> nsc3` | `108.361924 s` |
| SHA-256 objeto `nsc2` | `e02e653bf55a9fe2be266af18f83abfb0d3fe4efbaf87a005710b05c0c1ca39a` |
| SHA-256 objeto `nsc3` | `e02e653bf55a9fe2be266af18f83abfb0d3fe4efbaf87a005710b05c0c1ca39a` |
| Punto fijo | objetos byte a byte idénticos |
| Imágenes verificadas | `nsc2` y `nsc3` |

### Causa confirmada

La regresión de la 181 estaba en la normalización global de nombres calificados por ownership en
el backend self-hosted. `f0fad42ef` extendió esa política desde la conversión genérica puntual a
clasificadores compartidos de tipo, ABI, signedness y conversiones; `19ec3a57e` la propagó a
constantes, bitfields, varargs, condiciones y strings. Así, el compilador generado dejó de conservar
la representación anterior al ejecutar su propio lexer/parser y rechazó un grafo que el compilador
semilla había aceptado.

La prueba contrafactual es directa: manteniendo el árbol vigente, O0 y NSLink, restaurar
`LlvmBackend.ns` exactamente a `f0fad42ef^` convirtió el fallo `nsc1 -> nsc2` en una cadena
`nsc1 -> nsc2 -> nsc3` aprobada y estable. No fue necesario modificar proyectos, pruebas,
verificadores, CMake, runtime ni NSLink.

### Solución aplicada

La solución para la 181 es el commit `f6b7e7930`: retira en el estado actual la política global de
normalización y restaura la clasificación nominal previa. Los commits originales permanecen en la
historia; no fueron borrados, reescritos ni revertidos mediante manipulación de Git.

La 181 queda resuelta en O0 con NSLink. Cualquier futura corrección de un escalar genérico debe ser
local al cruce concreto placeholder/cerrado y no volver a modificar clasificadores compartidos sin
una prueba causal independiente.
