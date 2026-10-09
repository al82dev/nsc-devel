# Corrección de TB-BAS-066 — Preflight de espacio acotado

## Defecto y evidencia

`TB-BAS-066` creaba un archivo sparse con tamaño lógico igual al espacio disponible más un byte. El producto
consultaba primero la capacidad y debía rechazarlo antes de copiar. Durante una corrida con poco espacio, otra
ejecución obtuvo una capacidad distinta y alcanzó `CopyFileW`; Windows comenzó a materializar una copia densa.
El timeout terminó el proceso y dejó un sibling parcial que redujo A: a 512 KB libres.

La evidencia causal fue Win32 112 en `TB-BAS-065`, el proceso superviviente `nsc_component_tests.exe --all` y
el fixture `.nsc-suite-TB-BAS-066-41548` con `oversized-stage.lib` y un sibling parcial. Después de detener sólo
esa corrida y retirar exclusivamente el fixture se recuperaron aproximadamente 690 GB.

## Corrección mínima

El caso conserva exactamente el contrato `insufficient-space`, pero usa un stage real de nueve bytes.
`ScopedAvailableSpaceLimit` reemplaza temporalmente el import `GetDiskFreeSpaceExW` del ejecutable de prueba y
reporta cuatro bytes disponibles únicamente para el directorio exacto del fixture. Las demás consultas delegan
a la función original y el import se restaura antes de las aserciones.

No se modifica `ArtifactTransaction`, no se cambia el oráculo y no se simula el resultado de `commit`: la
implementación vigente sigue midiendo el archivo y ejecutando su comparación real. El cambio elimina la
posibilidad de copiar un sparse proporcional a la capacidad del volumen.

## Evidencia de cierre

- `TB-BAS-066`: aprobado de forma dirigida en Debug y Release en menos de 3 ms por modo.
- Gate `CB-BAS-012`: aprobado en Debug y Release.
- Suite nueva completa: 11/11 en Debug y 11/11 en Release, ejecutadas secuencialmente.
- Análisis `clang-tidy` del include, función y encabezado añadidos: 0 hallazgos propios.
- Stage y sibling proporcionales a la capacidad del volumen: 0.
- Código de producto modificado: 0 archivos.

La validación no ejecutó la batería histórica ni un baseline.

## HALLAZGO NO AUTORIZADO

**Evidencia:** el análisis completo del módulo anterior informa cuatro hallazgos heredados: un destructor que
puede asignar, dos operaciones bitwise que el check considera signed y dos parámetros de path intercambiables.

**Riesgo:** robustez y claridad del soporte de pruebas previo, sin relación causal con la copia densa de
`TB-BAS-066`.

**Componentes afectados:** `BasicArtifactControlledPreflightModule.cpp`.

**Posible solución:** separar los tipos históricos por archivo y corregirlos en una tanda de infraestructura
independiente.

**Estado:** `NO IMPLEMENTAR` dentro de esta corrección causal.
