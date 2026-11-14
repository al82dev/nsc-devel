#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) return 2;
    std::string error;
    nativesharp::runtime::WindowsRuntimeBuilder builder;
    builder.setMainArguments(true);
    auto module = argc == 3 ? builder.buildSupport(error) : builder.buildEntry("test_main", error);
    if (!module || !module->verify(error)) { std::cerr << error; return 1; }
    if (argc == 3)
    {
        std::ofstream output(argv[1], std::ios::binary);
        output << module->text();
        return output ? 0 : 1;
    }
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(error);
    if (!target || !target->emit(*module, argv[1],
            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, error))
    { std::cerr << error; return 1; }
    return 0;
}
