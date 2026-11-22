// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Command-line walking and declarative argument parsing.
 */

#ifndef BEDROCK_ARGS_H
#define BEDROCK_ARGS_H

#include "bedrock/config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Cursor over argc and argv.
 *
 * `index` starts at 1. `program` is argv[0], or `"program"` when argc is 0.
 */
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
BED_API void args_init(ProgramArgs *it, int argc, char **argv);

/**
 * @brief Whether all arguments have been consumed.
 *
 * @param it Argument walker.
 * @return true if it is NULL/invalid or index is past argc; false otherwise.
 */
BED_API bool args_done(const ProgramArgs *it);

/**
 * @brief Current argument without advancing.
 *
 * @param it Argument walker.
 * @return argv[index], or NULL when done.
 */
BED_API const char *args_current(const ProgramArgs *it);

/**
 * @brief Take the current argument and advance the index.
 *
 * @param it Argument walker; advanced when an argument is taken.
 * @return The taken argument, or NULL when done.
 */
BED_API const char *args_take(ProgramArgs *it);

/**
 * @brief If current argv matches short or long option, consume it.
 *
 * @param it        Argument walker; advanced on match.
 * @param short_opt Optional short form (e.g. "-h"); may be NULL.
 * @param long_opt  Optional long form (e.g. "--help"); may be NULL.
 * @return true if matched and consumed; false otherwise.
 */
BED_API bool args_match(ProgramArgs *it, const char *short_opt, const char *long_opt);

/**
 * @brief True if str is non-empty and consists only of decimal digits.
 *
 * @param str String to test; may be NULL.
 * @return true if str is non-empty and digits-only; false otherwise.
 */
BED_API bool args_is_digits(const char *str);

/**
 * @brief Parse an unsigned 16-bit decimal value from str into out.
 *
 * @param str Digit string to parse; may be NULL.
 * @param out Receives the parsed value on success; must not be NULL.
 * @return true on success; false on NULL, empty, non-digits, overflow, or NULL
 *         out.
 */
BED_API bool args_parse_u16(const char *str, uint16_t *out);

/**
 * @brief Count ':' characters in str.
 *
 * @param str String to scan; may be NULL.
 * @return Number of ':' characters, or 0 if str is NULL.
 */
BED_API int args_colon_count(const char *str);

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
BED_API bool args_split_host_port(const char *spec, char *host, size_t host_size, uint16_t *port);

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
    const char *id;              /**< Name used by getters and by conflicts and requires. */
    char short_name;             /**< Short-option character, without '-'. 0 omits it. Ignored when positional. */
    const char *long_name;       /**< Long-option name, without dashes. NULL omits it. */
    const char *help;            /**< Help text. NULL prints a blank description. */
    const char *value_name;      /**< Placeholder name in help. NULL uses id. */
    const char *default_value;   /**< Default text. NULL if none. */
    const char *const *possible; /**< NULL-terminated exact values. NULL skips this check. Not copied. */
    const char *conflicts;       /**< Conflicting argument id, or NULL. */
    const char *requires;        /**< Required companion argument id, or NULL. */
    ArgAction action;            /**< What happens when this argument appears. */
    ArgValueKind kind;           /**< Type a value must parse as before it is stored. */
    bool required;               /**< When true, an omitted argument fails unless default_value was stored. */
    bool positional;             /**< True when this is a positional argument. */
} ProgramArg;

/**
 * @brief A command and its direct subcommands.
 *
 * subs points at an array of the same struct, so nesting is recursive.
 * --help is recognized automatically. --version is recognized when version
 * is not NULL, unless the command already defines those names.
 */
typedef struct ProgramCmd {
    const char *name;              /**< Command name in usage and as a subcommand token. */
    const char *about;             /**< Help description. NULL or empty omits the paragraph under the usage line. */
    const char *version;           /**< Version text. NULL disables --version. */
    const ProgramArg *args;        /**< Caller-owned arguments. Not copied. NULL when n_args is 0. */
    size_t n_args;                 /**< Number of elements in args. */
    const struct ProgramCmd *subs; /**< Caller-owned direct subcommands. Not copied. NULL when n_subs is 0. */
    size_t n_subs;                 /**< Number of elements in subs. */
} ProgramCmd;

/**
 * @brief Values captured for one argument. Owned by CmdMatches.
 */
typedef struct ArgMatch {
    const ProgramArg *spec; /**< Schema entry in the command's args array. */
    bool explicit_present;  /**< True when the user passed this argument. */
    size_t count;           /**< Occurrence count, or number of stored values. */
    const char **values;    /**< Stored strings, or NULL when empty. */
    size_t n_values;        /**< Number of pointers stored in values. */
    size_t capacity;        /**< Allocated pointer slots of values. May exceed n_values. */
} ArgMatch;

/**
 * @brief Successful parse of one command.
 *
 * String values point at argv or at default literals. child owns the parse of
 * the selected subcommand. Release the whole tree with args_matches_free.
 */
typedef struct CmdMatches {
    const ProgramCmd *command;    /**< Schema parsed into this node. Not owned. */
    const ProgramCmd *subcommand; /**< Selected direct subcommand, or NULL when none was chosen. */
    struct CmdMatches *child;     /**< Parse of the selected subcommand, or NULL. */
    ArgMatch *items;              /**< Match for each argument, or NULL when empty. */
    size_t n_items;               /**< Length of items. Equal to the schema's n_args. */
} CmdMatches;

/**
 * @brief Flag that store true when present.
 *
 * @param id         Argument id used by the match getters.
 * @param short_name Short option character. 0 omits the short form.
 * @param long_name  Long option name, without leading dashes. NULL omits it.
 * @param help       Help text. May be NULL.
 *
 * @return A `ProgramArg` with action `BD_ARG_SET_TRUE` and kind `BD_VALUE_BOOL`.
 */
BED_API ProgramArg args_flag(const char *id, char short_name,
                             const char *long_name, const char *help);

/**
 * @brief Option that stores one value of the given kind.
 *
 * @param id         Argument id used by the match getters.
 * @param short_name Short option character. 0 omits the short form.
 * @param long_name  Long option name, without leading dashes. NULL omits it.
 * @param help       Help text. May be NULL.
 * @param kind       How the stored text is parsed by the typed getters.
 *
 * @return A `ProgramArg` with action `BD_ARG_SET`.
 */
BED_API ProgramArg args_option(const char *id, char short_name, const char *long_name,
                               const char *help, ArgValueKind kind);

/**
 * @brief Positional argument that stores one value.
 *
 * `required` stays false. Call `args_required` to change it.
 *
 * @param id         Argument id used by the match getters.
 * @param value_name Name shown in the usage line. NULL falls back to `id`.
 * @param help       Help text. May be NULL.
 * @param kind       How the stored text is parsed by the typed getters.
 *
 * @return A positional `ProgramArg` with action `BD_ARG_SET`.
 */
BED_API ProgramArg args_positional(const char *id, const char *value_name,
                                   const char *help, ArgValueKind kind);

/**
 * @brief Flag that counts occurrences.
 *
 * @param id         Argument id used by the match getters.
 * @param short_name Short option character. 0 omits the short form.
 * @param long_name  Long option name, without leading dashes. NULL omits it.
 * @param help       Help text. May be NULL.
 *
 * @return A `ProgramArg` with action `BD_ARG_COUNT` and kind `BD_VALUE_U64`.
 */
BED_API ProgramArg args_count(const char *id, char short_name,
                              const char *long_name, const char *help);

/**
 * @brief Mark an argument required. NULL is ignored.
 *
 * @param arg      Argument to update. NULL is ignored.
 * @param required true when the parse must see this argument.
 */
BED_API void args_required(ProgramArg *arg, bool required);

/**
 * @brief Set the default text used when the argument is absent.
 *
 * @param arg   Argument to update. NULL is ignored.
 * @param value Default text. Not copied. NULL clears the default.
 */
BED_API void args_default(ProgramArg *arg, const char *value);

/**
 * @brief Restrict the value to a NULL-terminated list. The list is not copied.
 *
 * @param arg    Argument to update. NULL is ignored.
 * @param values NULL-terminated list of allowed text. Not copied. NULL clears it.
 */
BED_API void args_possible(ProgramArg *arg, const char *const *values);

/**
 * @brief Reject a parse where both this argument and id were given.
 *
 * @param arg Argument to update. NULL is ignored.
 * @param id  Identifier of the argument that conflicts with `arg`. Not copied.
 */
BED_API void args_conflicts(ProgramArg *arg, const char *id);

/**
 * @brief Require id to be present whenever this argument is given.
 *
 * @param arg Argument to update. NULL is ignored.
 * @param id  Identifier that must also be present. Not copied.
 */
BED_API void args_requires(ProgramArg *arg, const char *id);

/**
 * @brief Parse argv against a command.
 *
 * argv[0] is the program name and is not an argument. On BD_ARGS_OK, out owns
 * match storage. On every other status, out is zeroed and nothing is owned.
 * err receives a short message when non-NULL and err_size is non-zero.
 *
 * @param command  Command to parse. NULL is invalid.
 * @param argc     Argument count, including the program name.
 * @param argv     Argument vector. NULL is invalid when `argc` is positive.
 * @param out      Receives the matches on success. NULL discards a successful parse.
 * @param err      Optional buffer for a short error message.
 * @param err_size Capacity of `err`, including the terminating NUL.
 *
 * @return `BD_ARGS_OK` on success. Any other status leaves `out` zeroed.
 */
BED_API ArgsStatus args_command_parse(const ProgramCmd *command, int argc, char **argv, CmdMatches *out,
                              char *err, size_t err_size);

/**
 * @brief Write usage, arguments, options, and subcommands.
 *
 * @param command Command to describe. NULL is ignored.
 * @param out     Stream to write. NULL is ignored.
 */
BED_API void args_command_write_help(const ProgramCmd *command, FILE *out);

/**
 * @brief Release match storage. NULL is ignored.
 * @param matches Matches to release, including the child parse. NULL is ignored.
 */
BED_API void args_matches_free(CmdMatches *matches);

/**
 * @brief True when the argument was given or a default value is available.
 *
 * @param matches Parse to query. NULL is not present.
 * @param id      Argument id.
 *
 * @return true when the argument was given or a stored value is available.
 */
BED_API bool args_matches_present(const CmdMatches *matches, const char *id);

/**
 * @brief Occurrence count. COUNT arguments return how many times they appeared.
 *
 * @param matches Parse to query. NULL counts as zero.
 * @param id      Argument id.
 *
 * @return How many times a COUNT argument appeared, or how many values another
 *         argument stored. 0 when `id` is absent.
 */
BED_API size_t args_matches_count(const CmdMatches *matches, const char *id);

/**
 * @brief Last stored string, or NULL when this id has no string value.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 *
 * @return Last stored string, or NULL when this id has no string value.
 */
BED_API const char *args_matches_get(const CmdMatches *matches, const char *id);

/**
 * @brief All stored strings for an APPEND or SET argument. count may be NULL.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param count   Receives the number of strings. May be NULL.
 *
 * @return The stored strings, or NULL when this id has none. `count` is 0 then.
 */
BED_API const char *const *args_matches_values(const CmdMatches *matches, const char *id, size_t *count);

/**
 * @brief Read a boolean argument.
 *
 * `BD_ARG_SET_TRUE` stores whether the flag was present. `BD_ARG_SET_FALSE`
 * stores the opposite. Any other action parses the last stored string with
 * `bd_parse_bool`.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when `id` is absent, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_bool(const CmdMatches *matches, const char *id, bool *out);

/**
 * @brief Parse the last stored string as a signed 64-bit integer.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when there is no string, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_i64(const CmdMatches *matches, const char *id, int64_t *out);

/**
 * @brief Read an unsigned 64-bit argument.
 *
 * A COUNT argument stores how many times it appeared. Any other action parses
 * the last stored string with `bd_parse_u64`.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when `id` is absent, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_u64(const CmdMatches *matches, const char *id, uint64_t *out);

/**
 * @brief Parse the last stored string as a signed 32-bit integer.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when there is no string, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_i32(const CmdMatches *matches, const char *id, int32_t *out);

/**
 * @brief Parse the last stored string as an unsigned 32-bit integer.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when there is no string, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_u32(const CmdMatches *matches, const char *id, uint32_t *out);

/**
 * @brief Parse the last stored string as an unsigned 16-bit integer.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when there is no string, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_u16(const CmdMatches *matches, const char *id, uint16_t *out);

/**
 * @brief Parse the last stored string as a floating-point value.
 *
 * @param matches Parse to query.
 * @param id      Argument id.
 * @param out     Receives the value. Must not be NULL.
 *
 * @return true on success. false when there is no string, `out` is NULL, or the
 *         text does not parse.
 */
BED_API bool args_matches_get_f64(const CmdMatches *matches, const char *id, double *out);

/**
 * @brief Selected subcommand, or NULL.
 * @param matches Parse to query. NULL has no subcommand.
 * @return Selected subcommand, or NULL.
 */
BED_API const ProgramCmd *args_matches_subcommand(const CmdMatches *matches);

/**
 * @brief Parse of the selected subcommand, or NULL.
 * @param matches Parse to query. NULL has no child.
 * @return Parse of the selected subcommand, or NULL.
 */
BED_API const CmdMatches *args_matches_child(const CmdMatches *matches);

/**
 * @brief Designated initializer that marks the option required.
 */
#define BD_ARG_REQUIRED .required = true

/**
 * @brief Designated initializer that marks the option optional.
 */
#define BD_ARG_OPTIONAL .required = false

/**
 * @brief Designated initializer that sets the default text.
 * @param value Default text. Not copied.
 */
#define BD_ARG_DEFAULT(value) .default_value = (value)

/**
 * @brief Compound literal for a flag that stores true when present.
 *
 * @param arg_id    Argument id.
 * @param arg_short Short option character. 0 omits the short form.
 * @param arg_long  Long option name, without leading dashes. NULL omits it.
 * @param arg_help  Help text. May be NULL.
 */
#define BD_ARG_FLAG(arg_id, arg_short, arg_long, arg_help)                                     \
{                                                                                              \
    .id = (arg_id), .short_name = (arg_short), .long_name = (arg_long), .help = (arg_help),    \
    .action = BD_ARG_SET_TRUE, .kind = BD_VALUE_BOOL                                           \
}

/**
 * @brief Compound literal for a flag that counts occurrences.
 *
 * @param arg_id    Argument id.
 * @param arg_short Short option character. 0 omits the short form.
 * @param arg_long  Long option name, without leading dashes. NULL omits it.
 * @param arg_help  Help text. May be NULL.
 */
#define BD_ARG_COUNT(arg_id, arg_short, arg_long, arg_help)                                    \
{                                                                                              \
    .id = (arg_id), .short_name = (arg_short), .long_name = (arg_long), .help = (arg_help),    \
    .action = BD_ARG_COUNT, .kind = BD_VALUE_U64                                               \
}

/**
 * @brief Compound literal for a positional argument.
 *
 * @param arg_id       Argument id.
 * @param arg_value    Name shown in the usage line.
 * @param arg_help     Help text. May be NULL.
 * @param arg_kind     How the stored text is parsed.
 * @param arg_required true when the parse must see this argument.
 */
#define BD_ARG_POSITIONAL(arg_id, arg_value, arg_help, arg_kind, arg_required)                 \
{                                                                                              \
    .id = (arg_id), .value_name = (arg_value), .help = (arg_help), .action = BD_ARG_SET,       \
    .kind = (arg_kind), .required = (arg_required), .positional = true                         \
}

/**
 * @brief Compound literal for an option that stores one value.
 *
 * @param arg_id    Argument id.
 * @param arg_short Short option character. 0 omits the short form.
 * @param arg_long  Long option name, without leading dashes. NULL omits it.
 * @param arg_help  Help text. May be NULL.
 * @param arg_kind  How the stored text is parsed.
 * @param arg_extra Designated initializer, such as `BD_ARG_REQUIRED` or
 *                  `BD_ARG_DEFAULT`. Unspecified `ProgramArg` fields stay zero.
 */
#define BD_ARG_OPTION(arg_id, arg_short, arg_long, arg_help, arg_kind, arg_extra)              \
{                                                                                              \
    .id = (arg_id), .short_name = (arg_short), .long_name = (arg_long), .help = (arg_help),    \
    .action = BD_ARG_SET, .kind = (arg_kind), arg_extra                                        \
}

/**
 * @brief Compound literal for a command.
 *
 * `cmd_args` must be an array. The macro counts it with `sizeof`.
 *
 * @param cmd_name    Command name.
 * @param cmd_about   One-line description. May be NULL.
 * @param cmd_version Version string. NULL hides automatic `--version`.
 * @param cmd_args    Array of `ProgramArg`.
 */
#define BD_COMMAND(cmd_name, cmd_about, cmd_version, cmd_args)                                 \
{                                                                                              \
    .name = (cmd_name), .about = (cmd_about), .version = (cmd_version), .args = (cmd_args),    \
    .n_args = sizeof(cmd_args) / sizeof((cmd_args)[0])                                         \
}

/**
 * @brief Compound literal for a command and its direct subcommands.
 *
 * `cmd_args` and `cmd_subs` must be arrays. The macro counts each with `sizeof`.
 *
 * @param cmd_name    Command name.
 * @param cmd_about   One-line description. May be NULL.
 * @param cmd_version Version string. NULL hides automatic `--version`.
 * @param cmd_args    Array of `ProgramArg`.
 * @param cmd_subs    Array of `ProgramCmd`.
 */
#define BD_COMMAND_WITH_SUBS(cmd_name, cmd_about, cmd_version, cmd_args, cmd_subs)             \
{                                                                                              \
    .name = (cmd_name), .about = (cmd_about), .version = (cmd_version), .args = (cmd_args),    \
    .n_args = sizeof(cmd_args) / sizeof((cmd_args)[0]), .subs = (cmd_subs),                    \
    .n_subs = sizeof(cmd_subs) / sizeof((cmd_subs)[0])                                         \
}

#endif /* BEDROCK_ARGS_H */
