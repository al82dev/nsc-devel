# SEM-06.07 — Probe reproducible de escalamiento semántico

## Motivo

La medición prevista de `NativeSharp.Math.Functions` no alcanzó `method-bodies`: los binarios O0
existentes en `A:\nativesharp-libraries\build\O0` tienen un fingerprint ABI anterior al compilador
de testing. Recompilar librerías o consumir `C:\native-sharp` está fuera del alcance autorizado.

Para no ocultar el hueco ni bloquear la medición del scheduler se añadió un probe independiente,
no registrado en CTest. Genera 1024 callables independientes con flujo, loops y expresiones;
ejecuta el Driver en O0 con `j1`, `j2` y `j12` y exige resúmenes semánticos
idénticos. Cada configuración ejecuta una pasada de calentamiento descartada y tres muestras. El
orden rota entre muestras para que `j1`, `j2` y `j12` ocupen cada posición una vez; se publica la
mediana para reducir ruido transitorio sin esconder cada criterio semántico.

## Medición

El ejecutable informa por configuración:

- tiempo de pared total del Driver;
- microsegundos de la fase productiva `method-bodies` obtenidos mediante `--time`;
- igualdad del resumen semántico.

También separa `wave_analysis_us` y `wave_merge_us`, de modo que una mejora de workers no pueda
quedar escondida detrás de la publicación determinista.

No contiene umbrales dependientes de una máquina ni convierte una regresión de rendimiento en una
aprobación falsa. El objetivo es conservar un corpus fijo y una medición reproducible que permita
comparar revisiones y presupuestos de jobs.

## Archivos

- `tests/performance/CallableWavePerformanceProbe.cpp`
- `CMakeLists.txt`

## Ejecución dirigida

```powershell
cmake --build A:\nsc-devel\build-test-nslink `
  --target nsc_callable_wave_performance_probe --parallel 12
$env:TEMP = 'A:\nsc-devel\_nscbuilder\semantic-performance\tmp'
$env:TMP = $env:TEMP
A:\nsc-devel\build-test-nslink\tests\nsc_callable_wave_performance_probe.exe
```

El bloqueo de `Math.Functions` queda conservado en
`_nscbuilder/semantic-performance/SEM-06.07/math-functions-j12.log`. No se ejecutan baseline,
minibaseline, CTest general, linker ni O1/O2/O3.

## Resultado posterior al cierre

Corpus de 1024 callables, O0, una pasada de calentamiento, tres rondas rotativas y mediana:

| Jobs | Pared | Method bodies | Analysis | Merge |
|---:|---:|---:|---:|---:|
| 1 | 115 ms | 45,179 us | 26,695 us | 13,133 us |
| 2 | 107 ms | 37,811 us | 17,264 us | 15,127 us |
| 12 | 105 ms | 24,528 us | 5,748 us | 12,639 us |

El análisis de la oleada mejora 78.5 % entre `j1` y `j12`; `method-bodies` mejora 45.7 % y la
pared completa del Driver mejora 8.7 %. La evidencia se conserva en
`_nscbuilder/semantic-performance/SEM-06.07/callable-wave-performance.log`.

El objetivo compila con warnings como errores y clang-tidy dirigido queda limpio con checks de
analyzer, bugprone y performance.
