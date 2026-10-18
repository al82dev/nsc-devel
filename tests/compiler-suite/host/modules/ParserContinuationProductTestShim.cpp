#include "ParserFoundationTestSupport.h"

// Compile the shared continuation coordinator under the same test-only access view as the
// other physical Parser modules. Production compiles the source directly through nsc_frontend.
#include "../../../../src/Parser/ParserContinuationCoordinator.cpp"
