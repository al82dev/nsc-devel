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
