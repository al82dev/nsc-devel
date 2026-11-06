# T08-010 / T08-012: record C anidado subalineado

El proyector admite un campo record C subalineado cuando su tipo interior ya
es completo y coincide en identidad, tamaño y alineación natural con la
declaración importada. El layout conserva la alineación efectiva exterior y
el emisor la propaga hacia los campos interiores. Siguen pendientes la prueba
mixta y las demás formas restringidas de records empaquetados.
