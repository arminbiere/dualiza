typedef struct Reader Reader;
typedef enum Type Type;

enum Type { UNKNOWN, DIMACS, AIGER, FORMULA };

struct Reader {
  char * name;
  FILE * file;
  int close, eof, char_saved;
  Char saved_char;
  Buffer * buffer;
  CharStack symbol;
  Coo coo;
  Type type;
};

Reader * new_reader_from_stdin ();
Reader * open_new_reader (const char * path);

void delete_reader (Reader *);

Char next_char (Reader *);
Char next_non_white_space_char (Reader *);
void prev_char (Reader *, Char);
int peek_char (Reader *);

Type get_file_type (Reader *);

void parse_error (Reader *, const char * fmt, ...);
