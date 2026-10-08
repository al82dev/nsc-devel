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
- Commit: pendiente.
- Ejecución SH41/181: pendiente.
