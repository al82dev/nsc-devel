# Tanda de Validacion 99: claves JSON unicas en configuracion

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`BuilderConfig.load` reabria `config.json` mediante la politica last-wins. Una
ruta o el paralelismo podian declararse dos veces y la segunda definicion se
aplicaba silenciosamente antes de validar y descubrir herramientas.

## Correccion

- la configuracion persistida se analiza con claves unicas;
- la violacion se rechaza antes de construir `BuilderConfig` y antes del
  descubrimiento;
- defaults y politica de descubrimiento permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas del esquema de configuracion: `5/5` PASS;
- suite Python completa del controlador: `233/233` PASS;
- dos definiciones de `jobs` son rechazadas antes del descubrimiento;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves al cargar la configuracion de
Builder 3. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
