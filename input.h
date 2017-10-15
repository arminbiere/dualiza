typedef struct Input Input;

struct Input {
  char * name;
  FILE * file;
  int close;
};

Input * new_input_from_stdin ();
Input * open_new_input (const char * name);
void delete_input (Input *);
