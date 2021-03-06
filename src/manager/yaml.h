// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "filesystem.h"

#include <primitives/yaml.h>

yaml load_yaml_config(const path &p);
yaml load_yaml_config(const String &s);

void dump_yaml_config(const path &p, const yaml &root);
String dump_yaml_config(const yaml &root);
