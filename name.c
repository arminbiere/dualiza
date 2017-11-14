#include "headers.h"

Name construct_name (void * state, GetName get) {
  Name res = { state, get };
  return res;
}
