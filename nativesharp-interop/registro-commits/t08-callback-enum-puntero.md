# Tanda 08: puntero a callback con firma enum

- Se amplía la prueba semántica existente con `ColorCallback*`, además del alias sin puntero adicional.
- La comprobación exige preservar el enum en resultado y parámetro, la convención C y el modificador de puntero; ejercita la ruta de tipos compuestos de la expansión, distinta de la ruta escalar.
- Estado: preparada para la validación agrupada. No demuestra todavía una llamada ejecutable a un callback de enum ni la cobertura completa de T08-017.
