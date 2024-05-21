#include "pp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_REGION_SIZE 8192

static aa_sweeper_t allocator;
static aa_sweeper_t default_allocator;
static aa_arena_t default_arena;

static pp_result_t parse(pp_parser_t* parser, pp_state_t state);

static pp_output_t none();
static pp_output_t chr(char chr);
static pp_output_t string(int len, const char* string);
static pp_output_t array(int len, pp_output_t* values);

static pp_result_t ok(int pos, pp_output_t output, const char* rest);
static pp_result_t err(int pos, pp_status_t status);

static pp_output_t skip(pp_output_t output, void* arg);
static pp_output_t concat_string(pp_output_t output, void* arg);
static pp_output_t concat_array(pp_output_t output, void* arg);
static pp_output_t select(pp_output_t output, void* arg);
static void copy_string_ref(pp_output_t output, void* arg);
static void copy_string_array_ref(pp_output_t output, void* arg);

void pp_init_default_allocator() {
  default_arena = aa_arena_init(ARENA_REGION_SIZE);
  default_allocator = aa_arena_make_sweeper(&default_arena);
  pp_set_default_allocator();
}

void pp_deinit_default_allocator() {
  aa_arena_deinit(&default_arena);
}

void pp_set_default_allocator() {
  allocator = default_allocator;
}

void pp_set_allocator(aa_sweeper_t new_allocator) {
  allocator = new_allocator;
}

void* pp_alloc(size_t size) {
  return aa_sweeper_alloc(&allocator, size);
}

void pp_sweep() {
  aa_sweeper_sweep(&allocator);
}

char* pp_strdup(const char* str) {
  size_t len = strlen(str) + 1;
  char* new_str = (char*)pp_alloc(len);
  strcpy(new_str, str);
  return new_str;
}

char* pp_strndup(const char* str, size_t len) {
  char* new_str = (char*)pp_alloc(len + 1);
  strncpy(new_str, str, len);
  new_str[len] = '\0';
  return new_str;
}

pp_result_t pp_parse(pp_parser_t* parser, const char* input) {
  const pp_state_t state = pp_init_state(input, 0);
  return parse(parser, state);
}

pp_parser_t* pp_init_parser() {
  pp_parser_t* p = pp_alloc(sizeof(pp_parser_t));
  if (p == NULL) {
    // idgaf honestly.
  }
  return p;
}

pp_state_t pp_init_state(const char* input, int pos) {
  return (pp_state_t){.input = input, .pos = pos};
}

pp_parser_t* pp_pure() {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_PURE;
  return p;
}

pp_parser_t* pp_fail() {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_FAIL;
  return p;
}

pp_parser_t* pp_eof() {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_EOF;
  return p;
}

pp_parser_t* pp_expect(char c) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_EXPECT;
  p->data.expect.c = c;
  return p;
}

pp_parser_t* pp_char(char c) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_STRING;
  p->data.chr.c = c;
  return p;
}

pp_parser_t* pp_string(const char* tag) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_STRING;
  p->data.string.string = pp_strdup(tag);
  return p;
}

pp_parser_t* pp_string_no_case(const char* tag) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_STRING_NO_CASE;
  p->data.string.string = pp_strdup(tag);
  return p;
}

pp_parser_t* pp_any_of(const char* chars) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_ANY_OF;
  p->data.any_of.chars = pp_strdup(chars);
  return p;
}

pp_parser_t* pp_none_of(const char* chars) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_NONE_OF;
  p->data.none_of.chars = pp_strdup(chars);
  return p;
}

pp_parser_t* pp_optional(pp_parser_t* parser) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_OPTIONAL;
  p->data.optional.parser = parser;
  return p;
}

pp_parser_t* pp_choice(int num_parsers, pp_parser_t** parsers) {
  pp_parser_t* p = pp_init_parser();
  pp_parser_t** parsers_copy = pp_alloc(num_parsers * sizeof(void*));
  memcpy(parsers_copy, parsers, num_parsers * sizeof(void*));
  p->op = PP_OP_CHOICE;
  p->data.choice.num_parsers = num_parsers;
  p->data.choice.parsers = parsers_copy;
  return p;
}

pp_parser_t* pp_many(pp_parser_t* parser) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_MANY;
  p->data.many.parser = parser;
  return p;
}

pp_parser_t* pp_sequence(int num_parsers, pp_parser_t** parsers) {
  pp_parser_t* p = pp_init_parser();
  pp_parser_t** parsers_copy = pp_alloc(num_parsers * sizeof(void*));
  memcpy(parsers_copy, parsers, num_parsers * sizeof(void*));
  p->op = PP_OP_SEQUENCE;
  p->data.sequence.num_parsers = num_parsers;
  p->data.sequence.parsers = parsers_copy;
  return p;
}

pp_parser_t*
pp_map(pp_parser_t* parser, pp_output_t (*map)(pp_output_t, void*), void* arg) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_MAP;
  p->data.map.parser = parser;
  p->data.map.map = map;
  p->data.map.arg = arg;
  return p;
}

pp_parser_t*
pp_tap(pp_parser_t* parser, void (*tap)(pp_output_t, void*), void* arg) {
  pp_parser_t* p = pp_init_parser();
  p->op = PP_OP_TAP;
  p->data.tap.parser = parser;
  p->data.tap.tap = tap;
  p->data.tap.arg = arg;
  return p;
}

pp_parser_t* pp_skip(pp_parser_t* parser) {
  return pp_map(parser, skip, NULL);
}

pp_parser_t* pp_concat_string(int num_parsers, pp_parser_t** parsers) {
  return pp_map(pp_sequence(num_parsers, parsers), concat_string, NULL);
}

pp_parser_t* pp_concat_array(int num_parsers, pp_parser_t** parsers) {
  return pp_map(pp_sequence(num_parsers, parsers), concat_array, NULL);
}

pp_parser_t* pp_select(pp_parser_t* parser, int pos) {
  return pp_map(parser, select, (void*)(long long)1);
}

pp_parser_t* pp_whitespace() {
  return pp_many(pp_any_of(" \t\n\r"));
}

pp_parser_t* pp_alpha() {
  return pp_any_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
}

pp_parser_t* pp_alphanumeric_or_underscore() {
  return pp_any_of(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"
  );
}

pp_parser_t* pp_skip_whitespace() {
  return pp_map(pp_whitespace(), skip, NULL);
}

pp_parser_t* pp_whitespace_delimited(pp_parser_t* parser) {
  return pp_select(
    pp_sequence(
      3,
      (pp_parser_t*[]){
        pp_skip_whitespace(),
        parser,
        pp_skip_whitespace(),
      }
    ),
    2
  );
}

pp_parser_t* pp_separated_list(pp_parser_t* item, pp_parser_t* separator) {
  return pp_concat_array(
    2,
    (pp_parser_t*[]){
      item,
      pp_many(pp_select(
        pp_sequence(
          2,
          (pp_parser_t*[]){
            separator,
            item,
          }
        ),
        0
      )),
    }
  );
}

pp_parser_t* pp_comma_separated_list(pp_parser_t* parser) {
  return pp_separated_list(parser, pp_string(","));
}

pp_parser_t* pp_copy_string_ref(pp_parser_t* parser, const char** str_ref) {
  return pp_tap(parser, copy_string_ref, (void*)str_ref);
}

pp_parser_t* pp_copy_string_array_ref(pp_parser_t* parser, void* len_arr_ref) {
  return pp_tap(parser, copy_string_array_ref, (void*)len_arr_ref);
}

static pp_result_t parse(pp_parser_t* parser, pp_state_t state) {
  const char* input = state.input;
  const int pos = state.pos;
  const int input_len = strlen(input);

  switch (parser->op) {
  case PP_OP_PURE:
    return ok(pos, none(), input + pos);

  case PP_OP_EOF:
    if (input[pos] == '\0')
      return ok(pos, none(), input + pos);
    break;

  case PP_OP_EXPECT:
    if (input[pos] == parser->data.expect.c)
      return ok(pos, none(), input + pos + 1);
    break;

  case PP_OP_CHAR:
    if (input[pos] == parser->data.chr.c)
      return ok(pos + 1, chr(input[pos]), input + pos + 1);
    break;

  case PP_OP_STRING: {
    const char* str = parser->data.string.string;
    const size_t len = strlen(str);
    if (strncmp(input + pos, str, len) == 0)
      return ok(pos + len, string(len, &input[pos]), input + pos + len);
    break;
  }

  case PP_OP_STRING_NO_CASE: {
    const char* str = parser->data.string.string;
    const size_t len = strlen(str);
    if (strncasecmp(input + pos, str, len) == 0)
      return ok(pos + len, string(len, &input[pos]), input + pos + len);
    break;
  }

  case PP_OP_ANY_OF: {
    const char* chars = parser->data.any_of.chars;
    if (strchr(chars, input[pos]) != NULL)
      return ok(pos + 1, chr(input[pos]), input + pos + 1);
    break;
  }

  case PP_OP_NONE_OF: {
    const char* chars = parser->data.none_of.chars;
    if (strchr(chars, input[pos]) == NULL)
      return ok(pos + 1, chr(input[pos]), input + pos + 1);
    break;
  }

  case PP_OP_OPTIONAL: {
    pp_result_t result = parse(parser->data.optional.parser, state);
    if (result.status == PP_ERROR_UNEXPECTED_TOK)
      return ok(pos, none(), input + pos);
    else
      return result;
  }

  case PP_OP_CHOICE: {
    for (size_t i = 0; i < parser->data.choice.num_parsers; ++i) {
      pp_parser_t* p = parser->data.choice.parsers[i];
      pp_result_t result = parse(p, state);
      if (result.status == PP_OK) {
        return result;
      }
    }
    break;
  }

  case PP_OP_MANY: {
    int len = 0;
    int max_len = 4096;
    // using C malloc because I am lazy. Could use linked list.
    // Also this creates less garbage on the arena.
    pp_output_t* outputs = malloc(max_len * sizeof(pp_output_t*));

    while (state.pos < input_len) {
      const pp_result_t result = parse(parser->data.many.parser, state);
      if (result.status != PP_OK) {
        break;
      }

      if (len >= max_len) {
        outputs = realloc(outputs, max_len * 2 * sizeof(pp_output_t*));
        max_len = max_len * 2;
      }

      outputs[len++] = result.output;
      state.pos = result.pos;
    }

    const pp_result_t result = ok(state.pos, array(len, outputs), input + pos);
    free(outputs);
    return result;
  }

  case PP_OP_SEQUENCE: {
    int num_parsers = parser->data.sequence.num_parsers;
    pp_parser_t** parsers = parser->data.sequence.parsers;
    pp_output_t* outputs = malloc(num_parsers * sizeof(pp_output_t));

    for (int i = 0; i < num_parsers; ++i) {
      pp_parser_t* p = parsers[i];
      pp_result_t result = parse(p, state);

      if (result.status != PP_OK) {
        free(outputs);
        return err(state.pos, result.status);
      }

      outputs[i] = result.output;
      state.pos = result.pos;
    }

    // we can make a non copying array function and just pass the allocated
    // array. this avoids malloc
    pp_result_t result =
      ok(state.pos, array(num_parsers, outputs), input + state.pos);
    free(outputs);
    return result;
  }
  case PP_OP_MAP: {
    pp_result_t result = parse(parser->data.tap.parser, state);
    if (result.status == PP_OK) {
      result.output = parser->data.map.map(result.output, parser->data.map.arg);
    }
    return result;
  }
  case PP_OP_TAP: {
    pp_result_t result = parse(parser->data.tap.parser, state);
    if (result.status == PP_OK) {
      parser->data.tap.tap(result.output, parser->data.tap.arg);
    }
    return result;
  }
  default:
    return err(pos, PP_ERROR_UNKNOWN_OP);
  }
  return err(pos, PP_ERROR_UNEXPECTED_TOK);
}

static pp_output_t none() {
  return (pp_output_t){.type = PP_OUTPUT_NONE, .output.none = NULL};
}

static pp_output_t chr(char chr) {
  return (pp_output_t){.type = PP_OUTPUT_CHAR, .output.chr = chr};
}

static pp_output_t string(int len, const char* string) {
  return (pp_output_t){
    .type = PP_OUTPUT_STRING,
    .output.string = pp_strndup(string, len),
  };
}

static pp_output_t array(int len, pp_output_t* values) {
  void* ptr = pp_alloc(sizeof(pp_output_t) * len);
  memcpy(ptr, values, sizeof(pp_output_t) * len);
  return (pp_output_t){
    .type = PP_OUTPUT_ARRAY,
    .output.array = {.len = len, .values = ptr},
  };
}

static pp_result_t ok(int pos, pp_output_t output, const char* rest) {
  return (pp_result_t){
    .pos = pos,
    .status = PP_OK,
    .output = output,
    .rest = rest,
  };
}

static pp_result_t err(int pos, pp_status_t status) {
  return (pp_result_t){
    .pos = pos,
    .status = status,
  };
}

static pp_output_t skip(pp_output_t output, void* arg) {
  output.type = PP_OUTPUT_NONE;
  output.output.none = NULL;
  return output;
}

// TODO: this should not concatenate recursively
static pp_output_t concat_string(pp_output_t output, void* arg) {
  switch (output.type) {
  case PP_OUTPUT_NONE:
    return string(0, "");
  case PP_OUTPUT_CHAR: {
    char str[] = {output.output.chr, '\0'};
    return string(1, str);
  }
  case PP_OUTPUT_STRING:
    return output;
  case PP_OUTPUT_ARRAY: {
    int len = 0;
    for (int i = 0; i < output.output.array.len; ++i) {
      pp_output_t ele = ((pp_output_t*)output.output.array.values)[i];
      len += strlen(concat_string(ele, NULL).output.string);
    }

    char* result = (char*)pp_alloc(len + 1);
    int offset = 0;
    for (int i = 0; i < output.output.array.len; ++i) {
      pp_output_t ele = ((pp_output_t*)output.output.array.values)[i];
      pp_output_t concat_ele = concat_string(ele, NULL);
      strcpy(result + offset, concat_ele.output.string);
      offset += strlen(concat_ele.output.string);
    }

    result[offset] = '\0';
    return string(len, result);
  }
  default:
    return none();
  }
}

static pp_output_t concat_array(pp_output_t output, void* arg) {
  if (output.type != PP_OUTPUT_ARRAY) {
    return output;
  }

  int len = 0;
  for (int i = 0; i < output.output.array.len; i++) {
    const pp_output_t ele = output.output.array.values[i];
    if (ele.type == PP_OUTPUT_ARRAY) {
      len += ele.output.array.len;
    } else {
      len++;
    }
  }

  pp_output_t* values = pp_alloc(len * sizeof(pp_output_t));
  for (int i = 0; i < len;) {
    const pp_output_t ele = output.output.array.values[i];
    switch (ele.type) {
    case PP_OUTPUT_ARRAY:
      memcpy(
        &values[i], ele.output.array.values,
        ele.output.array.len * sizeof(pp_output_t)
      );
      i += ele.output.array.len;
      break;
    default:
      values[i++] = ele;
      break;
    }
  }

  return array(len, values);
}

static pp_output_t select(pp_output_t output, void* arg) {
  if (output.type != PP_OUTPUT_ARRAY) {
    return output;
  }
  int pos = (int)(long long)arg;
  output = output.output.array.values[pos];
  return output;
}

static void copy_string_ref(pp_output_t output, void* arg) {
  const char** ref = (const char**)arg;
  if (output.type == PP_OUTPUT_STRING) {
    *ref = output.output.string;
  } else {
    *ref = NULL;
  }
}

static void copy_string_array_ref(pp_output_t output, void* arg) {
  void** ref = (void**)arg;
  int* len_ref = (int*)&ref[0];
  const char*** arr_ref = (const char***)&ref[1];

  if (output.type == PP_OUTPUT_ARRAY) {
    int len = output.output.array.len;
    *len_ref = len;
    const char** arr = (const char**)pp_alloc(sizeof(const char*) * len);

    for (int i = 0; i < len; i++) {
      pp_output_t string_output = output.output.array.values[i];
      if (string_output.type == PP_OUTPUT_STRING) {
        arr[i] = string_output.output.string;
      } else {
        arr[i] = NULL;
      }
    }

    *arr_ref = arr;
  } else {
    *arr_ref = NULL;
  }
}
