typedef struct Reader Reader;
typedef enum Info Info;

enum Info { UNKNOWN, DIMACS, AIGER, FORMULA };

#define IMPLIES 256	// '->' token for FORMULA
#define IFF	257	// '<->' token for FORMULA

struct Reader {
  char * name;
  FILE * file;
  int close, eof, char_saved;
  Char saved_char;
  Buffer * buffer;
  CharStack symbol;
  Coo coo;
  Info type;
};

Reader * new_reader_from_stdin ();
Reader * open_new_reader (const char * path);

void delete_reader (Reader *);

Char next_char (Reader *);
Char next_non_white_space_char (Reader *);
void prev_char (Reader *, Char);
int peek_char (Reader *);

Info get_file_type (Reader *);

void parse_error (Reader *, Char ch, const char * fmt, ...);
