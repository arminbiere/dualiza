typedef struct Reader Reader;
typedef enum Info Info;

enum Info { UNKNOWN, DIMACS, AIGER, FORMULA };

#define IMPLIES 256	// '->' token for FORMULA
#define IFF	257	// '<->' token for FORMULA

struct Reader {
  char * name;
  FILE * file;
  char close, eof, binary;
  Buffer * buffer;
  CharStack symbol;
  Coo saved, coo;
  Info info;
};

Reader * new_reader_from_stdin ();
Reader * open_new_reader (const char * path);

void delete_reader (Reader *);

Coo next_char (Reader *);
Coo next_non_white_space_char (Reader *);
void prev_char (Reader *, Coo);
int peek_char (Reader *);

Info get_file_info (Reader *);

void parse_error (Reader *, Coo ch, const char * fmt, ...);

#define parse_error_at(READER,COO,FMT,ARGS...) \
  parse_error (READER, COO, FMT " at character %s", ##ARGS, \
    make_character_printable_as_string (COO.code));
