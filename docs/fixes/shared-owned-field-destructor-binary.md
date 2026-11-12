# Campo propietario con destructor excepcional en Core binario

La observación `A:/nsc-fixes-agente-2-evidencia/owned-field-destructor-20261112/run-20261112T062140143977Z/results.json`
compiló y ejecutó 16 casos con Core binario aislado: Rc/Arc, payload clase o
struct, destructor normal o que propaga una excepción implícita, y O0/O2.
Todos devolvieron 42. La prueba exige un destructor del payload y uno del
campo `owned Resource` inicializado, además de excepción capturada cuando
corresponde, caducidad, un débil explícito y `Upgrade` vacío. La referencia
débil permanece viva durante la consulta; no se usa el payload destruido.
Los procesos arrancaron desde la copia aislada y usaron temporales en A; el
registro conserva comandos, salidas y hashes antes/después de compilador,
Core, metadata y runtime. El Core era preexistente y el árbol estaba en curso.

`tests/integration/shared_owned_field_destructor_binary.py` convierte la
observación en regresión autónoma: Core se configura mediante
`NSC_TEST_CORE_BINARY`, una ausencia de configuración devuelve 77 y una ruta
configurada inválida falla. Necesita temporales existentes en A y el runtime
aislado configurado por el lanzador. No ejecuta pruebas de interoperabilidad.
La ejecución de la regresión permanente pasó los 16 casos; sin Core devolvió
77 y con Core configurado inválido devolvió 1. Comandos y salidas:
`A:/nsc-fixes-agente-2-evidencia/owned-field-regression-config-20261112/run-20261112T062259614744Z/results.json`.
Esa regresión conserva hashes de compilador, Core y metadata antes/después.

Los resultados no prueban liberación física, múltiples campos, destructores
de campo que lanzan, doble excepción, descarga dinámica ni todas las rutas de
destrucción. No implican corrección general de las cuatro familias de punteros.
