// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock.h>
#include <bedrock/args.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const ProgramArg greet_args[] = {
    BD_ARG_OPTION("name", 'n', "name", "Name of the person", BD_VALUE_STRING, BD_ARG_REQUIRED),
    BD_ARG_OPTION("count", 'c', "count", "Times to greet", BD_VALUE_U64, BD_ARG_DEFAULT("1")),
    BD_ARG_FLAG("shout", 's', "shout", "Shout the greeting"),
};

static const ProgramArg add_args[] = {
    BD_ARG_POSITIONAL("item", "ITEM", "Item to add", BD_VALUE_STRING, true),
    BD_ARG_POSITIONAL("note", "NOTE", "Optional note", BD_VALUE_STRING, false),
};

static const ProgramArg serve_args[] = {
    BD_ARG_OPTION("listen", 'l', "listen", "Address to listen on, as host or host:port",
                  BD_VALUE_STRING, BD_ARG_DEFAULT("127.0.0.1:8080")),
};

static const ProgramArg help_args[] = {
    BD_ARG_POSITIONAL("command", "COMMAND", "Command to describe", BD_VALUE_STRING, false),
};

enum {
    CMD_GREET = 0,
    CMD_ADD,
    CMD_SERVE,
    CMD_HELP
};

/* Defined in place. MSVC rejects copying one static aggregate into another. */
static const ProgramCmd commands[] = {
    BD_COMMAND("greet", "Greet a person", NULL, greet_args),
    BD_COMMAND("add", "Add an item", NULL, add_args),
    BD_COMMAND("serve", "Listen for connections", NULL, serve_args),
    BD_COMMAND("help", "Show help for a command", NULL, help_args),
};

static const ProgramCmd root = {
    .name = "args_example",
    .about = "Demonstrate Bedrock argument parsing",
    .version = BEDROCK_VERSION,
    .subs = commands,
    .n_subs = sizeof(commands) / sizeof(commands[0]),
};

static const ProgramCmd *find_command(const char *name)
{
    if (!name) {
        return NULL;
    }

    for (size_t i = 0; i < root.n_subs; ++i) {
        if (root.subs[i].name && strcmp(root.subs[i].name, name) == 0) {
            return &root.subs[i];
        }
    }

    return NULL;
}

/* First subcommand token, if any. Stops at --help/-h so a leading help flag stays on the root. */
static const ProgramCmd *command_before_help(const int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            break;
        }

        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return NULL;
        }

        if (argv[i][0] == '-') {
            continue;
        }

        return find_command(argv[i]);
    }

    return NULL;
}

/* Subcommand the user named, including one that follows nothing but options. */
static const ProgramCmd *command_invoked(const int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) {
            break;
        }

        if (argv[i][0] == '-') {
            continue;
        }

        return find_command(argv[i]);
    }

    return NULL;
}

static int fail_command(const ProgramCmd *command, const char *err)
{
    if (err && err[0] != '\0') {
        fprintf(stderr, "%s\n\n", err);
    }

    args_command_write_help(command ? command : &root, stderr);
    return 1;
}

static int run_help(const CmdMatches *child)
{
    const char *name = args_matches_get(child, "command");

    if (!name) {
        args_command_write_help(&root, stdout);
        return 0;
    }

    const ProgramCmd *command = find_command(name);
    if (!command) {
        char err[128];
        snprintf(err, sizeof(err), "unknown command '%s'", name);
        return fail_command(&root, err);
    }

    args_command_write_help(command, stdout);
    return 0;
}

static int run_greet(const CmdMatches *child)
{
    const char *name = args_matches_get(child, "name");
    uint64_t count = 1;
    bool shout = false;

    if (!name) {
        return fail_command(&commands[CMD_GREET], "missing required argument --name");
    }

    args_matches_get_u64(child, "count", &count);
    args_matches_get_bool(child, "shout", &shout);

    for (uint64_t i = 0; i < count; ++i) {
        fputs(shout ? "HELLO " : "Hello ", stdout);
        if (shout) {
            for (const char *cursor = name; *cursor; ++cursor) {
                fputc(toupper((unsigned char)*cursor), stdout);
            }
        } else {
            fputs(name, stdout);
        }
        fputs("!\n", stdout);
    }

    return 0;
}

static int run_add(const CmdMatches *child)
{
    const char *item = args_matches_get(child, "item");
    const char *note = args_matches_get(child, "note");

    if (!item) {
        return fail_command(&commands[CMD_ADD], "missing required argument <ITEM>");
    }
    if (note) {
        printf("added %s (%s)\n", item, note);
    } else {
        printf("added %s\n", item);
    }

    return 0;
}

static int run_serve(const CmdMatches *child)
{
    const char *listen = args_matches_get(child, "listen");
    char host[256];
    uint16_t port = 0;
    char err[160];

    if (!listen || !args_split_host_port(listen, host, sizeof(host), &port)) {
        snprintf(err, sizeof(err), "invalid listen address '%s'", listen ? listen : "");
        return fail_command(&commands[CMD_SERVE], err);
    }

    if (port == 0) {
        port = 8080;
    }

    printf("listening on %s port %u\n", host, (unsigned)port);
    return 0;
}

int main(const int argc, char **argv)
{
    CmdMatches matches;
    char err[256];
    const ArgsStatus status = args_command_parse(&root, argc, argv, &matches, err, sizeof(err));
    const ProgramCmd *invoked = command_invoked(argc, argv);
    int result = 0;

    if (status == BD_ARGS_HELP) {
        const ProgramCmd *target = command_before_help(argc, argv);
        args_command_write_help(target ? target : &root, stdout);
        return 0;
    }

    if (status == BD_ARGS_VERSION) {
        printf("%s %s\n", root.name, root.version);
        return 0;
    }

    if (status != BD_ARGS_OK) {
        return fail_command(invoked, err);
    }

    if (!args_matches_subcommand(&matches)) {
        args_matches_free(&matches);
        args_command_write_help(&root, stdout);
        return 0;
    }

    const ProgramCmd *selected = args_matches_subcommand(&matches);
    if (selected && selected->name && strcmp(selected->name, "help") == 0) {
        result = run_help(args_matches_child(&matches));
    } else if (selected && selected->name && strcmp(selected->name, "greet") == 0) {
        result = run_greet(args_matches_child(&matches));
    } else if (selected && selected->name && strcmp(selected->name, "add") == 0) {
        result = run_add(args_matches_child(&matches));
    } else if (selected && selected->name && strcmp(selected->name, "serve") == 0) {
        result = run_serve(args_matches_child(&matches));
    } else {
        result = fail_command(&root, "unknown command");
    }

    args_matches_free(&matches);
    return result;
}
