# Control de carga dinámica en el gate

Se incorpora el control C que carga la DLL TLS de arranque mínimo mediante
`LoadLibraryA`. El control continúa fallando y el gate debe mostrarlo; no
equivale a una regresión demostrada de NSC. La evidencia detallada está en
`tls-dll-escalar-carga-dinamica.md`.
