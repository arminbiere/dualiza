typedef struct Reader Reader;

struct Reader {
  char * name;
  FILE * file;
  int close, eof;
  Buffer * buffer;
  CharStack symbol;
  long lineno, bytes;
  const char * comment;
};

Reader * new_reader_from_stdin ();
Reader * open_new_reader (const char * path);

void delete_reader (Reader *);

int next_char (Reader *);
int next_non_white_space_char (Reader *);
void prev_char (Reader *, int);
int peek_char (Reader *);

void parse_error (Reader *, const char * fmt, ...);
