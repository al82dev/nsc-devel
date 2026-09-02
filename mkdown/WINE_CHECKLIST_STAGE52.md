# Checklist para Alberto — verificación en Windows real (Task 27 / P0 3.2)

Cuatro `.exe` reales, ya compilados, en `windows_execution_checklist/`. No requieren instalar
nada del compilador -- son binarios PE32+ standalone.

## Cómo correr

Abrir `cmd.exe` o PowerShell en la carpeta `windows_execution_checklist/` y ejecutar cada uno,
revisando el exit code inmediatamente después (`echo %ERRORLEVEL%` en cmd.exe, o `$LASTEXITCODE`
en PowerShell):

```bat
repro_1_minimal.exe
echo %ERRORLEVEL%

repro_2_owned_no_nested_call.exe
echo %ERRORLEVEL%

repro_3_owned_nested_call.exe
echo %ERRORLEVEL%

Stage52Collections.exe
echo %ERRORLEVEL%
```

## Qué esperar / qué significa cada resultado

| Paso | Exit code esperado | Si en cambio crashea / no devuelve ese código |
|---|---|---|
| `repro_1_minimal.exe` | **42** | Algo más grave y general está roto (no específico de este hallazgo); avisar de inmediato. |
| `repro_2_owned_no_nested_call.exe` | **5** | Igual que arriba. |
| `repro_3_owned_nested_call.exe` | **1** | **Confirma el bug real** descrito en `WINE_EXECUTION_FINDINGS_STAGE52.md` (`emitOwnedParameterDestroy` sin chequeo de tipo valor). Alta prioridad, probablemente afecta etapas anteriores también. |
| `Stage52Collections.exe` | **0** (el `Main()` real usa `+` entre resultados, no hay valor fijo esperado documentado aquí -- lo importante es que NO crashee) | Si crashea igual que `repro_3`, confirma que el mismo bug alcanza a la superficie completa de `NativeSharp.Core.Collections.Generic`. |

## Los tres resultados posibles

1. **Los 4 corren limpio, sin crash** → el hallazgo de Wine 9.0 en este sandbox era un artefacto
   de esa emulación específica, no un bug real de Windows. Igual vale la pena corregir el código
   fuente señalado (higiene), pero no es bloqueante.
2. **`repro_1`/`repro_2` corren bien, `repro_3`/`Stage52Collections` crashean** → confirma el bug
   real descrito. Recomendación: tratarlo como blocker de alta prioridad antes de considerar
   "cerrada" la ejecución en Windows de cualquier etapa (no sólo Stage 52) que use un parámetro
   `owned` de tipo valor junto con una llamada a otro método dentro del mismo cuerpo.
3. **Incluso `repro_1`/`repro_2` fallan** → algo más fundamental está roto (entorno de la máquina
   de pruebas, antivirus interceptando el binario sin firmar, etc.) -- no es evidencia sobre este
   hallazgo específico, hay que investigar el entorno de ejecución primero.

Reportar de vuelta los 4 exit codes (o el comportamiento de crash observado) para decidir el
siguiente paso.
