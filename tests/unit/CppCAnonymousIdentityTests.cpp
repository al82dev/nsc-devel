#include "nativesharp/Compilation/CompilationInputManifest.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "../../src/Driver/Internal/DriverInternal.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    std::ifstream input(argv[2]);
    std::map<std::string, std::string> expected;
    std::string line;
    while (std::getline(input, line))
    {
        const auto begin = line.find("c:@S@AnonOuter");
        if (begin == std::string::npos) continue;
        const auto end = line.find(' ', begin);
        const auto usr = line.substr(begin, end - begin);
        for (const auto* name : {"mode", "value", "AnonFirst", "AnonSecond"})
        {
            const std::string suffix = std::string("@") + name;
            if (usr.size() >= suffix.size() && usr.compare(usr.size() - suffix.size(), suffix.size(), suffix) == 0)
                expected[name] = usr;
        }
    }
    if (expected.size() != 4) { std::cerr << "Clang identity evidence is incomplete\n"; return 1; }
    const auto project = std::filesystem::absolute(argv[1]);
    nativesharp::compilation::ProjectLoadOptions options;
    nativesharp::compilation::ProjectGraphLoader loader(1, {}, options);
    std::string error;
    auto graph = loader.load(project, error);
    if (!graph) { std::cerr << error << '\n'; return 1; }
    nativesharp::compilation::CompilationInputManifest manifest(std::move(*graph), options);
    nativesharp::frontend::ProjectCompilation consumer;
    if (!nativesharp::driver::configurePrecompiledModules(manifest, consumer, error, false) ||
        !consumer.compileProjectMembers(project, error))
    { std::cerr << "consumer: " << error << '\n'; return 1; }
    const auto* members = consumer.memberModel();
    const auto* values = consumer.namespaceFunctionModel();
    if (!members || !values) return 1;
    bool ok = true;
    for (const auto* name : {"mode", "value"})
    {
        const auto all = members->members();
        const auto found = std::find_if(all.begin(), all.end(), [&](const auto& item) {
            return item.name == name && item.cppClangUsr && *item.cppClangUsr == expected[name];
        });
        if (found == all.end()) { std::cerr << "restored field identity missing: " << name << '\n'; ok = false; }
        else std::cout << "restored field " << name << ": " << *found->cppClangUsr << '\n';
    }
    for (const auto* name : {"AnonFirst", "AnonSecond"})
    {
        const auto all = values->states();
        const auto found = std::find_if(all.begin(), all.end(), [&](const auto& item) {
            return item.name == name && item.isCppEnumConstant && item.cppClangUsr && *item.cppClangUsr == expected[name];
        });
        if (found == all.end()) { std::cerr << "restored enumerator identity missing: " << name << '\n'; ok = false; }
        else std::cout << "restored enumerator " << name << ": " << *found->cppClangUsr << '\n';
    }
    return ok ? 0 : 1;
}
