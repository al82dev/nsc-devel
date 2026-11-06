# Identidad de código para volatile importado

T08-015: el emisor distingue la opción MS volatile y la alineación acreditada,
pero `writeNodeFlags` no incluía ninguna en la identidad de código por función.
La identidad ahora conserva ambos opcionales y cambia la versión de definición
a v2, evitando reutilizar entradas antiguas con el contrato incompleto.
La prueba y la validación agrupada se registran por separado.
