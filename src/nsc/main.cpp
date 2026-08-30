#include "nativesharp/Driver/Driver.h"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);

    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    const nativesharp::driver::Driver driver;
    const nativesharp::ExitCode exitCode = driver.run(arguments, std::cout, std::cerr);
    return static_cast<int>(exitCode);
}
