#ifndef NAME_H_INCLUDED
#define NAME_H_INCLUDED

typedef struct Name Name;

typedef const char *(*GetName) (void *, int);

struct Name
{
  void *state;
  GetName get;
};

Name construct_name (void *, GetName);

#endif
