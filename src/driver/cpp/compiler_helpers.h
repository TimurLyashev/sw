// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace sw
{

template <class T>
static void getCommandLineOptions(driver::cpp::Command *c, const CommandLineOptions<T> &t, const String prefix = "", bool end_options = false)
{
    for (auto &o : t)
    {
        if (o.manual_handling)
            continue;
        if (end_options != o.place_at_the_end)
            continue;
        auto cmd = o.getCommandLine(c);
        for (auto &c2 : cmd)
        {
            if (!prefix.empty())
                c->args.push_back(prefix);
            c->args.push_back(c2);
        }
    }
}

}
