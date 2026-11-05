# Tanda 08: proyección inicial de global C enum

- El proveedor del driver consulta el índice de tipos ya construido. Para un global enum, la proyección reutiliza la verificación de identidad, propietario, representación y layout de `projectCppSemanticEnumType`; la ruta previa sin índice se conserva para sus consumidores.
- Se mantienen la selección de nombres y conflictos, la propiedad del símbolo, los calificadores admitidos y el rechazo de TLS. Los enums que no aparezcan en el índice siguen sin proyectarse.
- Pendiente: seleccionar el enum cuando solo se menciona por el global y validar en conjunto el proyecto mixto preparado. Este cambio no cierra T08-004 ni T08-009.
