typedef enum Type Type;
enum Type { NOTYPE = 0, DIMACS, AIGER, FORMULA };
Type get_file_info (Reader *);
