// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <package_path.h>

#include <primitives/context.h>
#include <primitives/filesystem.h>

namespace sw
{

enum class GeneratorType
{
    UnspecifiedGenerator,

    CMake,
    Ninja,
    QMake,
    UnixMakefiles,
    VisualStudio,
    VisualStudioNMake,
};

struct Generator
{
    GeneratorType type = GeneratorType::UnspecifiedGenerator;
    //path dir;
    path file;

    virtual ~Generator() = default;

    virtual void generate(const struct Build &b) = 0;
    void generate(const path &file, const struct Build &b);

    static std::unique_ptr<Generator> create(const String &s);
};

struct VSGenerator : Generator
{
    String cwd;
    path dir;
    const path projects_dir = "projects";
    const InsecurePath deps_subdir = "Dependencies";
    const String predefined_targets_dir = ". SW Predefined Targets"s;
    const String all_build_name = "ALL_BUILD"s;

    VSGenerator();

    void generate(const struct Build &b) override;
};

struct VSGeneratorNMake : VSGenerator
{
    void generate(const struct Build &b) override;
};

struct NinjaGenerator : Generator
{
    void generate(const struct Build &b) override;
};

String toString(GeneratorType Type);
String toPathString(GeneratorType Type);
GeneratorType fromString(const String &ss);

}
