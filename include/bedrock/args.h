// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_ARGS_H
#define BEDROCK_ARGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    int argc;             /**< Argument count. */
    char **argv;          /**< Argument vector. */
    int index;            /**< Current index (starts at 1). */
    const char *program;  /**< argv[0], or "program". */
} ProgramArgs;

/**
 * @brief Initialize argument walker from argc/argv.
 *
 * @param it    Walker state (no-op if NULL).
 * @param argc  Argument count.
 * @param argv  Argument vector; argv[0] becomes program name when present.
 */
void args_init(ProgramArgs *it, int argc, char **argv);

/**
 * @brief Whether all arguments have been consumed.
 *
 * @param it Argument walker.
 * @return true if it is NULL/invalid or index is past argc; false otherwise.
 */
bool args_done(const ProgramArgs *it);

/**
 * @brief Current argument without advancing.
 *
 * @param it Argument walker.
 * @return argv[index], or NULL when done.
 */
const char *args_current(const ProgramArgs *it);

/**
 * @brief Take the current argument and advance the index.
 *
 * @param it Argument walker; advanced when an argument is taken.
 * @return The taken argument, or NULL when done.
 */
const char *args_take(ProgramArgs *it);

/**
 * @brief If current argv matches short or long option, consume it.
 *
 * @param it        Argument walker; advanced on match.
 * @param short_opt Optional short form (e.g. "-h"); may be NULL.
 * @param long_opt  Optional long form (e.g. "--help"); may be NULL.
 * @return true if matched and consumed; false otherwise.
 */
bool args_match(ProgramArgs *it, const char *short_opt, const char *long_opt);

/**
 * @brief True if str is non-empty and consists only of decimal digits.
 *
 * @param str String to test; may be NULL.
 * @return true if str is non-empty and digits-only; false otherwise.
 */
bool args_is_digits(const char *str);

/**
 * @brief Parse an unsigned 16-bit decimal value from str into out.
 *
 * @param str Digit string to parse; may be NULL.
 * @param out Receives the parsed value on success; must not be NULL.
 * @return true on success; false on NULL, empty, non-digits, overflow, or NULL
 *         out.
 */
bool args_parse_u16(const char *str, uint16_t *out);

/**
 * @brief Count ':' characters in str.
 *
 * @param str String to scan; may be NULL.
 * @return Number of ':' characters, or 0 if str is NULL.
 */
int args_colon_count(const char *str);

/**
 * @brief Split "host" or "host:port".
 *
 * Several colons are the whole host; a single trailing ":digits" with non-zero
 * port is parsed. Rejects empty/leading-dash specs and hosts that do not fit in
 * host_size (including NUL).
 *
 * @param spec      Input "host" or "host:port".
 * @param host      Output buffer for host (always NUL-terminated on success).
 * @param host_size Capacity of host.
 * @param port      Optional; set when a valid port is present.
 * @return true on success; false on invalid input or if the host does not fit.
 */
bool args_split_host_port(const char *spec, char *host, size_t host_size, uint16_t *port);

/**
 * @brief What an argument does when it appears on the command line.
 *
 * SET stores one value and a later occurrence replaces it. APPEND stores every
 * occurrence. SET_TRUE and SET_FALSE are flags. COUNT counts occurrences,
 * including clustered shorts such as -vvv. HELP and VERSION stop parsing.
 */
typedef enum ArgAction {
    BD_ARG_SET = 0,
    BD_ARG_APPEND,
    BD_ARG_SET_TRUE,
    BD_ARG_SET_FALSE,
    BD_ARG_COUNT,
    BD_ARG_HELP,
    BD_ARG_VERSION
} ArgAction;

/**
 * @brief How a stored value is interpreted by the typed getters.
 */
typedef enum ArgValueKind {
    BD_VALUE_STRING = 0,
    BD_VALUE_BOOL,
    BD_VALUE_I64,
    BD_VALUE_U64,
    BD_VALUE_I32,
    BD_VALUE_U32,
    BD_VALUE_U16,
    BD_VALUE_F64
} ArgValueKind;

/**
 * @brief Result of args_command_parse. Parsing never exits the process.
 */
typedef enum ArgsStatus {
    BD_ARGS_OK = 0,
    BD_ARGS_HELP,
    BD_ARGS_VERSION,
    BD_ARGS_MISSING,
    BD_ARGS_UNKNOWN,
    BD_ARGS_INVALID,
    BD_ARGS_CONFLICT,
    BD_ARGS_EXTRA
} ArgsStatus;

/**
 * @brief One flag, option, or positional argument.
 *
 * The schema is not allocated. Value pointers passed in must outlive parsing.
 * possible is a NULL-terminated list. conflicts and requires name another
 * argument id. short_name 0 and a NULL long_name omit that form.
 */
typedef struct ProgramArg {
    const char *id;
    char short_name;
    const char *long_name;
    const char *help;
    const char *value_name;
    const char *default_value;
    const char *const *possible;
    const char *conflicts;
    const char *requires;
    ArgAction action;
    ArgValueKind kind;
    bool required;
    bool positional;
} ProgramArg;

/**
 * @brief A command and its direct subcommands.
 *
 * subs points at an array of the same struct, so nesting is recursive.
 * --help is recognized automatically. --version is recognized when version
 * is not NULL, unless the command already defines those names.
 */
typedef struct ProgramCmd {
    const char *name;
    const char *about;
    const char *version;
    const ProgramArg *args;
    size_t n_args;
    const struct ProgramCmd *subs;
    size_t n_subs;
} ProgramCmd;

/**
 * @brief Values captured for one argument. Owned by CmdMatches.
 */
typedef struct ArgMatch {
    const ProgramArg *spec;
    bool explicit_present;
    size_t count;
    const char **values;
    size_t n_values;
    size_t capacity;
} ArgMatch;

/**
 * @brief Successful parse of one command.
 *
 * String values point at argv or at default literals. child owns the parse of
 * the selected subcommand. Release the whole tree with args_matches_free.
 */
typedef struct CmdMatches {
    const ProgramCmd *command;
    const ProgramCmd *subcommand;
    struct CmdMatches *child;
    ArgMatch *items;
    size_t n_items;
} CmdMatches;

/**
 * @brief Flag that stores true when present.
 */
ProgramArg args_flag(const char *id, char short_name, const char *long_name, const char *help);

/**
 * @brief Option that stores one value of the given kind.
 */
ProgramArg args_option(const char *id, char short_name, const char *long_name,
                       const char *help, ArgValueKind kind);

/**
 * @brief Positional argument that stores one value.
 */
ProgramArg args_positional(const char *id, const char *value_name,
                             const char *help, ArgValueKind kind);

/**
 * @brief Flag that counts occurrences.
 */
ProgramArg args_count(const char *id, char short_name, const char *long_name, const char *help);

/**
 * @brief Mark an argument required. NULL is ignored.
 */
void args_required(ProgramArg *arg, bool required);

/**
 * @brief Set the default text used when the argument is absent.
 */
void args_default(ProgramArg *arg, const char *value);

/**
 * @brief Restrict the value to a NULL-terminated list. The list is not copied.
 */
void args_possible(ProgramArg *arg, const char *const *values);

/**
 * @brief Reject a parse where both this argument and id were given.
 */
void args_conflicts(ProgramArg *arg, const char *id);

/**
 * @brief Require id to be present whenever this argument is given.
 */
void args_requires(ProgramArg *arg, const char *id);

/**
 * @brief Parse argv against a command.
 *
 * argv[0] is the program name and is not an argument. On BD_ARGS_OK, out owns
 * match storage. On every other status, out is zeroed and nothing is owned.
 * err receives a short message when non-NULL and err_size is non-zero.
 */
ArgsStatus args_command_parse(const ProgramCmd *command, int argc, char **argv, CmdMatches *out,
                              char *err, size_t err_size);

/**
 * @brief Write usage, arguments, options, and subcommands.
 */
void args_command_write_help(const ProgramCmd *command, FILE *out);

/**
 * @brief Release match storage. NULL is ignored.
 */
void args_matches_free(CmdMatches *matches);

/**
 * @brief True when the argument was given or a default value is available.
 */
bool args_matches_present(const CmdMatches *matches, const char *id);

/**
 * @brief Occurrence count. COUNT arguments return how many times they appeared.
 */
size_t args_matches_count(const CmdMatches *matches, const char *id);

/**
 * @brief Last stored string, or NULL when this id has no string value.
 */
const char *args_matches_get(const CmdMatches *matches, const char *id);

/**
 * @brief All stored strings for an APPEND or SET argument. count may be NULL.
 */
const char *const *args_matches_values(const CmdMatches *matches, const char *id, size_t *count);

bool args_matches_get_bool(const CmdMatches *matches, const char *id, bool *out);
bool args_matches_get_i64(const CmdMatches *matches, const char *id, int64_t *out);
bool args_matches_get_u64(const CmdMatches *matches, const char *id, uint64_t *out);
bool args_matches_get_i32(const CmdMatches *matches, const char *id, int32_t *out);
bool args_matches_get_u32(const CmdMatches *matches, const char *id, uint32_t *out);
bool args_matches_get_u16(const CmdMatches *matches, const char *id, uint16_t *out);
bool args_matches_get_f64(const CmdMatches *matches, const char *id, double *out);

/**
 * @brief Selected subcommand, or NULL.
 */
const ProgramCmd *args_matches_subcommand(const CmdMatches *matches);

/**
 * @brief Parse of the selected subcommand, or NULL.
 */
const CmdMatches *args_matches_child(const CmdMatches *matches);

/**
 * @brief Initializer modifiers for BD_ARG_OPTION.
 *
 * Pass one of these as the last argument. They are designated initializers,
 * so unspecified ProgramArg fields stay zero.
 */
#define BD_ARG_REQUIRED .required = true
#define BD_ARG_OPTIONAL .required = false
#define BD_ARG_DEFAULT(value) .default_value = (value)

#define BD_ARG_FLAG(arg_id, arg_short, arg_long, arg_help)                                         \
    {                                                                                              \
        .id = (arg_id), .short_name = (arg_short), .long_name = (arg_long), .help = (arg_help),    \
        .action = BD_ARG_SET_TRUE, .kind = BD_VALUE_BOOL                                           \
    }

#define BD_ARG_COUNT(arg_id, arg_short, arg_long, arg_help)                                        \
    {                                                                                              \
        .id = (arg_id), .short_name = (arg_short), .long_name = (arg_long), .help = (arg_help),    \
        .action = BD_ARG_COUNT, .kind = BD_VALUE_U64                                               \
    }

#define BD_ARG_POSITIONAL(arg_id, arg_value, arg_help, arg_kind, arg_required)                     \
    {                                                                                              \
        .id = (arg_id), .value_name = (arg_value), .help = (arg_help), .action = BD_ARG_SET,       \
        .kind = (arg_kind), .required = (arg_required), .positional = true                         \
    }

#define BD_ARG_OPTION(arg_id, arg_short, arg_long, arg_help, arg_kind, arg_extra)                  \
    {                                                                                              \
        .id = (arg_id), .short_name = (arg_short), .long_name = (arg_long), .help = (arg_help),    \
        .action = BD_ARG_SET, .kind = (arg_kind), arg_extra                                        \
    }

#define BD_COMMAND(cmd_name, cmd_about, cmd_version, cmd_args)                                     \
    {                                                                                              \
        .name = (cmd_name), .about = (cmd_about), .version = (cmd_version), .args = (cmd_args),    \
        .n_args = sizeof(cmd_args) / sizeof((cmd_args)[0])                                         \
    }

#define BD_COMMAND_WITH_SUBS(cmd_name, cmd_about, cmd_version, cmd_args, cmd_subs)                 \
    {                                                                                              \
        .name = (cmd_name), .about = (cmd_about), .version = (cmd_version), .args = (cmd_args),    \
        .n_args = sizeof(cmd_args) / sizeof((cmd_args)[0]), .subs = (cmd_subs),                    \
        .n_subs = sizeof(cmd_subs) / sizeof((cmd_subs)[0])                                         \
    }

#endif /* BEDROCK_ARGS_H */
