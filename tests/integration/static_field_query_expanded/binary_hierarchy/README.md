# Jerarquía de clases binarias

El productor publica `Base` y `Derived` con sendos campos `Same`, un campo
protected y uno private. `External` espera únicamente los dos públicos, en
orden base a derivada, para producir 37. `Derived` consulta desde una clase
derivada y espera incluir además el protected, para producir 347. Los controles
ordinarios acceden mediante métodos o desde el ámbito derivado sin usar Fields.

Estas son expectativas propuestas para comprobar la implementación futura;
si el productor o un control ordinario falla, no se atribuye el resultado a
Fields. Los casos de certificado ausente o alterado requieren una sonda de
modelo separada y no representan por sí mismos metadata externa manipulada.
