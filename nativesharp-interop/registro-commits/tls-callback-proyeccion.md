# Proyección de callback C TLS

El control C del callback pasó y NSC lo rechazó en la barrera TLS. Se permite
que un puntero a función alcance el mapeo existente de callbacks almacenados,
que conserva y valida su firma y convención. La construcción y las pruebas
estática y DLL esperan validación agrupada; no se afirma soporte de todas las
firmas o convenciones por el caso reproducido.
