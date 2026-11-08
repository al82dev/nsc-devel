# Validación de agregados anónimos C en dos sesiones

Tras construir incrementalmente `nsc` de `A:/nsc-interop-build/tanda1-ON`, el
fixture `pruebas-tanda-08-anonimos-c/validar.cmd` terminó con código 0 en O0–O3.
Incluyó el proyecto con dos sesiones que observan el mismo encabezado, además de
consumo directo, Shared y Metadata; comprobó tamaños y accesos cruzados con C.

La prueba no demuestra equivalencia de archivos distintos ni de declaraciones
incompatibles; el cambio exige coincidencia de procedencia y contrato. La validación
completa y los fixtures históricos de metadata siguen pendientes en la tanda 8.
