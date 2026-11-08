# T08-003: objeto nativo inválido con huella válida

Se amplía el control de integración para alterar el encabezado COFF del primer
objeto de un sidecar v3 y recalcular su SHA-256. El consumidor debe rechazar ese
sidecar, conservar el otro, enlazar la biblioteca nativa y ejecutarse. El caso
distingue una huella coherente de un objeto realmente procesable. Su resultado
se registrará tras la ejecución.
