# Tanda 08: colisiones de enum referenciado por campo

La observación conserva las identidades de enums que colisionan con tipos NS o
records. El selector las consulta también al incorporar enums indirectamente
por un campo, evitando instalar un tipo distinto con la misma identidad. La
prueba y validación agrupada siguen pendientes.
