#include <colm/colm.h>

int
main(int argc, const char **argv)
{
   struct colm_program *prg = colm_new_program(&colm_object);
   colm_set_debug(prg, 0);
   colm_run_program(prg, argc, argv);
   return colm_delete_program(prg);
}
