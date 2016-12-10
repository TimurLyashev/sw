/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "project_path.h"

#include "enums.h"

ProjectPath::ProjectPath(String s)
{
    if (s.size() > 2048)
        throw std::runtime_error("Too long project path (must be <= 2048)");

    auto prev = s.begin();
    for (auto i = s.begin(); i != s.end(); ++i)
    {
        auto &c = *i;
        if (c < 0 || c > 127 ||
            !(isalnum(c) || c == '.' || c == '_'))
            throw std::runtime_error("Bad symbol in project name");
        if (isupper(c))
            c = (char)tolower(c);
        if (c == '.')
        {
            path_elements.emplace_back(prev, i);
            prev = std::next(i);
        }
    }
    if (!s.empty())
        path_elements.emplace_back(prev, s.end());
}

ProjectPath::ProjectPath(const PathElements &pe)
    : path_elements(pe)
{
}

String ProjectPath::toString(const String &delim) const
{
    String p;
    if (path_elements.empty())
        return p;
    for (auto &e : path_elements)
        p += e + delim;
    p.resize(p.size() - delim.size());
    return p;
}

String ProjectPath::toPath() const
{
    String p = toString();
    std::replace(p.begin(), p.end(), '.', '/');
    return p;
}

path ProjectPath::toFileSystemPath() const
{
    // TODO: replace with hash, affects both server and client
    path p;
    if (path_elements.empty())
        return p;
    int i = 0;
    for (auto &e : path_elements)
    {
        if (i++ == toIndex(PathElementType::Owner))
        {
            p /= e.substr(0, 1);
            p /= e.substr(0, 2);
        }
        p /= e;
    }
    return p;
}

bool ProjectPath::operator<(const ProjectPath &p) const
{
    if (path_elements.empty() && p.path_elements.empty())
        return false;
    if (path_elements.empty())
        return true;
    if (p.path_elements.empty())
        return false;
    auto &p0 = path_elements[0];
    auto &pp0 = p.path_elements[0];
    if (p0 == pp0)
        return path_elements < p.path_elements;
    // ??
    if (p0 == "org")
        return true;
    if (pp0 == "org")
        return false;
    if (p0 == "pvt")
        return true;
    if (pp0 == "pvt")
        return false;
    return false;
}

bool ProjectPath::has_namespace() const
{
    if (path_elements.empty())
        return false;
    if (path_elements[0] == pvt().path_elements[0] ||
        path_elements[0] == org().path_elements[0] ||
        path_elements[0] == com().path_elements[0] ||
        path_elements[0] == loc().path_elements[0])
        return true;
    return false;
}

ProjectPath::PathElement ProjectPath::get_owner() const
{
    if (path_elements.size() < 2)
        return PathElement();
    return path_elements[1];
}

bool ProjectPath::is_absolute(const String &username) const
{
    if (!has_namespace())
        return false;
    if (username.empty())
    {
        if (path_elements.size() > 1)
            return true;
        return false;
    }
    if (path_elements.size() > 2 && path_elements[1] == username)
        return true;
    return false;
}

bool ProjectPath::is_relative(const String &username) const
{
    return !is_absolute(username);
}

ProjectPath ProjectPath::operator[](PathElementType e) const
{
    switch (e)
    {
    case PathElementType::Namespace:
        if (path_elements.empty())
            return *this;
        return PathElements{ path_elements.begin(), path_elements.begin() + 1 };
    case PathElementType::Owner:
        if (path_elements.size() < 2)
            return *this;
        return PathElements{ path_elements.begin() + 1, path_elements.begin() + 2 };
    case PathElementType::Tail:
        if (path_elements.size() >= 2)
            return *this;
        return PathElements{ path_elements.begin() + 2, path_elements.end() };
    }
    return *this;
}

bool ProjectPath::is_root_of(const ProjectPath &p) const
{
    if (path_elements.size() >= p.path_elements.size())
        return false;
    for (size_t i = 0; i < path_elements.size(); i++)
    {
        if (path_elements[i] != p.path_elements[i])
            return false;
    }
    return true;
}

void ProjectPath::push_back(const PathElement &pe)
{
    path_elements.push_back(pe);
}