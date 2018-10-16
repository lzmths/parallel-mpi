#include <math.h>

typedef double (*math_func_def)(double);

double cos_hyperbolic(double x) {
    return cosh(x) * cosh(x) * cosh(x) * cosh(x);
}

double exp_x2(double x) {
    return exp(x * x);
}

math_func_def get_math_function(int id) {
    math_func_def math_func;
    if (id == 0) {
        math_func = &cos_hyperbolic;
    } else {
        math_func = &exp_x2;
    }
    return math_func;
}
