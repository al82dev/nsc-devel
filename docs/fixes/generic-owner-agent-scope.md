# Alcance del agente 2: método genérico de propietario genérico

- Objetivo: investigar y corregir el caso 70-method del informe y su consumo
  binario, conservando la reproducción original y sin alterar préstamos.
- Base local inicial: fc06f5a38, descendiente de a56498f05; el árbol principal
  integró los cambios de decimal por commits propios después de esa base.
- Naturaleza: corrección de especialización y superficie binaria NativeSharp;
  preservar las reglas de identificación, restricciones, caché y seguridad.
- Material de partida: ensayo sin registrar en A:/nsc-devel-fixes, sujeto a
  revisión y traslado selectivo; no se modifica esa copia.
- Archivos candidatos: ProjectGenericModel, archivos de Generics y producción
  de superficie precompilada que participen en la reproducción, pruebas
  dirigidas existentes, su registro y configuración mínima de pruebas.
- Protegidos: propiedad/préstamos salvo dependencia demostrada y coordinada;
  C/C++ interop, Daikon y LLVM/Clang externos; A:/nsc-devel y C:/native-sharp.
- Builds, resultados, temporales y cachés en A y separados; compilación
  incremental y pruebas dirigidas únicamente. Sin push, merge ni despliegue.
- La evidencia distinguirá ejecutable híbrido y compilador integrado. La
  corrección solo se entrega tras validación dirigida y commits revisables.
