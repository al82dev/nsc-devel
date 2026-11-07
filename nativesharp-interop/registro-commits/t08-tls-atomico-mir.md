# Transporte atómico en MIR y cachés

El indicador preparado para HIR se copia a MIR y se incluye en las identidades
HIR y de código, así como en el payload MIR. La clave de caché MIR cambia a v6
para no leer un formato anterior como si llevara el nuevo byte. No se proyectan
todavía globales atómicos ni se habilitan accesos; falta generación y pruebas.
