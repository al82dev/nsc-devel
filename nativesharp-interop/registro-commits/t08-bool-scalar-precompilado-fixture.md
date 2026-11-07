# T08: global C bool volatile en productor precompilado

El fixture prepara un productor NativeSharp que lee, escribe y modifica con
`^=` un global C `volatile _Bool`; un consumidor independiente usa la biblioteca
en O0–O3 mientras el header C está oculto. Funciones C observan y modifican el
mismo global. Antes se comprueban carga acquire y escritura release de byte en
el IR del productor. La copia temporal del header se restaura tras compilar el
consumidor, incluso cuando esa compilación devuelve error.

El caso se incorpora al gate de la tanda, pero todavía no se ha construido ni
ejecutado. No acredita DLL, caché activa, otros tipos ni cierre de T08. No se
modificó NSLink.
