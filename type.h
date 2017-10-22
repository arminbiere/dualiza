typedef enum Type Type;
enum Type { DIMACS, AIGER, FORMULA };
Type get_file_type (Reader *);
