// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <compiler.h>

#include <solution.h>
#include <compiler_helpers.h>

#include <primitives/sw/settings.h>

#ifdef _WIN32
#include <misc/cmVSSetupHelper.h>
#endif

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler");

#define SW_MAKE_COMPILER_COMMAND(t)             \
    auto c = std::make_shared<t>(); \
    c->fs = fs

#define SW_MAKE_COMPILER_COMMAND_WITH_FILE(t) \
    SW_MAKE_COMPILER_COMMAND(driver::cpp::t)

static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");

#define CPP_EXTS ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC"

namespace sw
{

std::string getVsToolset(VisualStudioVersion v)
{
    switch (v)
    {
    case VisualStudioVersion::VS15:
        return "vc141";
    case VisualStudioVersion::VS14:
        return "vc14";
    case VisualStudioVersion::VS12:
        return "vc12";
    case VisualStudioVersion::VS11:
        return "vc11";
    case VisualStudioVersion::VS10:
        return "vc10";
    case VisualStudioVersion::VS9:
        return "vc9";
    case VisualStudioVersion::VS8:
        return "vc8";
    }
    throw std::runtime_error("Unknown VS version");
}

path getProgramFilesX86()
{
    auto e = getenv("programfiles(x86)");
    if (!e)
        throw std::runtime_error("Cannot get 'programfiles(x86)' env. var.");
    return e;
}

bool findDefaultVS2017(path &root, VisualStudioVersion &VSVersion)
{
    auto program_files_x86 = getProgramFilesX86();
    for (auto &edition : { "Enterprise", "Professional", "Community" })
    {
        path p = program_files_x86 / ("Microsoft Visual Studio/2017/"s + edition + "/VC/Auxiliary/Build/vcvarsall.bat");
        if (fs::exists(p))
        {
            root = p.parent_path().parent_path().parent_path();
            VSVersion = VisualStudioVersion::VS15;
            return true;
        }
    }
    return false;
}

StringSet listMajorWindowsKits()
{
    StringSet kits;
    auto program_files_x86 = getProgramFilesX86();
    for (auto &k : { "10", "8.1", "8.0", "7.1A", "7.0A", "6.0A" })
    {
        auto d = program_files_x86 / "Windows Kits" / k;
        if (fs::exists(d))
            kits.insert(k);
    }
    return kits;
}

StringSet listWindows10Kits()
{
    StringSet kits;
    auto program_files_x86 = getProgramFilesX86();
    auto dir = program_files_x86 / "Windows Kits" / "10" / "Include";
    for (auto &i : fs::directory_iterator(dir))
    {
        if (fs::is_directory(i))
            kits.insert(i.path().filename().string());
    }
    return kits;
}

StringSet listWindowsKits()
{
    auto allkits = listMajorWindowsKits();
    auto i = allkits.find("10");
    if (i == allkits.end())
        return allkits;
    auto kits2 = listWindows10Kits();
    allkits.insert(kits2.begin(), kits2.end());
    return allkits;
}

String getLatestWindowsKit()
{
    auto allkits = listMajorWindowsKits();
    auto i = allkits.find("10");
    if (i == allkits.end())
        return *allkits.rbegin();
    return *listWindows10Kits().rbegin();
}

path getWindowsKitDir()
{
    auto program_files_x86 = getProgramFilesX86();
    for (auto &k : { "10", "8.1", "8.0", "7.1A", "7.0A", "6.0A" })
    {
        auto d = program_files_x86 / "Windows Kits" / k;
        if (fs::exists(d))
            return d;
    }
    throw std::runtime_error("No Windows Kits available");
}

path getWindowsKit10Dir(Solution &s, const path &d)
{
    // take current or the latest version
    path last_dir = d / s.Settings.HostOS.Version.toString(true);
    if (fs::exists(last_dir))
        return last_dir;
    last_dir.clear();
    Version p;
    for (auto &i : fs::directory_iterator(d))
    {
        if (!fs::is_directory(i))
            continue;
        try
        {
            Version v(i.path().filename().u8string());
            if (v.isBranch())
                continue;
            if (v > p)
            {
                p = v;
                last_dir = i;
            }
        }
        catch (...)
        {
        }
    }
    if (last_dir.empty())
        throw std::runtime_error("No Windows Kits 10.0 available");
    return last_dir;
}

void detectNativeCompilers(struct Solution &s)
{
    //TODO: find preview versions also

    path root;
    Version V;
    auto VSVersion = VisualStudioVersion::Unspecified;

    auto find_comn_tools = [&root, &VSVersion](auto v)
    {
        auto n = std::to_string(v);
        auto ver = "VS"s + n + "COMNTOOLS";
        auto e = getenv(ver.c_str());
        if (e)
        {
            root = e;
            root /= "..\\..\\VC\\";
            VSVersion = v;
            return true;
        }
        return false;
    };

#if defined(_WIN32)
    cmVSSetupAPIHelper h;
    if (h.IsVS2017Installed())
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root /= "VC";
        VSVersion = VisualStudioVersion::VS15;

        // can be split by points
        static std::wregex r(L"(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::wsmatch m;
        if (!std::regex_match(h.chosenInstanceInfo.Version, m, r))
            throw std::runtime_error("Cannot match vs version regex");
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    else if (!find_comn_tools(VisualStudioVersion::VS15) && !findDefaultVS2017(root, VSVersion))
    {
        // find older versions
        static const auto vers =
        {
            VisualStudioVersion::VS14,
            VisualStudioVersion::VS12,
            VisualStudioVersion::VS11,
            VisualStudioVersion::VS10,
            VisualStudioVersion::VS9,
            VisualStudioVersion::VS8,
        };
        for (auto n : vers)
        {
            if (find_comn_tools(n))
                break;
        }
    }

    // we do not look for older compilers like vc7.1 and vc98
    if (VSVersion == VisualStudioVersion::Unspecified)
        return;

    if (VSVersion == VisualStudioVersion::VS15)
        root = root / "Tools\\MSVC" / boost::trim_copy(read_file(root / "Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt"));

    auto ToolSet = getVsToolset(VSVersion);
    auto compiler = root / "bin";
    NativeCompilerOptions COpts;
    COpts.System.IncludeDirectories.insert(root / "include");
    COpts.System.IncludeDirectories.insert(root / "ATLMFC\\include"); // also add

    struct DirSuffix
    {
        std::string host;
        std::string target;
    } dir_suffix;

    // get suffix
    switch (s.Settings.HostOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.host = "x64";
        break;
    case ArchType::x86:
        dir_suffix.host = "x86";
        break;
        // arm
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm";
        // arm64 !
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm64";
    default:
        assert(false && "Unknown arch");
    }

    switch (s.Settings.TargetOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.target = "x64";
        break;
    case ArchType::x86:
        dir_suffix.host = "x86";
        dir_suffix.target = "x86";
        break;
        // arm
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm";
        // arm64 !
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm64";
    default:
        assert(false && "Unknown arch");
    }

    NativeLinkerOptions LOpts;

    // continue
    if (VSVersion == VisualStudioVersion::VS15)
    {
        // always use host tools and host arch for building config files
        compiler /= "Host" + dir_suffix.host + "\\" + dir_suffix.target + "\\cl.exe";
        LOpts.System.LinkDirectories.insert(root / ("lib\\" + dir_suffix.target));
        LOpts.System.LinkDirectories.insert(root / ("ATLMFC\\lib\\" + dir_suffix.target)); // also add
    }
    else
    {
        // but we won't detect host&arch stuff on older versions
        compiler /= "cl.exe";
    }

    // add kits include dirs
    auto windows_kit_dir = getWindowsKitDir();
    for (auto &i : fs::directory_iterator(getWindowsKit10Dir(s, windows_kit_dir / "include")))
    {
        if (fs::is_directory(i))
            COpts.System.IncludeDirectories.insert(i);
    }
    for (auto &i : fs::directory_iterator(getWindowsKit10Dir(s, windows_kit_dir / "lib")))
    {
        if (fs::is_directory(i))
            LOpts.System.LinkDirectories.insert(i / path(dir_suffix.target));
    }

    // create programs

    {
        auto Linker = std::make_shared<VisualStudioLinker>();
        Linker->Type = LinkerType::MSVC;
        Linker->file = compiler.parent_path() / "link.exe";
        Linker->vs_version = VSVersion;
        if (s.Settings.TargetOS.Arch == ArchType::x86)
            Linker->Machine = vs::MachineType::X86;
        *Linker = LOpts;
        s.registerProgram("com.Microsoft.VisualStudio.VC.link", Linker);

        auto Librarian = std::make_shared<VisualStudioLibrarian>();
        Librarian->Type = LinkerType::MSVC;
        Librarian->file = compiler.parent_path() / "lib.exe";
        Librarian->vs_version = VSVersion;
        if (s.Settings.TargetOS.Arch == ArchType::x86)
            Librarian->Machine = vs::MachineType::X86;
        *Librarian = LOpts;
        s.registerProgram("com.Microsoft.VisualStudio.VC.lib", Librarian);
    }

    // ASM
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::ASM;
        L->CompiledExtensions = { ".asm" };
        //s.registerLanguage(L);

        //auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<VisualStudioASMCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = s.Settings.HostOS.Arch == ArchType::x86_64 ?
            (compiler.parent_path() / "ml64.exe") :
            (compiler.parent_path() / "ml.exe");
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.ml", C, L);
    }

    // C
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { ".c" };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<VisualStudioCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = compiler;
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.cl", C, L);
    }

    // C++
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { CPP_EXTS };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<VisualStudioCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = compiler;
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        C->CompileAsCPP = true;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.clpp", C, L);
    }

    // clang

    // create programs
    const path base_llvm_path = "c:\\Program Files\\LLVM";
    const path bin_llvm_path = base_llvm_path / "bin";

    /*auto Linker = std::make_shared<VisualStudioLinker>();
    Linker->Type = LinkerType::LLD;
    Linker->file = bin_llvm_path / "lld-link.exe";
    Linker->vs_version = VSVersion;
    *Linker = LOpts;

    auto Librarian = std::make_shared<VisualStudioLibrarian>();
    Librarian->Type = LinkerType::LLD;
    Librarian->file = bin_llvm_path / "llvm-ar.exe"; // ?
    Librarian->vs_version = VSVersion;
    *Librarian = LOpts;*/

    // C
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { ".c" };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangCompiler>();
        C->Type = CompilerType::Clang;
        C->file = bin_llvm_path / "clang.exe";
        auto COpts2 = COpts;
        COpts2.System.IncludeDirectories.erase(root / "include");
        COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include"); // also add
        COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        COpts2.System.CompileOptions.push_back("-Wno-everything");
        *C = COpts2;
        L->compiler = C;
        s.registerProgramAndLanguage("org.LLVM.clang", C, L);
    }

    // C++
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { CPP_EXTS };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangCompiler>();
        C->Type = CompilerType::Clang;
        C->file = bin_llvm_path / "clang++.exe";
        auto COpts2 = COpts;
        COpts2.System.IncludeDirectories.erase(root / "include");
        COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include"); // also add
        COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        COpts2.System.CompileOptions.push_back("-Wno-everything");
        *C = COpts2;
        L->compiler = C;
        s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);
    }

    // clang-cl

    // C
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { ".c", CPP_EXTS };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangClCompiler>();
        C->Type = CompilerType::ClangCl;
        C->file = bin_llvm_path / "clang-cl.exe";
        auto COpts2 = COpts;
        COpts2.System.IncludeDirectories.erase(root / "include");
        COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include"); // also add
        COpts2.System.IncludeDirectories.insert(bin_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        COpts2.System.CompileOptions.push_back("-Wno-everything");
        *C = COpts2;
        L->compiler = C;
        s.registerProgramAndLanguage("org.LLVM.clang_cl", C, L);
    }
#else
    // gnu

    path p;

    NativeLinkerOptions LOpts;
    LOpts.System.LinkDirectories.insert("/lib");
    LOpts.System.LinkDirectories.insert("/lib/x86_64-linux-gnu");
    LOpts.System.LinkLibraries.push_back("stdc++");
    LOpts.System.LinkLibraries.push_back("stdc++fs");
    LOpts.System.LinkLibraries.push_back("pthread");
    LOpts.System.LinkLibraries.push_back("dl");
    LOpts.System.LinkLibraries.push_back("m");

    auto resolve = [](const path &p)
    {
        if (do_not_resolve_compiler)
            return p;
        return primitives::resolve_executable(p);
    };

    p = resolve("ar");
    if (!p.empty())
    {
        auto Librarian = std::make_shared<GNULibrarian>();
        Librarian->Type = LinkerType::GNU;
        Librarian->file = p;
        *Librarian = LOpts;
        s.registerProgram("org.gnu.binutils.ar", Librarian);
    }

    //p = resolve("ld.gold");
    p = resolve("gcc");
    if (!p.empty())
    {
        auto Linker = std::make_shared<GNULinker>();

        Linker->Type = LinkerType::GNU;
        Linker->file = p;
        *Linker = LOpts;
        s.registerProgram("org.gnu.gcc.ld", Linker);
    }

    NativeCompilerOptions COpts;
    //COpts.System.IncludeDirectories.insert("/usr/include");
    //COpts.System.IncludeDirectories.insert("/usr/include/x86_64-linux-gnu");

    // ASM
    {
        p = resolve("as");

        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::ASM;
        L->CompiledExtensions = { ".s", ".S" };
        //s.registerLanguage(L);

        //auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<GNUASMCompiler>();
        C->Type = CompilerType::GNU;
        C->file = p;
        *C = COpts;
        L->compiler = C;
        s.registerProgramAndLanguage("org.gnu.gcc.as", C, L);
    }

    p = resolve("gcc");
    if (!p.empty())
    {
        // C
        {
            auto L = std::make_shared<NativeLanguage>();
            //L->Type = LanguageType::C;
            L->CompiledExtensions = { ".c" };
            //s.registerLanguage(L);

            //auto L = (CLanguage*)s.languages[LanguageType::C].get();
            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = p;
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.gnu.gcc.gcc", C, L);
        }
    }

    p = resolve("g++");
    if (!p.empty())
    {
        // CPP
        {
            auto L = std::make_shared<NativeLanguage>();
            //L->Type = LanguageType::C;
            L->CompiledExtensions = { CPP_EXTS };
            //s.registerLanguage(L);

            //auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = p;
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.gnu.gcc.gpp", C, L);
        }
    }

    // clang
    {
        //p = resolve("ld.gold");
        p = resolve("clang");
        if (!p.empty())
        {
            auto Linker = std::make_shared<GNULinker>();

            Linker->Type = LinkerType::GNU;
            Linker->file = p;
            *Linker = LOpts;
            s.registerProgram("org.LLVM.clang.ld", Linker);

            NativeCompilerOptions COpts;
            //COpts.System.IncludeDirectories.insert("/usr/include");
            //COpts.System.IncludeDirectories.insert("/usr/include/x86_64-linux-gnu");

            // C
            {
                auto L = std::make_shared<NativeLanguage>();
                //L->Type = LanguageType::C;
                L->CompiledExtensions = { ".c" };
                //s.registerLanguage(L);

                //auto L = (CLanguage*)s.languages[LanguageType::C].get();
                auto C = std::make_shared<GNUCompiler>();
                C->Type = CompilerType::Clang;
                C->file = p;
                *C = COpts;
                L->compiler = C;
                s.registerProgramAndLanguage("org.LLVM.clang", C, L);
            }
        }

        p = resolve("clang++");
        if (!p.empty())
        {
            // CPP
            {
                auto L = std::make_shared<NativeLanguage>();
                //L->Type = LanguageType::C;
                L->CompiledExtensions = { CPP_EXTS };
                //s.registerLanguage(L);

                //auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
                auto C = std::make_shared<GNUCompiler>();
                C->Type = CompilerType::Clang;
                C->file = p;
                *C = COpts;
                L->compiler = C;
                s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);
            }
        }
    }
#endif
}

ToolBase::~ToolBase()
{
    //delete cmd;
}

Version VisualStudio::gatherVersion(const path &program) const
{
    Version V;
    primitives::Command c;
    c.program = program;
    c.args = { "--version" };
    std::error_code ec;
    c.execute(ec);
    // ms returns exit code = 2 on --version
    if (ec)
    {
        static std::regex r("(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::smatch m;
        if (std::regex_search(c.err.text.empty() ? c.out.text : c.err.text, m, r))
        {
            if (m[5].matched)
                V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
            else
                V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
        }
    }
    return V;
}

std::shared_ptr<builder::Command> VisualStudioASMCompiler::getCommand() const
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(VSCommand);

    if (file.filename() == "ml64.exe")
        ((VisualStudioASMCompiler*)this)->SafeSEH = false;

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<VisualStudioAssemblerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

std::shared_ptr<Program> VisualStudioASMCompiler::clone() const
{
    return std::make_shared<VisualStudioASMCompiler>(*this);
}

void VisualStudioASMCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

Files VisualStudioASMCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(ObjectFile().parent_path());
    return f;
}

void VisualStudioASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

std::shared_ptr<builder::Command> VisualStudioCompiler::getCommand() const
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(VSCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (CSourceFile)
    {
        c->name = normalize_path(CSourceFile());
        c->name_short = CSourceFile().filename().u8string();
        //c->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        c->name = normalize_path(CPPSourceFile());
        c->name_short = CPPSourceFile().filename().u8string();
        //c->file = CPPSourceFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<VisualStudioCompilerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    if (PreprocessToFile)
    {
        //c->addOutput(c->file.file.parent_path() / (c->file.file.filename().stem().u8string() + ".i"));
        // TODO: remove old object file, it's now incorrect
    }

    return cmd = c;
}

void VisualStudioCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

Files VisualStudioCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(ObjectFile().parent_path());
    return f;
}

std::shared_ptr<Program> VisualStudioCompiler::clone() const
{
    return std::make_shared<VisualStudioCompiler>(*this);
}

void VisualStudioCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    VisualStudioCompiler::setOutputFile(output_file);
}

Version Clang::gatherVersion(const path &program) const
{
    Version v;
    primitives::Command c;
    c.program = program;
    c.args = {"-v"};
    std::error_code ec;
    c.execute(ec);
    if (!ec)
    {
        static std::regex r("^clang version (\\d+).(\\d+).(\\d+)");
        std::smatch m;
        if (std::regex_search(c.err.text, m, r))
            v = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    return v;
}

std::shared_ptr<builder::Command> ClangCompiler::getCommand() const
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(GNUCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (OutputFile)
    {
        c->deps_file = OutputFile().parent_path() / (OutputFile().stem().u8string() + ".d");
        c->working_directory = OutputFile().parent_path();
    }

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<ClangOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

void ClangCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

Files ClangCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(OutputFile().parent_path());
    return f;
}

std::shared_ptr<Program> ClangCompiler::clone() const
{
    return std::make_shared<ClangCompiler>(*this);
}

void ClangCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}


std::shared_ptr<builder::Command> ClangClCompiler::getCommand() const
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(VSCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (CSourceFile)
    {
        c->name = normalize_path(CSourceFile());
        c->name_short = CSourceFile().filename().u8string();
        //c->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        c->name = normalize_path(CPPSourceFile());
        c->name_short = CPPSourceFile().filename().u8string();
        //c->file = CPPSourceFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<VisualStudioCompilerOptions>(c.get(), *this);
    getCommandLineOptions<ClangClOptions>(c.get(), *this, "-Xclang");
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

void ClangClCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

Files ClangClCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(ObjectFile().parent_path());
    return f;
}

std::shared_ptr<Program> ClangClCompiler::clone() const
{
    return std::make_shared<ClangClCompiler>(*this);
}

void ClangClCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

Version GNU::gatherVersion(const path &program) const
{
    Version v;
    primitives::Command c;
    c.program = program;
    c.args = { "-v" };
    std::error_code ec;
    c.execute(ec);
    if (!ec)
    {
        static std::regex r("(\\d+).(\\d+).(\\d+)");
        std::smatch m;
        if (std::regex_search(c.err.text, m, r))
            v = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    return v;
}

std::shared_ptr<builder::Command> GNUASMCompiler::getCommand() const
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(GNUCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (OutputFile)
        c->working_directory = OutputFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<GNUAssemblerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

std::shared_ptr<Program> GNUASMCompiler::clone() const
{
    return std::make_shared<GNUASMCompiler>(*this);
}

void GNUASMCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

Files GNUASMCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(OutputFile().parent_path());
    return f;
}

void GNUASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

std::shared_ptr<Program> ClangASMCompiler::clone() const
{
    return std::make_shared<ClangASMCompiler>(*this);
}

std::shared_ptr<builder::Command> GNUCompiler::getCommand() const
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(GNUCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (OutputFile)
    {
        c->deps_file = OutputFile().parent_path() / (OutputFile().stem().u8string() + ".d");
        c->working_directory = OutputFile().parent_path();
    }

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<GNUOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    getCommandLineOptions<GNUOptions>(c.get(), *this, "", true);

    return cmd = c;
}

void GNUCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

Files GNUCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(OutputFile().parent_path());
    return f;
}

std::shared_ptr<Program> GNUCompiler::clone() const
{
    return std::make_shared<GNUCompiler>(*this);
}

void GNUCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

FilesOrdered NativeLinker::gatherLinkDirectories() const
{
    FilesOrdered dirs;
    iterate([&dirs](auto &v, auto &gs)
    {
        auto get_ldir = [&dirs](const auto &a)
        {
            for (auto &d : a)
                dirs.push_back(d);
        };

        get_ldir(v.System.gatherLinkDirectories());
        get_ldir(v.gatherLinkDirectories());
    });
    return dirs;
}

FilesOrdered NativeLinker::gatherLinkLibraries() const
{
    FilesOrdered dirs;
    iterate([&dirs](auto &v, auto &gs)
    {
        auto get_ldir = [&dirs](const auto &a)
        {
            for (auto &d : a)
                dirs.push_back(d);
        };

        get_ldir(v.System.gatherLinkLibraries());
        get_ldir(v.gatherLinkLibraries());
    });
    return dirs;
}

void VisualStudioLibraryTool::setObjectFiles(const Files &files)
{
    if (!files.empty())
        InputFiles().insert(files.begin(), files.end());
}

void VisualStudioLibraryTool::setOutputFile(const path &out)
{
    Output = out.u8string() + Extension;
}

void VisualStudioLibraryTool::setImportLibrary(const path &out)
{
    ImportLibrary = out.u8string() + ".lib";
}

path VisualStudioLibraryTool::getOutputFile() const
{
    return Output;
}

path VisualStudioLibraryTool::getImportLibrary() const
{
    if (ImportLibrary)
        return ImportLibrary();
    path p = Output;
    return p.parent_path() / (p.filename().stem() += ".lib");
}

std::shared_ptr<builder::Command> VisualStudioLibraryTool::getCommand() const
{
    if (cmd)
        return cmd;

    if (InputFiles.empty() && DefinitionFile.empty())
        return nullptr;

    if (Output.empty())
        throw std::runtime_error("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    //c->out.capture = true;
    c->base = clone();
    if (Output)
    {
        c->working_directory = Output().parent_path();
        c->name = normalize_path(Output());
        c->name_short = Output().filename().u8string();
    }

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    getAdditionalOptions(c.get());

    return cmd = c;
}

VisualStudioLinker::VisualStudioLinker()
{
    Extension = ".exe";
}

std::shared_ptr<Program> VisualStudioLinker::clone() const
{
    return std::make_shared<VisualStudioLinker>(*this);
}

void VisualStudioLinker::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<VisualStudioLinkerOptions>(c, *this);
}

void VisualStudioLinker::setInputLibraryDependencies(const FilesOrdered &files)
{
    if (!files.empty())
        InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
}

VisualStudioLibrarian::VisualStudioLibrarian()
{
    Extension = ".lib";
}

std::shared_ptr<Program> VisualStudioLibrarian::clone() const
{
    return std::make_shared<VisualStudioLibrarian>(*this);
}

void VisualStudioLibrarian::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(c, *this);
}

GNULinker::GNULinker()
{
    //Extension = ".exe";
}

std::shared_ptr<Program> GNULinker::clone() const
{
    return std::make_shared<GNULinker>(*this);
}

void GNULinker::setObjectFiles(const Files &files)
{
    if (!files.empty())
        InputFiles().insert(files.begin(), files.end());
}

void GNULinker::setOutputFile(const path &out)
{
    Output = out.u8string() + Extension;
}

void GNULinker::setImportLibrary(const path &out)
{
    //ImportLibrary = out.u8string();// + ".lib";
}

void GNULinker::setLinkLibraries(const FilesOrdered &in)
{
    for (auto &lib : in)
        NativeLinker::LinkLibraries.push_back(lib);
}

void GNULinker::setInputLibraryDependencies(const FilesOrdered &files)
{
    if (files.empty())
		return;
    // TODO: fast fix for GNU
    // https://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking
    InputLibraryDependencies().push_back("-Wl,--start-group");
    InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
    InputLibraryDependencies().push_back("-Wl,--end-group");
}

path GNULinker::getOutputFile() const
{
    return Output;
}

path GNULinker::getImportLibrary() const
{
    //if (ImportLibrary)
        //return ImportLibrary();
    //path p = Output;
    //return p.parent_path() / (p.filename().stem() += ".a");
    return Output;
}

void GNULinker::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<GNULinkerOptions>(c, *this);
}

std::shared_ptr<builder::Command> GNULinker::getCommand() const
{
    if (cmd)
        return cmd;

    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
        return nullptr;

    if (Output.empty())
        throw std::runtime_error("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    ((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    //c->out.capture = true;
    c->base = clone();
    if (Output)
    {
        c->working_directory = Output().parent_path();
        c->name = normalize_path(Output());
        c->name_short = Output().filename().u8string();
    }

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULinkerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    //getAdditionalOptions(c.get());

    return cmd = c;
}

GNULibrarian::GNULibrarian()
{
    Extension = ".a";
}

std::shared_ptr<Program> GNULibrarian::clone() const
{
    return std::make_shared<GNULibrarian>(*this);
}

void GNULibrarian::setObjectFiles(const Files &files)
{
    if (!files.empty())
        InputFiles().insert(files.begin(), files.end());
}

void GNULibrarian::setOutputFile(const path &out)
{
    Output = out.u8string() + Extension;
}

void GNULibrarian::setImportLibrary(const path &out)
{
    //ImportLibrary = out.u8string();// + ".lib";
}

path GNULibrarian::getOutputFile() const
{
    return Output;
}

path GNULibrarian::getImportLibrary() const
{
    //if (ImportLibrary)
        //return ImportLibrary();
    path p = Output;
    return p.parent_path() / (p.filename().stem() += ".a");
}

void GNULibrarian::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<GNULibrarianOptions>(c, *this);
}

std::shared_ptr<builder::Command> GNULibrarian::getCommand() const
{
    if (cmd)
        return cmd;

    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
        return nullptr;

    if (Output.empty())
        throw std::runtime_error("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    //c->out.capture = true;
    c->base = clone();
    if (Output)
    {
        c->working_directory = Output().parent_path();
        c->name = normalize_path(Output());
        c->name_short = Output().filename().u8string();
    }

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULibrarianOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    //getAdditionalOptions(c.get());

    return cmd = c;
}

}
