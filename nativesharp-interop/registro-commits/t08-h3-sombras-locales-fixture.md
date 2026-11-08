# T08-002: sombras locales en una plantilla publicada

El productor usa dos locales cuyos nombres también aparecen en una cabecera C,
como global externa y función `static inline`. La plantilla solo usa los
locales; el consumidor debe obtener 30 y el catálogo no debe incluir esas
declaraciones C como dependencias. El fixture se prepara sin cambiar producción.
