#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>

#include "math_function.h"

#define MAX_SIZE 1e-16
#define FALSE 0
#define TRUE 1

int function_id;

double F(double arg) {
    math_func_def math_func = get_math_function(function_id);
    return math_func(arg);
}
     
double calc_area(double left_size, double right_size) {
    double base = (right_size - left_size);
    double mid = (right_size + left_size) / 2.0;
    double fmid = F(mid);
    double larea = base * ( (F(left_size) + fmid) / 2.0 );
    double rarea = base * ( (fmid + F(right_size)) / 2.0 );
    
    double total_area = larea;
    //Rever
    double size = rarea - larea;
    if (size < 0) { size *= -1; }
    if (size > MAX_SIZE) {
        larea = calc_area(left_size, mid);
        rarea = calc_area(mid, right_size);
        total_area = larea + rarea;
    }
    return total_area;
}

void main(int argc, char **argv) {
    double start = MPI_Wtime();
    double end;
    int ierr, num_procs, id;
    double result, result_total, left_size, right_size;
    
    ierr = MPI_Init(&argc, &argv);

    ierr = MPI_Comm_rank(MPI_COMM_WORLD, &id);
    ierr = MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
        
    left_size = atof(argv[1]);
    right_size = atof(argv[2]);
    function_id = atoi(argv[3]);
    
    double base = (left_size + right_size) / num_procs;
    double left_proc = left_size + (base * id);
    double right_proc = left_size + (base * (id + 1));
    double start_2, end_2;
    start_2 = MPI_Wtime();
    result = calc_area(left_proc, right_proc);
    end_2 = MPI_Wtime();
    printf("Time-around-quad (%d): %f\n", id, end_2 - start_2);
    
    MPI_Reduce(&result, &result_total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    end = MPI_Wtime();
    
    if (id == 0) {
        printf("Result: %0.20f\n", result_total);
    }
    printf("Time-total (%d): %f\n", id, end - start);
    
    ierr = MPI_Finalize();
}
