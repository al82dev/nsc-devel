# T08: muestra histórica NSMETA 1.48

Se conserva sin recodificar `tests/fixtures/nsmeta/historical-1.48.nsmeta`,
copiada del archivo `current-format.nsmeta` de la suite `Stage48_2` en
`A:/nsc-interop-build/tanda08-nsmeta-temp/nativesharp_stage48_2_library_metadata_tests/Integrity`.
SHA-256: `b2875c89de91f857bc0cba4bd599c0634d1fdf7cd4a77269f29d0448cffad665`.

La suite generó esa muestra con `codec.writeModel` usando el formato vigente de
su compilación; no se presenta como un artefacto de biblioteca producido por
el driver. Identifica `3.0.0+build.20261108T075734Z.220d44fcec18415c`:
01:57:34 hora local, antes del commit `67cd71016965affc95a351b146149ac7eea2aee4`
que introdujo 1.49 a las 03:01:07. La fecha no demuestra el commit exacto del
escritor. La muestra no fue recodificada con el lector actual.

El `nsc` de desarrollo build `f2be0335c46cb515` verificó formato 1.48, módulo
`Integrity`, destino `x86_64-pc-windows-msvc` y Object ABI 3. Falta la muestra
independiente 1.46 y ejecutar la suite completa; la tanda permanece abierta.
