# T08 H3: lector de tipos en la envoltura de dependencias

El lector versionado de tipos reconoce la envoltura de dependencias preparada
únicamente cuando el minor indicado es 46 o posterior. Decodifica y valida la
envoltura completa antes de devolver sus records y enums; para el minor 42 y
para los catálogos anteriores conserva las rutas existentes.

Se añadieron controles para recuperar un enum de la envoltura con minor 46 y
rechazar la misma entrada con minor 45. La construcción y ejecución se agruparán
con los cambios de verificación y publicación que siguen. El formato 1.46 no
se declara ni se emite todavía y esta modificación no habilita el consumo de las
funciones, globales o aliases transportados.
