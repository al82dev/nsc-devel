# Tanda 08: identidad y base de tipos enum de Clang

`describeType` conserva la identidad del enum y, cuando Clang la conoce, su
representación entera. Un enum incompleto no recibe una base inventada. Esta
captura permite diferenciar un `typedef` a enum de un entero ordinario; aún no
demuestra su resolución ni su ABI y se validará al cerrar la tanda.
