#include "fspec.h"
#include "ragel.h"

// It's pretty good base so far.
// ragel_search_str for typechecking variable delcaration is hack.
// State should have hashmap for fields/containers.
//
// XXX: Maybe drop whole container thing and just give field const char *parent; that points to keypath of container.
//      Then we would have flat structure like, "foo, foo.var, foo.b, ..."

static const struct fspec_container default_container = {0};
static const struct fspec_field default_field = { .array.nmemb = 1 };

enum stack_type {
   STACK_VAR,
   STACK_STR,
   STACK_NUM,
};

struct stack {
   enum stack_type type;

   union {
      struct fspec_bytes str;
      const char *var;
      uint64_t num;
   };
};

struct state {
   struct ragel ragel;
   struct stack stack;
   struct fspec_field field;
   struct fspec_container container;
   size_t container_data_offset;
};

static const char*
stack_type_to_str(const enum stack_type type)
{
   switch (type) {
      case STACK_VAR: return "var";
      case STACK_STR: return "str";
      case STACK_NUM: return "num";
   };

   assert(0 && "should not happen");
   return "unknown";
}

static void
stack_check_type(const struct ragel *ragel, const struct stack *stack, const enum stack_type type)
{
   assert(ragel && stack);

   if (stack->type != type)
      ragel_throw_error(ragel, "tried to get '%s' from stack, but the last pushed type was '%s'", stack_type_to_str(type), stack_type_to_str(stack->type));
}

static const char*
stack_get_var(const struct ragel *ragel, const struct stack *stack)
{
   assert(ragel && stack);
   stack_check_type(ragel, stack, STACK_VAR);
   return stack->var;
}

static const struct fspec_bytes*
stack_get_str(const struct ragel *ragel, const struct stack *stack)
{
   assert(ragel && stack);
   stack_check_type(ragel, stack, STACK_STR);
   return &stack->str;
}

static uint64_t
stack_get_num(const struct ragel *ragel, const struct stack *stack)
{
   assert(ragel && stack);
   stack_check_type(ragel, stack, STACK_NUM);
   return stack->num;
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static void
fspec_type_from_str(const struct ragel *ragel, const char *str, struct fspec_type *out_type)
{
   assert(ragel && str);

   const struct fspec_type types[] = {
      { .name = "u8", .size = sizeof(uint8_t) },
      { .name = "u16", .size = sizeof(uint16_t) },
      { .name = "u32", .size = sizeof(uint32_t) },
      { .name = "u64", .size = sizeof(uint64_t) },
      { .name = "s8", .size = sizeof(int8_t), .flags = FSPEC_TYPE_SIGNED },
      { .name = "s16", .size = sizeof(int16_t), .flags = FSPEC_TYPE_SIGNED },
      { .name = "s32", .size = sizeof(int32_t), .flags = FSPEC_TYPE_SIGNED },
      { .name = "s64", .size = sizeof(int64_t), .flags = FSPEC_TYPE_SIGNED },
   };

   for (size_t i = 0; i < ARRAY_SIZE(types); ++i) {
      if (strcmp(str, types[i].name))
         continue;

      *out_type = types[i];
      return;
   }

   if (ragel_search_str(ragel, 0, str)) {
      *out_type = (struct fspec_type){ .name = str, .flags = FSPEC_TYPE_CONTAINER };
      return;
   }

   ragel_throw_error(ragel, "invalid type");
}

static void
fspec_kind_from_str(const struct ragel *ragel, const char *str, struct fspec_kind *out_kind)
{
   assert(ragel && str);

   const struct fspec_kind kinds[] = {
      { .name = "pad", .flags = FSPEC_KIND_IGNORE },
      { .name = "hex", .flags = FSPEC_KIND_HEXADECIMAL },
      { .name = "ascii", .flags = FSPEC_KIND_ENCODING },
      { .name = "utf8", .flags = FSPEC_KIND_ENCODING },
      { .name = "sjis", .flags = FSPEC_KIND_ENCODING },
   };

   for (size_t i = 0; i < ARRAY_SIZE(kinds); ++i) {
      if (strcmp(str, kinds[i].name))
         continue;

      *out_kind = kinds[i];
      return;
   }

   ragel_throw_error(ragel, "invalid kind");
}

static void
check_field_kind(const struct ragel *ragel, const struct fspec_field *field)
{
   assert(ragel && field);

   if ((field->kind.flags & FSPEC_KIND_ENCODING) && field->type.size != sizeof(uint8_t))
      ragel_throw_error(ragel, "invalid kind: %s kind only allowed for u8 and s8 types", field->kind.name);
}

%%{
   # File specification parser.

   machine fspec;
   variable p state.ragel.p;
   variable pe state.ragel.pe;
   variable eof state.ragel.eof;
   write data noerror nofinal;

   action field {
      fspec->ops.field(fspec, &state.container, &state.field);
   }

   action field_kind {
      fspec_kind_from_str(&state.ragel, stack_get_var(&state.ragel, &state.stack), &state.field.kind);
      check_field_kind(&state.ragel, &state.field);
   }

   action field_array {
      switch (state.stack.type) {
         case STACK_NUM:
            state.field.array.type = FSPEC_ARRAY_FIXED;
            state.field.array.nmemb = stack_get_num(&state.ragel, &state.stack);
            break;

         case STACK_STR:
            state.field.array.type = FSPEC_ARRAY_MATCH;
            state.field.array.match = *stack_get_str(&state.ragel, &state.stack);
            break;

         case STACK_VAR:
            state.field.array.type = FSPEC_ARRAY_VAR;
            state.field.array.var = stack_get_var(&state.ragel, &state.stack);

            if (!ragel_search_str(&state.ragel, state.container_data_offset, state.field.array.var))
               ragel_throw_error(&state.ragel, "undeclared variable '%s'", state.field.array.var);
            break;

         default:
            ragel_throw_error(&state.ragel, "array can't contain the stack type of '%s'", stack_type_to_str(state.stack.type));
            break;
      }
   }

   action field_name {
      state.field.name = stack_get_var(&state.ragel, &state.stack);
   }

   action field_type {
      state.field = default_field;
      fspec_type_from_str(&state.ragel, stack_get_var(&state.ragel, &state.stack), &state.field.type);
   }

   action container_name {
      state.container = default_container;
      state.container.name = stack_get_var(&state.ragel, &state.stack);
      state.container_data_offset = state.ragel.mem.cur - state.ragel.mem.data;
   }

   action push_var {
      state.stack.type = STACK_VAR;
      state.stack.var = (char*)state.ragel.mem.cur;
   }

   action push_hex {
      state.stack.type = STACK_NUM;
      state.stack.num = strtoll((char*)state.ragel.mem.cur, NULL, 16);
   }

   action push_dec {
      state.stack.type = STACK_NUM;
      state.stack.num = strtoll((char*)state.ragel.mem.cur, NULL, 10);
   }

   action push_str {
      state.stack.type = STACK_STR;
      state.stack.str.data = state.ragel.mem.cur;
      state.stack.str.size = (state.ragel.mem.data + state.ragel.mem.written) - state.ragel.mem.cur;
   }

   action convert_escape {
      ragel_convert_escape(&state.ragel);
   }

   action remove {
      ragel_remove_last_data(&state.ragel);
   }

   action finish {
      ragel_finish_data(&state.ragel);
   }

   action store {
      ragel_store_data(&state.ragel);
   }

   action begin {
      ragel_begin_data(&state.ragel);
   }

   action invalid_kind {
      ragel_throw_error(&state.ragel, "invalid kind");
   }

   action invalid_type {
      ragel_throw_error(&state.ragel, "invalid type");
   }

   action error {
      ragel_throw_error(&state.ragel, "malformed input (machine failed here or in previous or next expression)");
   }

   action line {
      ragel_advance_line(&state.ragel);
   }

   # Semantic
   ws = space;
   valid = ^cntrl;
   es = '\\';
   delim = ';';
   quote = ['"];
   bopen = '{';
   bclose = '}';
   newline = '\n';
   octal = [0-7];
   hex = '0x' <: xdigit+;
   decimal = ([1-9] <: digit*) | '0';
   comment = '//' <: valid* :>> newline;
   escape = es <: ('x' <: xdigit+ | [abfnrtv\\'"e] | octal{1,3});
   type = 'u8' | 'u16' | 'u32' | 'u64' | 's8' | 's16' | 's32' | 's64';
   kind = 'ascii' | 'utf8' | 'sjis' | 'hex' | 'pad';
   reserved = 'struct' | type | kind;
   var = ((alpha | '_') <: (alnum | '_')*) - reserved;

   # Catchers
   catch_var = var >begin $store %finish %push_var;
   catch_struct = ('struct' $store ws+ >store <: var $store) >begin %finish %push_var;
   catch_type = (catch_struct | type >begin $store %push_var %remove) $!invalid_type;
   catch_hex = hex >begin $store %push_hex %remove;
   catch_decimal = decimal >begin $store %push_dec %remove;
   catch_string = quote <: (escape %convert_escape | print)* >begin $store %finish %push_str :>> quote;
   catch_array = '[' <: (catch_hex | catch_decimal | catch_string | catch_var) :>> ']';
   catch_kind = '=' ws* <: kind >begin $store %push_var %remove $!invalid_kind;

   # Actions
   field = catch_type %field_type ws+ <: catch_var %field_name ws* <: (catch_array %field_array ws*)? <: (catch_kind %field_kind ws*)? :>> delim %field;
   container = catch_struct %container_name ws* :>> bopen <: (ws | comment | field)* :>> bclose ws* delim;
   line = valid* :>> newline @line;
   main := (ws | comment | container)* & line* $!error;
}%%

void
fspec_parse(struct fspec *fspec)
{
   int cs;
   %% write init;

   (void)fspec_en_main;
   assert(fspec);
   assert(fspec->ops.read);
   assert(fspec->ops.field);

   struct state state = {
      .ragel = {
         .lineno = 1,
         .mem = {
            .data = fspec->mem.data,
            .size = fspec->mem.size,
         },
      },
   };

   for (bool ok = true; ok;) {
      const size_t bytes = fspec->ops.read(fspec, state.ragel.buf, 1, sizeof(state.ragel.buf));
      ok = ragel_confirm_input(&state.ragel, bytes);
      %% write exec;
   }
}
