# Vida de vistas asignadas a salidas

Una asignación directa a un parámetro `ref` u `out` no puede almacenar una
vista de un propietario local ni de un propietario move-only consumido por
valor: ambos se destruyen al terminar la función. El análisis rechaza estas
asignaciones cuando puede identificar el origen y conserva la asignación de
una vista que recibe el propio llamador.

La prueba dirigida `nsc.sdk.shared-view-output-lifetime` analiza las familias
Rc y Arc con ambos tipos de salida. Comprueba ocho escapes —propietario local
o consumido— y cuatro controles válidos. Los controles se compilan y ejecutan
en O0 y O2 y deben devolver 42; los escapes solo se analizan. En la copia de
trabajo aislada, la prueba terminó correctamente. La reproducción previa de
asignación directa aceptaba los escapes antes del cambio.

La verificación usó el compilador aislado con otros cambios locales sin
registrar y fuentes del SDK de la copia de trabajo; no demuestra el resultado
de compilar este commit desde un árbol limpio. La regla solo cubre orígenes
identificables de asignaciones directas; el seguimiento entre llamadas,
orígenes desconocidos, campos y salidas compuestas se investiga por separado.
No se ejecutó una batería completa ni se desplegó el SDK.
