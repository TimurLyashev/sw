// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "program.h"
#include "options.h"
#include "options_cl.h"
#include "options_cl_clang.h"
#include "options_cl_vs.h"
#include "types.h"
#include "cppan_version.h"

#include <memory>

namespace sw
{

namespace builder
{
struct Command;
}

namespace driver::cpp
{
struct Command;
}

struct Solution;

enum VisualStudioVersion
{
    Unspecified,

    //VS7 = 71,
    VS8 = 80,
    VS9 = 90,
    VS10 = 100,
    VS11 = 110,
    VS12 = 120,
    //VS13 = 130 was skipped
    VS14 = 140,
    VS15 = 150,
};

SW_DRIVER_CPP_API
void detectNativeCompilers(struct Solution &s);

struct SW_DRIVER_CPP_API ToolBase
{
    ~ToolBase();

protected:
    mutable std::shared_ptr<driver::cpp::Command> cmd;
};

struct SW_DRIVER_CPP_API CompilerToolBase : ToolBase
{
    virtual ~CompilerToolBase() = default;

    // consider as static
    //virtual bool findToolchain(struct Solution &s) const = 0;

protected:
    virtual Version gatherVersion(const path &program) const = 0;
};

struct SW_DRIVER_CPP_API Compiler : Program
{
    virtual ~Compiler() = default;

    virtual String getObjectExtension() const = 0;
};

struct SW_DRIVER_CPP_API NativeCompiler : Compiler,
    NativeCompilerOptions//, OptionsGroup<NativeCompilerOptions>
{
    CompilerType Type = CompilerType::UnspecifiedCompiler;

    virtual ~NativeCompiler() = default;

    virtual void setSourceFile(const path &input_file, path &output_file) = 0;
    virtual String getObjectExtension() const { return ".o"; }
    virtual Files getGeneratedDirs() const = 0;

protected:
    mutable Files dependencies;
};

struct SW_DRIVER_CPP_API VisualStudio : CompilerToolBase
{
    VisualStudioVersion vs_version = VisualStudioVersion::Unspecified;
    String toolset;

    virtual ~VisualStudio() = default;

protected:
    Version gatherVersion(const path &program) const override;
};

struct SW_DRIVER_CPP_API VisualStudioCompiler : VisualStudio,
    NativeCompiler,
    CommandLineOptions<VisualStudioCompilerOptions>
{
    virtual ~VisualStudioCompiler() = default;

    using NativeCompilerOptions::operator=;

    std::shared_ptr<Program> clone() const override;
    std::shared_ptr<builder::Command> getCommand() const override;
    void setOutputFile(const path &output_file);
    String getObjectExtension() const override { return ".obj"; }
    Files getGeneratedDirs() const override;
    void setSourceFile(const path &input_file, path &output_file) override;

protected:
    Version gatherVersion() const override { return VisualStudio::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API VisualStudioASMCompiler : VisualStudio, NativeCompiler,
    CommandLineOptions<VisualStudioAssemblerOptions>
{
    virtual ~VisualStudioASMCompiler() = default;

    using NativeCompilerOptions::operator=;

    std::shared_ptr<Program> clone() const override;
    std::shared_ptr<builder::Command> getCommand() const override;
    void setSourceFile(const path &input_file, path &output_file) override;
    void setOutputFile(const path &output_file);
    String getObjectExtension() const override { return ".obj"; }
    Files getGeneratedDirs() const override;

protected:
    Version gatherVersion() const override { return VisualStudio::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API Clang : CompilerToolBase
{
    //bool findToolchain(struct Solution &s) const override;

protected:
    Version gatherVersion(const path &program) const override;
};

struct SW_DRIVER_CPP_API ClangCompiler : Clang, NativeCompiler,
    CommandLineOptions<ClangOptions>
{
    using NativeCompilerOptions::operator=;

    std::shared_ptr<Program> clone() const override;
    std::shared_ptr<builder::Command> getCommand() const override;
    void setOutputFile(const path &output_file);
    String getObjectExtension() const override { return ".obj"; }
    Files getGeneratedDirs() const override;
    void setSourceFile(const path &input_file, path &output_file) override;

protected:
    Version gatherVersion() const override { return Clang::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API ClangCl : Clang
{
};

struct SW_DRIVER_CPP_API ClangClCompiler : ClangCl,
    NativeCompiler,
    CommandLineOptions<VisualStudioCompilerOptions>,
    CommandLineOptions<ClangClOptions>
{
    using NativeCompilerOptions::operator=;

    std::shared_ptr<Program> clone() const override;
    std::shared_ptr<builder::Command> getCommand() const override;
    void setOutputFile(const path &output_file);
    String getObjectExtension() const override { return ".obj"; }
    Files getGeneratedDirs() const override;
    void setSourceFile(const path &input_file, path &output_file) override;

protected:
    Version gatherVersion() const override { return Clang::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API GNU : CompilerToolBase
{
protected:
    Version gatherVersion(const path &program) const override;
};

struct SW_DRIVER_CPP_API GNUASMCompiler : GNU, NativeCompiler,
    CommandLineOptions<GNUAssemblerOptions>
{
    virtual ~GNUASMCompiler() = default;

    using NativeCompilerOptions::operator=;

    std::shared_ptr<Program> clone() const override;
    std::shared_ptr<builder::Command> getCommand() const override;
    void setSourceFile(const path &input_file, path &output_file) override;
    void setOutputFile(const path &output_file);
    String getObjectExtension() const override { return ".o"; }
    Files getGeneratedDirs() const override;

protected:
    Version gatherVersion() const override { return GNU::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API ClangASMCompiler : GNUASMCompiler
{
    std::shared_ptr<Program> clone() const override;
};

struct SW_DRIVER_CPP_API GNUCompiler : GNU, NativeCompiler,
    CommandLineOptions<GNUOptions>
{
    using NativeCompilerOptions::operator=;

    std::shared_ptr<Program> clone() const override;
    std::shared_ptr<builder::Command> getCommand() const override;
    void setOutputFile(const path &output_file);
    String getObjectExtension() const override { return ".o"; }
    Files getGeneratedDirs() const override;
    void setSourceFile(const path &input_file, path &output_file) override;

protected:
    Version gatherVersion() const override { return GNU::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API Linker : Program
{
    virtual ~Linker() = default;
};

struct SW_DRIVER_CPP_API NativeLinker : Linker,
    NativeLinkerOptions//, OptionsGroup<NativeLinkerOptions>
{
    LinkerType Type = LinkerType::UnspecifiedLinker;

    std::string Extension;
    std::string Prefix;
    std::string Suffix;

    virtual ~NativeLinker() = default;

    virtual void setObjectFiles(const Files &files) = 0;
    virtual void setInputLibraryDependencies(const FilesOrdered &files) {}
    virtual void setOutputFile(const path &out) = 0;
    virtual void setImportLibrary(const path &out) = 0;
    virtual void setLinkLibraries(const FilesOrdered &in) {}

    virtual path getOutputFile() const = 0;
    virtual path getImportLibrary() const = 0;

    FilesOrdered gatherLinkDirectories() const;
    FilesOrdered gatherLinkLibraries() const;
};

struct SW_DRIVER_CPP_API VisualStudioLibraryTool : VisualStudio,
    NativeLinker,
    CommandLineOptions<VisualStudioLibraryToolOptions>
{
    virtual ~VisualStudioLibraryTool() = default;

    virtual void setObjectFiles(const Files &files) override;
    virtual void setOutputFile(const path &out) override;
    virtual void setImportLibrary(const path &out) override;

    virtual path getOutputFile() const override;
    virtual path getImportLibrary() const override;

    virtual std::shared_ptr<builder::Command> getCommand() const override;

protected:
    virtual void getAdditionalOptions(driver::cpp::Command *c) const = 0;

    Version gatherVersion() const override { return VisualStudio::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API VisualStudioLinker : VisualStudioLibraryTool,
    CommandLineOptions<VisualStudioLinkerOptions>
{
    using NativeLinkerOptions::operator=;

    VisualStudioLinker();
    virtual ~VisualStudioLinker() = default;

    std::shared_ptr<Program> clone() const override;
    void getAdditionalOptions(driver::cpp::Command *c) const override;
    void setInputLibraryDependencies(const FilesOrdered &files) override;
};

struct SW_DRIVER_CPP_API VisualStudioLibrarian : VisualStudioLibraryTool,
    CommandLineOptions<VisualStudioLibrarianOptions>
{
    using NativeLinkerOptions::operator=;

    VisualStudioLibrarian();
    virtual ~VisualStudioLibrarian() = default;

    std::shared_ptr<Program> clone() const override;
    void getAdditionalOptions(driver::cpp::Command *c) const override;
};

struct SW_DRIVER_CPP_API GNULibraryTool : GNU,
    NativeLinker,
    CommandLineOptions<GNULibraryToolOptions>
{
    virtual ~GNULibraryTool() = default;

protected:
    virtual void getAdditionalOptions(driver::cpp::Command *c) const = 0;

    Version gatherVersion() const override { return GNU::gatherVersion(file); }
};

struct SW_DRIVER_CPP_API GNULinker : GNULibraryTool,
    CommandLineOptions<GNULinkerOptions>
{
    using NativeLinkerOptions::operator=;

    GNULinker();
    virtual ~GNULinker() = default;

    std::shared_ptr<Program> clone() const override;
    void getAdditionalOptions(driver::cpp::Command *c) const override;

    void setInputLibraryDependencies(const FilesOrdered &files) override;
    void setObjectFiles(const Files &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;
    void setLinkLibraries(const FilesOrdered &in) override;

    path getOutputFile() const override;
    path getImportLibrary() const override;

    std::shared_ptr<builder::Command> getCommand() const override;
};

struct SW_DRIVER_CPP_API GNULibrarian : GNULibraryTool,
    CommandLineOptions<GNULibrarianOptions>
{
    using NativeLinkerOptions::operator=;

    GNULibrarian();
    virtual ~GNULibrarian() = default;

    std::shared_ptr<Program> clone() const override;
    void getAdditionalOptions(driver::cpp::Command *c) const override;

    void setObjectFiles(const Files &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;

    virtual path getOutputFile() const override;
    virtual path getImportLibrary() const override;

    std::shared_ptr<builder::Command> getCommand() const override;
};

struct SW_DRIVER_CPP_API NativeToolchain
{
    std::shared_ptr<NativeLinker> Librarian;
    std::shared_ptr<NativeLinker> Linker;

    // rc (resource compiler)
    // ar, more tools...
    // more native compilers (fortran, cuda etc.)
    CompilerType CompilerType = CompilerType::UnspecifiedCompiler;
    LinkerType LinkerType; // rename
    BuildLibrariesAs LibrariesType = LibraryType::Shared;
    ConfigurationType ConfigurationType = ConfigurationType::Release;
    // more settings

    // misc
    bool CopySharedLibraries = true;

    // service

    /// set on server to eat everything
    bool AssignAll = false;

    // members
    //String getConfig() const;
};

}
