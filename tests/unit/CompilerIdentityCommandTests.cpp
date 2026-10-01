#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

int main()
{
    std::ostringstream output;
    std::ostringstream error;
    const nativesharp::driver::Driver driver;
    const std::vector<std::string_view> arguments{"--print-compiler-identity"};
    const nativesharp::ExitCode code = driver.run(arguments, output, error);
    const std::string identity = output.str();
    const bool valid = code == nativesharp::ExitCode::Success &&
        identity.starts_with("NSCOMPILER 1\n") &&
        identity.find("compiler_version=3.0.0+build.") != std::string::npos &&
        identity.find("compiler_release=3.0.0\n") != std::string::npos &&
        identity.find("language=3.0.0\n") != std::string::npos &&
        identity.find("stage=70\n") != std::string::npos &&
        identity.find("build=") != std::string::npos &&
        identity.find("Copyright") == std::string::npos &&
        identity.find("Compilador") == std::string::npos &&
        error.str().empty();
    if (!valid)
    {
        std::cerr << "FAIL: machine compiler identity is not stable and complete\n"
                  << "stdout:\n" << identity << "stderr:\n" << error.str();
        return EXIT_FAILURE;
    }
    std::cout << "PASS: versioned machine compiler identity\n";
    return EXIT_SUCCESS;
}
