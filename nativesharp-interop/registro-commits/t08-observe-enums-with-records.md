# Tanda 08: enums durante la observación de records

La observación instala candidatos enum C después de los records, para que
aliases y usos de ambos puedan resolverse en la misma copia de sintaxis. El
selector de grupos ignora los candidatos enum posteriores sin confundir sus
índices con records. Se evita introducir duplicados donde ya existe una
identidad nativa o record; la resolución definitiva de colisiones sigue
pendiente. Este código y sus interacciones aún no se han validado.
