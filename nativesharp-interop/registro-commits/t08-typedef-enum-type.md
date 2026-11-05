# Tanda 08: identidad de enum en typedef

`cppAliasEnumType` conserva nombre, base entera y punteros representables de un
alias a enum. Exige identidad y base conocidas y rechaza los calificadores que
la sintaxis actual no puede preservar. No establece equivalencia entre enums ni
resuelve el nombre por sí solo; queda pendiente de integración y validación.
