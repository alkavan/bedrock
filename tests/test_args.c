// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/args.h>
#include <bedrock/unitest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const ProgramArg greet_args[] = {
    BD_ARG_OPTION("name", 'n', "name", "Name of the person", BD_VALUE_STRING, BD_ARG_REQUIRED),
    BD_ARG_OPTION("count", 'c', "count", "Times to greet", BD_VALUE_U64, BD_ARG_DEFAULT("1")),
};

static const ProgramCmd greet = BD_COMMAND("greet", "Greet a person", "1.0.0", greet_args);

static ArgsStatus parse_argv(const ProgramCmd *command, const int argc, char **argv,
                             CmdMatches *matches, char *err, const size_t err_size)
{
    return args_command_parse(command, argc, argv, matches, err, err_size);
}

TEST_CASE(test_args_walker_and_u16_wrapper, "[args]") {
    char arg0[] = "prog";
    char arg1[] = "-h";
    char arg2[] = "file";
    char *argv[] = {arg0, arg1, arg2};
    ProgramArgs it;
    uint16_t port = 0;
    char host[32];

    args_init(&it, 3, argv);
    REQUIRE(strcmp(it.program, "prog") == 0);
    REQUIRE(strcmp(args_current(&it), "-h") == 0);
    REQUIRE(args_match(&it, "-h", "--help"));
    REQUIRE(strcmp(args_take(&it), "file") == 0);
    REQUIRE(args_done(&it));
    REQUIRE(args_take(&it) == NULL);

    REQUIRE(args_is_digits("8080"));
    REQUIRE(!args_is_digits("80a"));
    REQUIRE(args_parse_u16("65535", &port));
    REQUIRE(port == 65535u);
    REQUIRE(!args_parse_u16("65536", &port));
    REQUIRE(args_colon_count("a:b") == 1);
    REQUIRE(args_split_host_port("localhost:80", host, sizeof(host), &port));
    REQUIRE(strcmp(host, "localhost") == 0);
    REQUIRE(port == 80u);
}

TEST_CASE(test_args_options_and_defaults, "[args]") {
    char arg0[] = "greet";
    char arg1[] = "--name=Me";
    char *argv[] = {arg0, arg1};
    CmdMatches matches;
    char err[128];
    uint64_t count = 0;

    REQUIRE(parse_argv(&greet, 2, argv, &matches, err, sizeof(err)) == BD_ARGS_OK);
    REQUIRE(strcmp(args_matches_get(&matches, "name"), "Me") == 0);
    REQUIRE(args_matches_present(&matches, "count"));
    REQUIRE(args_matches_get_u64(&matches, "count", &count));
    REQUIRE(count == 1u);
    args_matches_free(&matches);

    {
        char name_flag[] = "-nMe";
        char *short_argv[] = {arg0, name_flag};
        REQUIRE(parse_argv(&greet, 2, short_argv, &matches, err, sizeof(err)) == BD_ARGS_OK);
        REQUIRE(strcmp(args_matches_get(&matches, "name"), "Me") == 0);
        args_matches_free(&matches);
    }

    {
        char *missing[] = {arg0};
        REQUIRE(parse_argv(&greet, 1, missing, &matches, err, sizeof(err)) == BD_ARGS_MISSING);
        REQUIRE(strstr(err, "name") != NULL);
    }
}

TEST_CASE(test_args_cluster_replace_and_negative, "[args]") {
    static const ProgramArg args[] = {
        BD_ARG_FLAG("all", 'a', "all", "All"),
        BD_ARG_FLAG("brief", 'b', "brief", "Brief"),
        BD_ARG_OPTION("num", 'n', "num", "Number", BD_VALUE_I64, BD_ARG_REQUIRED),
        BD_ARG_POSITIONAL("rest", "REST", "Rest", BD_VALUE_I64, false),
    };

    static const ProgramCmd command = BD_COMMAND("tool", "Tool", NULL, args);
    char arg0[] = "tool";
    char arg1[] = "-abn5";
    char arg2[] = "--num";
    char arg3[] = "-8";
    char arg4[] = "-3";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4};
    CmdMatches matches;
    char err[128];
    bool all = false;
    bool brief = false;
    int64_t number = 0;
    int64_t rest = 0;

    REQUIRE(parse_argv(&command, 5, argv, &matches, err, sizeof(err)) == BD_ARGS_OK);
    REQUIRE(args_matches_get_bool(&matches, "all", &all) && all);
    REQUIRE(args_matches_get_bool(&matches, "brief", &brief) && brief);
    REQUIRE(args_matches_get_i64(&matches, "num", &number));
    REQUIRE(number == -8);
    REQUIRE(args_matches_get_i64(&matches, "rest", &rest));
    REQUIRE(rest == -3);
    args_matches_free(&matches);
}

TEST_CASE(test_args_append_count_and_possible, "[args]") {
    static const char *const colors[] = {"red", "blue", NULL};
    static ProgramArg args[] = {
        BD_ARG_OPTION("file", 'f', "file", "File", BD_VALUE_STRING, BD_ARG_OPTIONAL),
        BD_ARG_COUNT("verbose", 'v', "verbose", "Verbose"),
        BD_ARG_OPTION("color", 'c', "color", "Color", BD_VALUE_STRING, BD_ARG_OPTIONAL),
    };

    static ProgramCmd command = BD_COMMAND("tool", "Tool", "1.0.0", args);
    char arg0[] = "tool";
    char arg1[] = "--file";
    char arg2[] = "a";
    char arg3[] = "-fb";
    char arg4[] = "-vv";
    char arg5[] = "--verbose";
    char arg6[] = "--color";
    char arg7[] = "blue";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7};
    CmdMatches matches;
    char err[128];
    size_t count = 0;
    uint64_t verbose = 0;

    args[0].action = BD_ARG_APPEND;
    args_possible(&args[2], colors);
    command.args = args;

    REQUIRE(parse_argv(&command, 8, argv, &matches, err, sizeof(err)) == BD_ARGS_OK);
    const char *const *values = args_matches_values(&matches, "file", &count);
    REQUIRE(count == 2u);
    REQUIRE(values != NULL);
    REQUIRE(strcmp(values[0], "a") == 0);
    REQUIRE(strcmp(values[1], "b") == 0);
    REQUIRE(args_matches_count(&matches, "verbose") == 3u);
    REQUIRE(args_matches_get_u64(&matches, "verbose", &verbose));
    REQUIRE(verbose == 3u);
    REQUIRE(strcmp(args_matches_get(&matches, "color"), "blue") == 0);
    args_matches_free(&matches);

    {
        char bad_color[] = "--color=green";
        char *bad[] = {arg0, bad_color};
        REQUIRE(parse_argv(&command, 2, bad, &matches, err, sizeof(err)) == BD_ARGS_INVALID);
    }
}

TEST_CASE(test_args_conflicts_requires_and_end, "[args]") {
    static ProgramArg args[] = {
        BD_ARG_FLAG("json", 'j', "json", "JSON"),
        BD_ARG_FLAG("pretty", 'p', "pretty", "Pretty"),
        BD_ARG_FLAG("output", 'o', "output", "Output"),
        BD_ARG_OPTION("format", 'f', "format", "Format", BD_VALUE_STRING, BD_ARG_OPTIONAL),
        BD_ARG_POSITIONAL("item", "ITEM", "Item", BD_VALUE_STRING, false),
    };
    static const ProgramCmd command = BD_COMMAND("tool", "Tool", NULL, args);
    char arg0[] = "tool";
    char arg1[] = "--json";
    char arg2[] = "--pretty";
    char *conflict[] = {arg0, arg1, arg2};
    CmdMatches matches;
    char err[160];

    args_conflicts(&args[0], "pretty");
    args_requires(&args[2], "format");

    REQUIRE(parse_argv(&command, 3, conflict, &matches, err, sizeof(err)) == BD_ARGS_CONFLICT);

    {
        char output[] = "--output";
        char *alone[] = {arg0, output};
        REQUIRE(parse_argv(&command, 2, alone, &matches, err, sizeof(err)) == BD_ARGS_MISSING);
    }
    {
        char output[] = "--output";
        char format_flag[] = "--format";
        char format_value[] = "text";
        char dashdash[] = "--";
        char item[] = "--not-an-option";
        char *ok[] = {arg0, output, format_flag, format_value, dashdash, item};
        REQUIRE(parse_argv(&command, 6, ok, &matches, err, sizeof(err)) == BD_ARGS_OK);
        REQUIRE(args_matches_present(&matches, "output"));
        REQUIRE(strcmp(args_matches_get(&matches, "format"), "text") == 0);
        REQUIRE(strcmp(args_matches_get(&matches, "item"), "--not-an-option") == 0);
        args_matches_free(&matches);
    }
}

TEST_CASE(test_args_subcommand_help_and_version, "[args]") {
    static const ProgramArg add_args[] = {
        BD_ARG_POSITIONAL("item", "ITEM", "Item to add", BD_VALUE_STRING, true),
    };
    static const ProgramCmd subs[] = {
        BD_COMMAND("add", "Add an item", NULL, add_args),
    };
    static const ProgramArg root_args[] = {
        BD_ARG_FLAG("verbose", 'v', "verbose", "Verbose"),
    };

    static const ProgramCmd root = BD_COMMAND_WITH_SUBS("tool", "Tool", "9.9.9", root_args, subs);
    char arg0[] = "tool";
    char arg1[] = "-v";
    char arg2[] = "add";
    char arg3[] = "widget";
    char *argv[] = {arg0, arg1, arg2, arg3};
    CmdMatches matches;
    char err[128];
    bool verbose = false;

    REQUIRE(parse_argv(&root, 4, argv, &matches, err, sizeof(err)) == BD_ARGS_OK);
    REQUIRE(args_matches_subcommand(&matches) == &subs[0]);
    REQUIRE(args_matches_child(&matches) != NULL);
    REQUIRE(args_matches_get_bool(&matches, "verbose", &verbose) && verbose);
    REQUIRE(strcmp(args_matches_get(args_matches_child(&matches), "item"), "widget") == 0);
    args_matches_free(&matches);

    {
        char help_flag[] = "--help";
        char *help_argv[] = {arg0, help_flag};
        REQUIRE(parse_argv(&root, 2, help_argv, &matches, err, sizeof(err)) == BD_ARGS_HELP);
        char version_flag[] = "-V";
        char *version_argv[] = {arg0, version_flag};
        REQUIRE(parse_argv(&root, 2, version_argv, &matches, err, sizeof(err)) == BD_ARGS_VERSION);
        REQUIRE(strcmp(err, "9.9.9") == 0);
    }

    {
        char *extra[] = {arg0, arg2};
        REQUIRE(parse_argv(&root, 2, extra, &matches, err, sizeof(err)) == BD_ARGS_MISSING);
    }

    {
        char unknown[] = "--nope";
        char *bad[] = {arg0, unknown};
        REQUIRE(parse_argv(&root, 2, bad, &matches, err, sizeof(err)) == BD_ARGS_UNKNOWN);
    }

    FILE *fp = tmpfile();
    REQUIRE(fp != NULL);
    args_command_write_help(&greet, fp);
    REQUIRE(fseek(fp, 0, SEEK_END) == 0);
    const long size = ftell(fp);
    REQUIRE(size > 0);
    rewind(fp);

    char *help = malloc((size_t) size + 1u);
    REQUIRE(help != NULL);
    REQUIRE(fread(help, 1, (size_t)size, fp) == (size_t)size);
    help[size] = '\0';
    fclose(fp);

    REQUIRE(strstr(help, "Usage: greet") != NULL);
    REQUIRE(strstr(help, "--name") != NULL);
    REQUIRE(strstr(help, "--count") != NULL);
    REQUIRE(strstr(help, "[default: 1]") != NULL);
    REQUIRE(strstr(help, "--help") != NULL);
    REQUIRE(strstr(help, "--version") != NULL);
    free(help);
}

TEST_CASE(test_args_function_builders, "[args]") {
    ProgramArg args[2];
    ProgramCmd command;
    char arg0[] = "greet";
    char arg1[] = "--name";
    char arg2[] = "Ada";
    char flag[] = "-qq";
    char *argv[] = {arg0, arg1, arg2, flag};
    CmdMatches matches;
    char err[64];
    uint64_t count = 0;

    args[0] = args_option("name", 'n', "name", "Name", BD_VALUE_STRING);
    args_required(&args[0], true);
    args[1] = args_count("quiet", 'q', "quiet", "Quiet");
    memset(&command, 0, sizeof(command));
    command.name = "greet";
    command.about = "Greet";
    command.args = args;
    command.n_args = 2;

    REQUIRE(parse_argv(&command, 4, argv, &matches, err, sizeof(err)) == BD_ARGS_OK);
    REQUIRE(strcmp(args_matches_get(&matches, "name"), "Ada") == 0);
    REQUIRE(args_matches_get_u64(&matches, "quiet", &count));
    REQUIRE(count == 2u);
    args_matches_free(&matches);
}
