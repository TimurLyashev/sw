#ifndef COMMA
#define COMMA
#define SW_COMMA_SELF
#endif

/*
commands are:

test file/dir profile - test something. profile - test actions, default - test
profiles: all, bench, test, ..., doc? ...

run file/dir/pkg - run the selected program or an executable package directly,
run knows how to run pkg (e.g. if we run python package it will select py interpreter with correct version)

update - update packages db

upgrade - upgrade installed pkgs

self-upgrade - upgrade the client. implement via upgrade?
*/

SUBCOMMAND(build, "Build files, dirs or packages") COMMA
SUBCOMMAND(ide, "Used to invoke sw application to do IDE tasks: generate project files, clean, rebuild etc.") COMMA
SUBCOMMAND(init, "Used to do some system setup which may require administrator access.") COMMA
SUBCOMMAND(uri, "Used to invoke sw application from the website.") COMMA

#ifdef SW_COMMA_SELF
#undef COMMA
#undef SW_COMMA_SELF
#endif
