# Validación histórica tras NSMETA 1.48

En `A:/nsc-interop-build/tanda1-ON` se construyeron incrementalmente los objetivos
de tolerancia de versión y de metadata Stage 48.2. La prueba de tolerancia pasó
17 comprobaciones. Stage 48.2 terminó con 6 fallos de 439: no pudo abrir los
fixtures archivados `historical-1.45.nsmeta`, `historical-1.46.nsmeta` y
`historical-1.47.nsmeta`; cada ausencia afecta la lectura y la compatibilidad ABI.
No se sustituyeron por archivos generados actuales.

La primera ejecución de Stage 48.2 usó por error el temporal predeterminado en
`C:/Users/al82dev/AppData/Local/Temp/nativesharp_stage48_2_library_metadata_tests`,
fuera de la restricción de trabajo en A:. La repetición usó TEMP y TMP en
`A:/nsc-interop-build/tanda08-nsmeta-temp` y obtuvo los mismos seis fallos.
La compatibilidad histórica real permanece sin demostrar y la tanda 8 sigue abierta.
