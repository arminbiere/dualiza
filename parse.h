struct Reader;
struct Symbols;

int is_symbol_character (int ch);
int is_symbol_start (int ch);

Circuit *parse_formula (struct Reader *, struct Symbols *);
