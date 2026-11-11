// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "bedrock/args.h"
#include "bedrock/parse.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool args_eq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

bool args_is_digits(const char *str)
{
    return bd_parse_digits(str);
}

bool args_parse_u16(const char *str, uint16_t *out)
{
    return bd_parse_u16(str, out);
}

int args_colon_count(const char *str)
{
    return bd_parse_colon_count(str);
}

bool args_split_host_port(const char *spec, char *host, const size_t host_size, uint16_t *port)
{
    return bd_parse_host_port(spec, host, host_size, port);
}

void args_init(ProgramArgs *it, const int argc, char **argv)
{
    if (!it) {
        return;
    }
    it->argc = argc;
    it->argv = argv;
    it->index = 1;
    it->program = (argc > 0 && argv && argv[0] && argv[0][0] != '\0') ? argv[0] : "program";
}

bool args_done(const ProgramArgs *it)
{
    return !it || !it->argv || it->index >= it->argc;
}

const char *args_current(const ProgramArgs *it)
{
    if (args_done(it)) {
        return NULL;
    }

    return it->argv[it->index];
}

const char *args_take(ProgramArgs *it)
{
    const char *cur = args_current(it);
    if (cur) {
        it->index++;
    }

    return cur;
}

bool args_match(ProgramArgs *it, const char *short_opt, const char *long_opt)
{
    const char *cur = args_current(it);
    if (!cur) {
        return false;
    }

    if (args_eq(cur, short_opt) || args_eq(cur, long_opt)) {
        it->index++;
        return true;
    }

    return false;
}

static ProgramArg arg_blank(void)
{
    const ProgramArg arg = {0};
    return arg;
}

ProgramArg args_flag(const char *id, const char short_name, const char *long_name, const char *help)
{
    ProgramArg arg = arg_blank();
    arg.id = id;
    arg.short_name = short_name;
    arg.long_name = long_name;
    arg.help = help;
    arg.action = BD_ARG_SET_TRUE;
    arg.kind = BD_VALUE_BOOL;
    return arg;
}

ProgramArg args_option(const char *id, const char short_name, const char *long_name, const char *help,
                    const ArgValueKind kind)
{
    ProgramArg arg = arg_blank();
    arg.id = id;
    arg.short_name = short_name;
    arg.long_name = long_name;
    arg.help = help;
    arg.action = BD_ARG_SET;
    arg.kind = kind;
    return arg;
}

ProgramArg args_positional(const char *id, const char *value_name, const char *help,
                        const ArgValueKind kind)
{
    ProgramArg arg = arg_blank();
    arg.id = id;
    arg.value_name = value_name;
    arg.help = help;
    arg.action = BD_ARG_SET;
    arg.kind = kind;
    arg.positional = true;
    return arg;
}

ProgramArg args_count(const char *id, const char short_name, const char *long_name, const char *help)
{
    ProgramArg arg = arg_blank();
    arg.id = id;
    arg.short_name = short_name;
    arg.long_name = long_name;
    arg.help = help;
    arg.action = BD_ARG_COUNT;
    arg.kind = BD_VALUE_U64;
    return arg;
}

void args_required(ProgramArg *arg, const bool required)
{
    if (arg) {
        arg->required = required;
    }
}

void args_default(ProgramArg *arg, const char *value)
{
    if (arg) {
        arg->default_value = value;
    }
}

void args_possible(ProgramArg *arg, const char *const *values)
{
    if (arg) {
        arg->possible = values;
    }
}

void args_conflicts(ProgramArg *arg, const char *id)
{
    if (arg) {
        arg->conflicts = id;
    }
}

void args_requires(ProgramArg *arg, const char *id)
{
    if (arg) {
        arg->requires = id;
    }
}

static void set_err(char *err, const size_t err_size, const char *fmt, ...)
{
    va_list ap;

    if (!err || err_size == 0) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

static bool takes_value(const ProgramArg *arg)
{
    return arg && (arg->action == BD_ARG_SET || arg->action == BD_ARG_APPEND);
}

static bool user_has_long(const ProgramCmd *command, const char *name)
{
    for (size_t i = 0; i < command->n_args; ++i) {
        if (!command->args[i].positional && args_eq(command->args[i].long_name, name)) {
            return true;
        }
    }

    return false;
}

static bool user_has_short(const ProgramCmd *command, const char name)
{
    if (name == '\0') {
        return false;
    }

    for (size_t i = 0; i < command->n_args; ++i) {
        if (!command->args[i].positional && command->args[i].short_name == name) {
            return true;
        }
    }

    return false;
}

static const ProgramArg *find_long(const ProgramCmd *command, const char *name, const size_t length)
{
    for (size_t i = 0; i < command->n_args; ++i) {
        const char *long_name = command->args[i].long_name;
        if (!command->args[i].positional && long_name && strlen(long_name) == length
            && strncmp(long_name, name, length) == 0) {
            return &command->args[i];
        }
    }

    return NULL;
}

static const ProgramArg *find_short(const ProgramCmd *command, const char name)
{
    for (size_t i = 0; i < command->n_args; ++i) {
        if (!command->args[i].positional && command->args[i].short_name == name) {
            return &command->args[i];
        }
    }

    return NULL;
}

static const ProgramCmd *find_sub(const ProgramCmd *command, const char *name)
{
    for (size_t i = 0; i < command->n_subs; ++i) {
        if (args_eq(command->subs[i].name, name)) {
            return &command->subs[i];
        }
    }

    return NULL;
}

static const ArgMatch *match_of(const CmdMatches *matches, const char *id)
{
    if (!matches || !id) {
        return NULL;
    }

    for (size_t i = 0; i < matches->n_items; ++i) {
        if (matches->items[i].spec && args_eq(matches->items[i].spec->id, id)) {
            return &matches->items[i];
        }
    }

    return NULL;
}

static const char *arg_label(const ProgramArg *arg, char *buf, const size_t buf_size)
{
    if (!arg) {
        snprintf(buf, buf_size, "argument");
        return buf;
    }

    if (arg->long_name) {
        snprintf(buf, buf_size, "--%s", arg->long_name);
        return buf;
    }

    if (arg->short_name) {
        snprintf(buf, buf_size, "-%c", arg->short_name);
        return buf;
    }

    if (arg->positional) {
        snprintf(buf, buf_size, "<%s>", arg->value_name ? arg->value_name : (arg->id ? arg->id : "ARG"));
        return buf;
    }

    snprintf(buf, buf_size, "%s", arg->id ? arg->id : "argument");
    return buf;
}

static bool value_allowed(const ProgramArg *arg, const char *text)
{
    if (!arg->possible) {
        return true;
    }

    for (const char *const *item = arg->possible; *item; ++item) {
        if (strcmp(*item, text) == 0) {
            return true;
        }
    }

    return false;
}

static bool value_typed(const ProgramArg *arg, const char *text)
{
    bool flag = false;
    int64_t i64 = 0;
    uint64_t u64 = 0;
    int32_t i32 = 0;
    uint32_t u32 = 0;
    uint16_t u16 = 0;
    double f64 = 0.0;

    switch (arg->kind) {
    case BD_VALUE_STRING:
        return true;
    case BD_VALUE_BOOL:
        return bd_parse_bool(text, &flag);
    case BD_VALUE_I64:
        return bd_parse_i64(text, &i64);
    case BD_VALUE_U64:
        return bd_parse_u64(text, &u64);
    case BD_VALUE_I32:
        return bd_parse_i32(text, &i32);
    case BD_VALUE_U32:
        return bd_parse_u32(text, &u32);
    case BD_VALUE_U16:
        return bd_parse_u16(text, &u16);
    case BD_VALUE_F64:
        return bd_parse_f64(text, &f64);
    default:
        return false;
    }
}

static bool push_value(ArgMatch *match, const char *text, const bool replace, const bool explicit)
{
    if (replace) {
        free(match->values);
        match->values = NULL;
        match->n_values = 0;
        match->capacity = 0;
    }

    if (match->n_values == match->capacity) {
        const size_t capacity = match->capacity == 0 ? 4u : match->capacity * 2u;
        const char **grown = realloc(match->values, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        match->values = grown;
        match->capacity = capacity;
    }

    match->values[match->n_values++] = text;
    match->count = match->n_values;
    match->explicit_present = match->explicit_present || explicit;

    return true;
}

static ArgsStatus accept_value(ArgMatch *match, const char *text, char *err, const size_t err_size)
{
    char label[64];
    const bool replace = match->spec->action != BD_ARG_APPEND;

    if (!text) {
        set_err(err, err_size, "a value is required for %s but none was supplied",
                arg_label(match->spec, label, sizeof(label)));
        return BD_ARGS_INVALID;
    }

    if (!value_allowed(match->spec, text) || !value_typed(match->spec, text)) {
        set_err(err, err_size, "invalid value '%s' for %s", text,
                arg_label(match->spec, label, sizeof(label)));
        return BD_ARGS_INVALID;
    }

    if (!push_value(match, text, replace, true)) {
        set_err(err, err_size, "out of memory");
        return BD_ARGS_INVALID;
    }

    return BD_ARGS_OK;
}

static ArgsStatus note_presence(ArgMatch *match)
{
    if (match->spec->action == BD_ARG_HELP) {
        return BD_ARGS_HELP;
    }

    if (match->spec->action == BD_ARG_VERSION) {
        return BD_ARGS_VERSION;
    }

    match->explicit_present = true;
    if (match->spec->action == BD_ARG_COUNT) {
        match->count++;
    } else {
        match->count = 1;
    }

    return BD_ARGS_OK;
}

static ArgsStatus parse_command(const ProgramCmd *command, int argc, char **argv, int index,
                                  CmdMatches *matches, char *err, size_t err_size);

static ArgsStatus finish_command(const CmdMatches *matches, char *err, const size_t err_size)
{
    size_t i;
    char left[64];
    char right[64];

    for (i = 0; i < matches->n_items; ++i) {
        ArgMatch *match = &matches->items[i];
        const ProgramArg *spec = match->spec;
        if (!spec) {
            continue;
        }

        if (!match->explicit_present && spec->default_value && takes_value(spec)) {
            if (!value_allowed(spec, spec->default_value) || !value_typed(spec, spec->default_value)) {
                set_err(err, err_size, "invalid value '%s' for %s", spec->default_value,
                        arg_label(spec, left, sizeof(left)));
                return BD_ARGS_INVALID;
            }

            if (!push_value(match, spec->default_value, true, false)) {
                set_err(err, err_size, "out of memory");
                return BD_ARGS_INVALID;
            }
        }
    }

    for (i = 0; i < matches->n_items; ++i) {
        const ArgMatch *match = &matches->items[i];
        const ProgramArg *spec = match->spec;
        const ArgMatch *other;
        if (!spec || !match->explicit_present) {
            continue;
        }

        if (spec->conflicts) {
            other = match_of(matches, spec->conflicts);
            if (other && other->explicit_present) {
                set_err(err, err_size, "argument %s conflicts with %s",
                        arg_label(spec, left, sizeof(left)),
                        arg_label(other->spec, right, sizeof(right)));
                return BD_ARGS_CONFLICT;
            }
        }

        if (spec->requires) {
            other = match_of(matches, spec->requires);
            if (!other || !other->explicit_present) {
                if (other) {
                    set_err(err, err_size, "argument %s requires %s", arg_label(spec, left, sizeof(left)),
                            arg_label(other->spec, right, sizeof(right)));
                } else {
                    set_err(err, err_size, "argument %s requires %s", arg_label(spec, left, sizeof(left)),
                            spec->requires);
                }
                return BD_ARGS_MISSING;
            }
        }
    }

    for (i = 0; i < matches->n_items; ++i) {
        const ArgMatch *match = &matches->items[i];
        const ProgramArg *spec = match->spec;
        if (!spec || !spec->required) {
            continue;
        }
        if (!match->explicit_present && match->n_values == 0) {
            set_err(err, err_size, "missing required argument %s", arg_label(spec, left, sizeof(left)));
            return BD_ARGS_MISSING;
        }
    }
    return BD_ARGS_OK;
}

static ArgsStatus parse_command(const ProgramCmd *command, const int argc, char **argv, int index,
                                  CmdMatches *matches, char *err, const size_t err_size)
{
    size_t i;
    size_t positional_cursor = 0;
    const ProgramArg *appending = NULL;
    bool options_done = false;

    if (!command || (command->n_args > 0 && !command->args) || (command->n_subs > 0 && !command->subs)) {
        set_err(err, err_size, "invalid command");
        return BD_ARGS_INVALID;
    }

    matches->command = command;
    matches->n_items = command->n_args;
    if (command->n_args > 0) {
        matches->items = calloc(command->n_args, sizeof(*matches->items));
        if (!matches->items) {
            set_err(err, err_size, "out of memory");
            return BD_ARGS_INVALID;
        }

        for (i = 0; i < command->n_args; ++i) {
            matches->items[i].spec = &command->args[i];
        }
    }

    while (index < argc) {
        const char *token = argv[index];
        ArgsStatus status;

        if (!token) {
            set_err(err, err_size, "invalid argument");
            return BD_ARGS_INVALID;
        }

        if (!options_done && strcmp(token, "--") == 0) {
            options_done = true;
            index++;
            continue;
        }

        if (!options_done && strncmp(token, "--", 2) == 0) {
            const char *name = token + 2;
            const char *eq = strchr(name, '=');
            const size_t length = eq ? (size_t)(eq - name) : strlen(name);
            const ProgramArg *spec = find_long(command, name, length);
            const char *value = NULL;

            if (length == 0) {
                set_err(err, err_size, "unknown option --");
                return BD_ARGS_UNKNOWN;
            }

            if (!spec && strncmp(name, "help", length) == 0 && length == 4 && !eq
                && !user_has_long(command, "help")) {
                return BD_ARGS_HELP;
            }

            if (!spec && command->version && strncmp(name, "version", length) == 0 && length == 7 && !eq
                && !user_has_long(command, "version")) {
                set_err(err, err_size, "%s", command->version);
                return BD_ARGS_VERSION;
            }

            if (!spec) {
                set_err(err, err_size, "unknown option --%.*s", (int)length, name);
                return BD_ARGS_UNKNOWN;
            }

            ArgMatch *match = &matches->items[(size_t) (spec - command->args)];

            if (!takes_value(spec)) {
                if (eq) {
                    char label[64];
                    set_err(err, err_size, "option %s does not take a value",
                            arg_label(spec, label, sizeof(label)));
                    return BD_ARGS_INVALID;
                }

                status = note_presence(match);
                if (status != BD_ARGS_OK) {
                    return status;
                }
                index++;
                continue;
            }

            if (eq) {
                value = eq + 1;
                index++;
            } else if (index + 1 < argc) {
                value = argv[++index];
                index++;
            } else {
                char label[64];
                set_err(err, err_size, "a value is required for %s but none was supplied",
                        arg_label(spec, label, sizeof(label)));
                return BD_ARGS_INVALID;
            }

            status = accept_value(match, value, err, err_size);
            if (status != BD_ARGS_OK) {
                return status;
            }
            continue;
        }

        if (!options_done && token[0] == '-' && token[1] != '\0'
            && !((token[1] >= '0' && token[1] <= '9') && !find_short(command, token[1]))) {
            const char *cursor = token + 1;
            index++;
            while (*cursor) {
                const ProgramArg *spec = find_short(command, *cursor);
                const char flag = *cursor;

                if (!spec && flag == 'h' && !user_has_short(command, 'h')) {
                    return BD_ARGS_HELP;
                }

                if (!spec && flag == 'V' && command->version && !user_has_short(command, 'V')) {
                    set_err(err, err_size, "%s", command->version);
                    return BD_ARGS_VERSION;
                }

                if (!spec) {
                    set_err(err, err_size, "unknown option -%c", flag);
                    return BD_ARGS_UNKNOWN;
                }

                ArgMatch *match = &matches->items[(size_t) (spec - command->args)];
                cursor++;
                if (!takes_value(spec)) {
                    status = note_presence(match);
                    if (status != BD_ARGS_OK) {
                        return status;
                    }
                    continue;
                }

                if (*cursor != '\0') {
                    status = accept_value(match, cursor, err, err_size);
                    if (status != BD_ARGS_OK) {
                        return status;
                    }
                    break;
                }

                if (index >= argc) {
                    char label[64];
                    set_err(err, err_size, "a value is required for %s but none was supplied",
                            arg_label(spec, label, sizeof(label)));
                    return BD_ARGS_INVALID;
                }

                status = accept_value(match, argv[index], err, err_size);
                if (status != BD_ARGS_OK) {
                    return status;
                }
                index++;
                break;
            }
            continue;
        }

        if (!appending && positional_cursor == 0) {
            const ProgramCmd *sub = find_sub(command, token);
            if (sub) {
                CmdMatches *child = calloc(1, sizeof(*child));
                if (!child) {
                    set_err(err, err_size, "out of memory");
                    return BD_ARGS_INVALID;
                }

                matches->child = child;
                matches->subcommand = sub;

                status = parse_command(sub, argc, argv, index + 1, child, err, err_size);
                if (status != BD_ARGS_OK) {
                    return status;
                }

                return finish_command(matches, err, err_size);
            }
        }

        {
            const ProgramArg *spec = appending;
            if (!spec) {
                size_t seen = 0;
                spec = NULL;
                for (i = 0; i < command->n_args; ++i) {
                    if (!command->args[i].positional) {
                        continue;
                    }
                    if (seen == positional_cursor) {
                        spec = &command->args[i];
                        positional_cursor++;
                        break;
                    }
                    seen++;
                }

                if (!spec) {
                    set_err(err, err_size, "unexpected argument '%s'", token);
                    return BD_ARGS_EXTRA;
                }

                if (spec->action == BD_ARG_APPEND) {
                    appending = spec;
                }
            }

            ArgMatch *match = &matches->items[(size_t) (spec - command->args)];
            status = accept_value(match, token, err, err_size);
            if (status != BD_ARGS_OK) {
                return status;
            }

            index++;
        }
    }

    return finish_command(matches, err, err_size);
}

ArgsStatus args_command_parse(const ProgramCmd *command, const int argc, char **argv, CmdMatches *out,
                              char *err, const size_t err_size)
{
    CmdMatches local;

    if (out) {
        memset(out, 0, sizeof(*out));
    }

    if (err && err_size > 0) {
        err[0] = '\0';
    }

    if (!command || argc < 0 || (argc > 0 && !argv)) {
        set_err(err, err_size, "invalid command");
        return BD_ARGS_INVALID;
    }

    memset(&local, 0, sizeof(local));

    const ArgsStatus status = parse_command(command, argc, argv, 1, &local, err, err_size);
    if (status != BD_ARGS_OK) {
        args_matches_free(&local);
        return status;
    }

    if (out) {
        *out = local;
    } else {
        args_matches_free(&local);
    }

    return BD_ARGS_OK;
}

static void upper_copy(const char *text, char *buf, const size_t buf_size)
{
    size_t i = 0;
    if (!text || text[0] == '\0') {
        text = "VALUE";
    }

    for (; text[i] && i + 1 < buf_size; ++i) {
        char ch = text[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        buf[i] = ch;
    }

    buf[i] = '\0';
}

static void option_syntax(const ProgramArg *arg, char *buf, const size_t buf_size)
{
    char value[48];
    const bool valued = takes_value(arg);

    if (valued) {
        upper_copy(arg->value_name ? arg->value_name : arg->id, value, sizeof(value));
    }

    if (arg->short_name && arg->long_name) {
        if (valued) {
            snprintf(buf, buf_size, "-%c, --%s <%s>", arg->short_name, arg->long_name, value);
        } else {
            snprintf(buf, buf_size, "-%c, --%s", arg->short_name, arg->long_name);
        }
    } else if (arg->long_name) {
        if (valued) {
            snprintf(buf, buf_size, "--%s <%s>", arg->long_name, value);
        } else {
            snprintf(buf, buf_size, "--%s", arg->long_name);
        }
    } else if (arg->short_name) {
        if (valued) {
            snprintf(buf, buf_size, "-%c <%s>", arg->short_name, value);
        } else {
            snprintf(buf, buf_size, "-%c", arg->short_name);
        }
    } else {
        buf[0] = '\0';
    }
}

static void write_row(FILE *out, const char *left, const char *help, const char *extra)
{
    const int width = left ? (int)strlen(left) : 0;
    if (width > 26) {
        fprintf(out, "  %s\n", left ? left : "");
        fprintf(out, "                            %s", help ? help : "");
    } else {
        fprintf(out, "  %-*s  %s", 26, left ? left : "", help ? help : "");
    }
    if (extra && extra[0] != '\0') {
        fprintf(out, " %s", extra);
    }
    fputc('\n', out);
}

void args_command_write_help(const ProgramCmd *command, FILE *out)
{
    size_t i;
    bool any_positional = false;

    if (!command || !out) {
        return;
    }
    const char *name = command->name ? command->name : "program";
    fprintf(out, "Usage: %s [OPTIONS]", name);
    for (i = 0; i < command->n_args; ++i) {
        /* Temporary buffer for uppercased positional arg name in the usage line. */
        char holder[48];

        const ProgramArg *arg = &command->args[i];
        if (!arg->positional) {
            continue;
        }

        any_positional = true;
        upper_copy(arg->value_name ? arg->value_name : arg->id, holder, sizeof(holder));

        if (arg->required) {
            fprintf(out, " <%s>", holder);
        } else {
            fprintf(out, " [%s]", holder);
        }

        if (arg->action == BD_ARG_APPEND) {
            fputs("...", out);
        }
    }

    if (command->n_subs > 0) {
        fputs(" [COMMAND]", out);
    }
    fputc('\n', out);
    if (command->about && command->about[0] != '\0') {
        fprintf(out, "\n%s\n", command->about);
    }

    if (any_positional) {
        fputs("\nArguments:\n", out);
        for (i = 0; i < command->n_args; ++i) {
            /* Uppercased positional name before wrapping as <NAME>. */
            char left[64];
            const ProgramArg *arg = &command->args[i];
            if (!arg->positional) {
                continue;
            }
            upper_copy(arg->value_name ? arg->value_name : arg->id, left, sizeof(left));
            {
                /* "<NAME>" text for a positional argument row. */
                char shown[80];
                snprintf(shown, sizeof(shown), "<%s>", left);
                write_row(out, shown, arg->help, NULL);
            }
        }
    }

    fputs("\nOptions:\n", out);
    for (i = 0; i < command->n_args; ++i) {
        /* Left column: option syntax (e.g. "-f, --file <FILE>"). */
        char left[96];
        /* Optional trailing note (e.g. "[default: ...]"). */
        char extra[96];
        const ProgramArg *arg = &command->args[i];
        if (arg->positional) {
            continue;
        }

        option_syntax(arg, left, sizeof(left));
        extra[0] = '\0';

        if (arg->default_value) {
            snprintf(extra, sizeof(extra), "[default: %s]", arg->default_value);
        }

        write_row(out, left, arg->help, extra);
    }
    if (!user_has_long(command, "help") && !user_has_short(command, 'h')) {
        write_row(out, "-h, --help", "Print help", NULL);
    }

    if (command->version && !user_has_long(command, "version") && !user_has_short(command, 'V')) {
        write_row(out, "-V, --version", "Print version", NULL);
    }

    if (command->n_subs > 0) {
        fputs("\nCommands:\n", out);
        for (i = 0; i < command->n_subs; ++i) {
            write_row(out, command->subs[i].name, command->subs[i].about, NULL);
        }
    }
}

void args_matches_free(CmdMatches *matches)
{
    if (!matches) {
        return;
    }

    if (matches->items) {
        for (size_t i = 0; i < matches->n_items; ++i) {
            free(matches->items[i].values);
        }
        free(matches->items);
    }

    if (matches->child) {
        args_matches_free(matches->child);
        free(matches->child);
    }

    memset(matches, 0, sizeof(*matches));
}

bool args_matches_present(const CmdMatches *matches, const char *id)
{
    const ArgMatch *match = match_of(matches, id);
    if (!match) {
        return false;
    }
    return match->explicit_present || match->n_values > 0;
}

size_t args_matches_count(const CmdMatches *matches, const char *id)
{
    const ArgMatch *match = match_of(matches, id);
    if (!match) {
        return 0;
    }
    return match->count;
}

const char *args_matches_get(const CmdMatches *matches, const char *id)
{
    const ArgMatch *match = match_of(matches, id);
    if (!match || match->n_values == 0) {
        return NULL;
    }
    return match->values[match->n_values - 1];
}

const char *const *args_matches_values(const CmdMatches *matches, const char *id, size_t *count)
{
    const ArgMatch *match = match_of(matches, id);
    if (count) {
        *count = match ? match->n_values : 0;
    }
    if (!match || match->n_values == 0) {
        return NULL;
    }
    return match->values;
}

bool args_matches_get_bool(const CmdMatches *matches, const char *id, bool *out)
{
    const ArgMatch *match = match_of(matches, id);
    if (!match || !out || !match->spec) {
        return false;
    }
    if (match->spec->action == BD_ARG_SET_TRUE) {
        *out = match->explicit_present;
        return true;
    }
    if (match->spec->action == BD_ARG_SET_FALSE) {
        *out = !match->explicit_present;
        return true;
    }
    if (match->n_values == 0) {
        return false;
    }
    return bd_parse_bool(match->values[match->n_values - 1], out);
}

bool args_matches_get_i64(const CmdMatches *matches, const char *id, int64_t *out)
{
    const char *text = args_matches_get(matches, id);
    return text && out && bd_parse_i64(text, out);
}

bool args_matches_get_u64(const CmdMatches *matches, const char *id, uint64_t *out)
{
    const ArgMatch *match = match_of(matches, id);
    if (!match || !out) {
        return false;
    }
    if (match->spec && match->spec->action == BD_ARG_COUNT) {
        *out = (uint64_t)match->count;
        return true;
    }
    return match->n_values > 0 && bd_parse_u64(match->values[match->n_values - 1], out);
}

bool args_matches_get_i32(const CmdMatches *matches, const char *id, int32_t *out)
{
    const char *text = args_matches_get(matches, id);
    return text && out && bd_parse_i32(text, out);
}

bool args_matches_get_u32(const CmdMatches *matches, const char *id, uint32_t *out)
{
    const char *text = args_matches_get(matches, id);
    return text && out && bd_parse_u32(text, out);
}

bool args_matches_get_u16(const CmdMatches *matches, const char *id, uint16_t *out)
{
    const char *text = args_matches_get(matches, id);
    return text && out && bd_parse_u16(text, out);
}

bool args_matches_get_f64(const CmdMatches *matches, const char *id, double *out)
{
    const char *text = args_matches_get(matches, id);
    return text && out && bd_parse_f64(text, out);
}

const ProgramCmd *args_matches_subcommand(const CmdMatches *matches)
{
    return matches ? matches->subcommand : NULL;
}

const CmdMatches *args_matches_child(const CmdMatches *matches)
{
    return matches ? matches->child : NULL;
}
