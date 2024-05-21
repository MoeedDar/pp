#ifndef PP_H
#define PP_H

#include "aa.h"

typedef struct pp_parser pp_parser_t;

// state

typedef struct {
  const char* input;
  int pos;
} pp_state_t;

// result

typedef enum {
  PP_OK,
  PP_ERROR_UNEXPECTED_TOK,
  PP_ERROR_UNKNOWN_OP,
} pp_status_t;

typedef enum {
  PP_OUTPUT_NONE,
  PP_OUTPUT_CHAR,
  PP_OUTPUT_STRING,
  PP_OUTPUT_ARRAY,
} pp_output_type_t;

typedef struct pp_output pp_output_t;

struct pp_output {
  pp_output_type_t type;
  union {
    void* none;
    char chr;
    const char* string;
    struct {
      int len;
      pp_output_t* values;
    } array;
  } output;
};

typedef struct {
  int pos;
  pp_status_t status;
  pp_output_t output;
  const char* rest;
} pp_result_t;

// ops

typedef enum {
  PP_OP_PURE,
  PP_OP_FAIL,
  PP_OP_EOF,
  PP_OP_EXPECT,
  PP_OP_SELECT,
  PP_OP_CHAR,
  PP_OP_STRING,
  PP_OP_STRING_NO_CASE,
  PP_OP_ANY_OF,
  PP_OP_NONE_OF,
  PP_OP_OPTIONAL,
  PP_OP_CHOICE,
  PP_OP_MANY,
  PP_OP_SEQUENCE,
  PP_OP_MAP,
  PP_OP_TAP,
} pp_op_t;

// op data

typedef struct {
} pp_pure_t;

typedef struct {
} pp_fail_t;

typedef struct {
} pp_eof_t;

typedef struct {
  char c;
} pp_expect_t;

typedef struct {
  char c;
} pp_char_t;

typedef struct {
  const char* string;
} pp_string_t;

typedef struct {
  const char* string;
} pp_string_no_case_t;

typedef struct {
  const char* chars;
} pp_any_of_t;

typedef struct {
  const char* chars;
} pp_none_of_t;

typedef struct {
  pp_parser_t* parser;
} pp_optional_t;

typedef struct {
  int num_parsers;
  pp_parser_t** parsers;
} pp_choice_t;

typedef struct {
  pp_parser_t* parser;
} pp_many_t;

typedef struct {
  int num_parsers;
  pp_parser_t** parsers;
} pp_sequence_t;

typedef struct {
  pp_parser_t* parser;
  pp_output_t (*map)(pp_output_t, void*);
  void* arg;
} pp_map_t;

typedef struct {
  pp_parser_t* parser;
  void (*tap)(pp_output_t, void*);
  void* arg;
} pp_tap_t;

typedef union {
  pp_pure_t pure;
  pp_fail_t fail;
  pp_expect_t expect;
  pp_char_t chr;
  pp_string_t string;
  pp_string_no_case_t string_no_case;
  pp_any_of_t any_of;
  pp_none_of_t none_of;
  pp_optional_t optional;
  pp_choice_t choice;
  pp_many_t many;
  pp_sequence_t sequence;
  pp_map_t map;
  pp_tap_t tap;
} pp_op_data_t;

// parser

struct pp_parser {
  pp_op_t op;
  pp_op_data_t data;
};

// allocation

void pp_init_default_allocator();
void pp_deinit_default_allocator();
void pp_set_default_allocator();
void pp_set_allocator(aa_sweeper_t sweeper);
void* pp_alloc(size_t size);
void pp_sweep();
char* pp_strdup(const char* str);
char* pp_strndup(const char* str, size_t len);

// parser

pp_result_t pp_parse(pp_parser_t* parser, const char* input);
pp_parser_t* pp_init_parser();

// state

pp_state_t pp_init_state(const char* input, int pos);

// combinators

pp_parser_t* pp_pure();
pp_parser_t* pp_fail();
pp_parser_t* pp_eof();
pp_parser_t* pp_expect(char c);
pp_parser_t* pp_char(char c);
pp_parser_t* pp_string(const char* string);
pp_parser_t* pp_string_no_case(const char* string);
pp_parser_t* pp_any_of(const char* chars);
pp_parser_t* pp_none_of(const char* chars);
pp_parser_t* pp_optional(pp_parser_t* parser);
pp_parser_t* pp_choice(int num_parsers, pp_parser_t** parsers);
pp_parser_t* pp_many(pp_parser_t* parser);
pp_parser_t* pp_sequence(int num_parsers, pp_parser_t** parsers);
pp_parser_t*
pp_map(pp_parser_t* parser, pp_output_t (*map)(pp_output_t, void*), void* arg);
pp_parser_t*
pp_tap(pp_parser_t* parser, void (*tap)(pp_output_t, void*), void* arg);

// higher order parsers

pp_parser_t* pp_skip(pp_parser_t* parser);
pp_parser_t* pp_concat_string(int num_parsers, pp_parser_t** parsers);
pp_parser_t* pp_concat_array(int num_parsers, pp_parser_t** parsers);
pp_parser_t* pp_select(pp_parser_t* parser, int pos);
pp_parser_t* pp_whitespace();
pp_parser_t* pp_alpha();
pp_parser_t* pp_alphanumeric_or_underscore();
pp_parser_t* pp_skip_whitespace();
pp_parser_t* pp_whitespace_delimited(pp_parser_t* parser);
pp_parser_t* pp_separated_list(pp_parser_t* item, pp_parser_t* separator);
pp_parser_t* pp_comma_separated_list(pp_parser_t* parser);
pp_parser_t* pp_copy_string_ref(pp_parser_t* parser, const char** str_ref);
// this function requires a reference to an array containing a length and a
// string in the given order
pp_parser_t* pp_copy_string_array_ref(pp_parser_t* parser, void* len_arr_ref);

#endif // PP_H