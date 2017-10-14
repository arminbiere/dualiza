typedef struct Symbols Symbols;
typedef struct Symbol Symbol;

struct Symbols {
  unsigned count, size;
  Symbol ** table;
};

struct Symbol {
  char * name;
  Symbol * next;
  Gate * gate;
};

Symbols * new_symbols ();
void delete_symbols (Symbols *);

Symbol * find_or_create_symbol (Symbols *, const char *);

extern long lookups, collisions;
