typedef struct Reader Reader;

struct Reader {
  FILE * file;
  const char * name;
  STACK (char) symbol;
  int comment, lineno, bytes, buffer, buffered;
};

Reader * new_reader (const char * name, FILE * file);
void delete_reader (Reader *);

int next_char (Reader *);
int next_non_white_space_char (Reader *);
void prev_char (Reader *, int);

void parse_error (Reader *, const char * fmt, ...);
