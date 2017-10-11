typedef struct Reader Reader;

struct Reader {
  const char * name;
  FILE * file;
  int lineno, bytes;
  int buffer, buffered;
};

Reader * new_reader (const char * name, FILE * file);
void delete_reader (Reader *);

int next_char (Reader *);
int next_non_white_space_char (Reader *);

void parse_error (Reader *, const char * fmt, ...);
