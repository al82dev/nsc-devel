#include "ParserFoundationTestSupport.h"

// Parser's canonical plan contains cursor and lookahead contracts without a public seam. The
// support header exposes that seam only to this test target before compiling the product body,
// keeping the access model consistent across Parser definitions and their directed tests.
#include "../../../../src/Parser/Parser.cpp"
