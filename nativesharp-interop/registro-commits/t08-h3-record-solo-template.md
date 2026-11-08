# T08 H3: record C usado solo en una plantilla

Una función C devuelta como dependencia de una plantilla pública puede devolver un puntero a un record que no figura en la superficie NativeSharp del productor. Antes del cambio, el consumidor fallaba con `precompiled C function cannot be projected: C/C++ return type is not yet mapped`: la función estaba capturada, pero el catálogo no incluía el record de su firma.

El productor visita los tipos de retorno y parámetros de las funciones C capturadas para agregar sus records y enums al cierre existente del catálogo. La selección exige módulo, símbolo y USR coincidentes.

El nuevo fixture compila el productor y un consumidor sin encabezado y comprueba el campo del record. Pasó en O0–O3 dentro del conjunto agrupado de 24 casos (`A:/nsc-interop-build/tanda08-dependencias-template-record.log`). No demuestra el cierre de todas las dependencias C ni de la tanda 8.
