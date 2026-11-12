# Fabrica compartida throwing: helper binario con receptor

La prueba dirigida en `A:/nsc-fixes-auditoria-evidencia/throwing_helper_emission_probe.py` comprueba clases creadas con `MakeRcThrowing` y `MakeArcThrowing`. Los constructores declaran `throws Exception`; los programas generados no se ejecutaron.

En `A:/nsc-fixes-auditoria-evidencia/throwing-helper-20261112T053053630506Z/results.json`, los ocho casos superaron el analisis semantico. Para ambas familias, el helper fuente inocuo y el helper binario sin receptor emitieron en O0; el helper binario publicador y el helper binario inocuo que reciben el receptor se rechazaron con `shared payload receiver passed to a callable with unverifiable body`. El segundo rechazo es conservador. Los hashes registrados del compilador, Core, metadata y runtime coinciden antes y despues.

El primer intento, `A:/nsc-fixes-auditoria-evidencia/throwing-helper-20261112T052931752032Z/results.json`, se bloqueo al generar el productor porque no estaba configurado el runtime aislado y se intento abrir un runtime predeterminado inexistente en C. No se llego a los casos. El fixture se ajusto para indicar explicitamente el runtime y directorio de importaciones en A; el segundo intento es evidencia separada.

La prueba cubre emision O0 para payload de clase y paso directo del receptor como argumento. El constructor declara que puede lanzar, pero el ejemplo no lanza: no verifica propagacion ni limpieza por excepcion. Tampoco cubre estructuras, destructor, llamadas con `this` como receptor, despacho virtual o por interfaz, ni retornos opacos. El compilador y el SDK corresponden a un arbol con cambios locales; no se acredita una revision limpia ni una garantia general.
