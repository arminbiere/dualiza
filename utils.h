int file_exists (const char * path);
int has_suffix (const char *, const char *);
FILE * read_pipe (const char * fmt, const char * arg);
FILE * write_pipe (const char * fmt, const char * arg);
void * first_non_zero_byte (void * p, size_t);
const char * make_character_printable_as_string (char ch);

extern unsigned primes[];
#define num_primes 8

int is_space_character (int ch);
