# Validación de LLD sin `NSC_LINKER`

El fixture prepara el proyecto TLS estático y un productor NativeSharp
precompilado en A:. Elimina `NSC_LINKER` solo del proceso de prueba y ejecuta
`nsc build --linker LLD` con Clang y LLD en el PATH local del proceso. Exige
que el proyecto TLS se ejecute, que el resultado JSON reporte LLD y que se
genere la biblioteca estática. El control previo de conflictos con una
variable explícita sigue en `validar-seleccion.cmd` sin alteraciones.

Este fixture está preparado, pero todavía no se ha construido el compilador
con la propagación nueva ni se ha ejecutado. Faltan validación de DLL y
enlace incremental. No se modifica NSLink ni se escribe en C:.

La construcción incremental posterior terminó con código 0. La primera
versión del fixture pasó el ejecutable TLS y la biblioteca estática con NSC
build `f4df02e72f872c83`; también pasaron los controles anteriores de
selección y la matriz TLS con LLD O0–O3. Se amplió el fixture para comprobar
una DLL y un segundo enlace incremental. La versión ampliada pasó con NSC
build `b910fa13ea6d5855`, incluido el consumidor NativeSharp de la DLL y un
acierto del enlace incremental. El contraste adicional confirmó invalidación
al cambiar de NSLink a LLD, reutilización al repetir y error al faltar LLD.
El proyecto TLS también pasó O0–O3 sin la variable con el mismo build; la
biblioteca estática, la DLL consumida y el enlace incremental volvieron a
pasar en esa ejecución.
