#pragma once

#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include <span>
#include <string>

namespace nativesharp::linker
{
// EN: Freeze 3.0 (unresolved DLL imports): checks every `extern C from "<dll>"` symbol against the
//     export table of the DLL it names, when that DLL can be found in the system directories or on
//     PATH. A symbol the DLL does not export is a compile error; without the check the program links
//     and then fails to load (0xC0000139). A DLL that cannot be found here is not judged, because it
//     may ship next to the program. DEC-13: the driver calls this only under
//     --validate-system-imports; by default the build never reads the DLLs of the host.
// ES: Congelamiento 3.0 (imports de DLL sin resolver): comprueba cada simbolo de
//     `extern C from "<dll>"` contra la tabla de exports de la DLL que nombra, cuando esa DLL se
//     encuentra en los directorios del sistema o en PATH. Un simbolo que la DLL no exporta es un
//     error de compilacion; sin la comprobacion el programa enlaza y luego no carga (0xC0000139). Una
//     DLL que no se encuentra aqui no se juzga, porque puede distribuirse junto al programa. DEC-13:
//     el driver la llama solo con --validate-system-imports; por defecto la compilacion nunca lee las
//     DLL del host.
bool validateExternalImportExports(
    std::span<const ExternalImportModule> externalImports,
    std::string& error);
}
