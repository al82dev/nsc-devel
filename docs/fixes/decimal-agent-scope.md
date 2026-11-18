# Alcance del agente 2: decimal en almacenamiento contiguo

- Objetivo: confirmar y corregir los casos de arreglos fijos y stackalloc de decimal identificados en la campaña; conservar las reproducciones.
- Rama base: a56498f051939b8e19410212a5dd1585b1fef117 de development-fixes-limitaciones.
- Naturaleza: correctivo; preservar layout y comportamiento de otros tipos y modos de compilación.
- Posibles archivos: src/Frontend/MIR/Internal/MirNodeMachine.cpp, fixtures y pruebas dirigidas de decimal, su registro y configuración mínima de pruebas.
- Prohibido: alterar A:/nsc-devel-fixes, A:/nsc-devel, C:/native-sharp, fuentes LLVM/Clang, interoperabilidad C/C++ y Daikon; push, merge, rebase y despliegue.
- Builds, cachés, temporales y resultados aislados en A; solo compilación incremental y pruebas dirigidas.
- El árbol principal contiene cambios previos no registrados, no incluidos en la base; su procedencia se documentará al usarlos.
- Terminación: reproducción, corrección mínima, pruebas dirigidas con identidad del compilador y limitaciones, evidencia y commits coherentes. No presumir que un binario preexistente representa esta rama.
