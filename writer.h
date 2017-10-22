typedef struct Writer Writer;

struct Writer {
  char * name;
  FILE * file;
  int close;
};

Writer * new_writer_from_stdout ();
Writer * open_new_writer (const char * path);
void delete_writer (Writer *);
