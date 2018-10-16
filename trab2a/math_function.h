#ifndef MATH_FUNCTION_H

#define MATH_FUNCTION_H

typedef double (*math_func_def)(double);

math_func_def get_math_function(int id);

#endif
