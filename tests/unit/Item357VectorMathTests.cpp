#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "../support/TestContext.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
namespace {
using nativesharp::ExitCode; using nativesharp::driver::Driver; using nativesharp::tests::TestContext;
void writeFile(const std::filesystem::path&p,std::string_view t){std::filesystem::create_directories(p.parent_path());std::ofstream s(p);s<<t;}
bool execute(const std::filesystem::path&p,const std::filesystem::path&e,std::string_view o,std::string&d){std::ostringstream out,err;const std::string ps=p.string(),es=e.string();const std::array<std::string_view,5>a={"--emit-project-oop-exe",ps,o,"-o",es};if(Driver{}.run(a,out,err)!=ExitCode::Success){d=err.str()+out.str();return false;}int r=std::system(("\""+es+"\"").c_str());d=r==42?std::string{}:"process result "+std::to_string(r);return r==42;}}
int main(){TestContext tests;const auto root=std::filesystem::path(NSC_BINARY_DIR)/"item357_vector_math";std::error_code x;std::filesystem::remove_all(root,x);std::filesystem::create_directories(root,x);const std::string core=(std::filesystem::path(NSC_SOURCE_DIR)/"lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();writeFile(root/"VectorMath.nsproj","<Project><PropertyGroup><Name>VectorMath</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\"/><ProjectReference Include=\""+core+"\"/></ItemGroup></Project>");writeFile(root/"Main.ns",R"NS(
using NativeSharp.Core.Numerics;
public class Program { public static int Main() {
 Float32x4 a; a.X=(float)1;a.Y=(float)2;a.Z=(float)3;a.W=(float)4;
 Float32x4 b; b.X=(float)4;b.Y=(float)3;b.Z=(float)2;b.W=(float)1;
 Float32x4 v; v.X=(float)-1;v.Y=(float)10;v.Z=(float)5;v.W=(float)2;
 Float32x4 lo; lo.X=(float)0;lo.Y=(float)0;lo.Z=(float)0;lo.W=(float)0;
 Float32x4 hi; hi.X=(float)4;hi.Y=(float)4;hi.Z=(float)4;hi.W=(float)4;
 Float32x3 x; x.X=(float)1;x.Y=(float)0;x.Z=(float)0;
 Float32x3 y; y.X=(float)0;y.Y=(float)1;y.Z=(float)0;
 Float32x4 clamped=Float32x4.Clamp(v,lo,hi); Float32x4 minimum=Float32x4.Min(a,b);
 Float32x4 maximum=Float32x4.Max(a,b); Float32x3 cross=Float32x3.Cross(x,y);
 return (int)Float32x4.Dot(a,b)+(int)(clamped.X+clamped.Y+clamped.Z+clamped.W)+
  (int)minimum.X+(int)maximum.X+(int)cross.Z+6;
} }
)NS");std::string d;tests.expect(execute(root/"VectorMath.nsproj",root/"VectorMathO0.exe","-O0",d),"vector math executes at O0 ("+d+")");d.clear();tests.expect(execute(root/"VectorMath.nsproj",root/"VectorMathO2.exe","-O2",d),"vector math executes at O2 ("+d+")");int r=tests.result();if(r==0)std::filesystem::remove_all(root,x);return r;}
