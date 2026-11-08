# Productor con nombres C en firmas públicas

Se añade un productor independiente para distinguir el caso de alias y enum C
usados en firmas públicas del productor original, que no los menciona. La
prueba original se conserva. Aún falta compilar el productor y verificar qué
nombres y contratos persisten y pueden usarse en un consumidor sin encabezado.
