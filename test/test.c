#include <design1.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
        code_block_t code = compile(expression("DERIVATIVE 1 x ( x^2 + 2*x + 1 + x + x^2/2)", 2, "x", "y"));
	printf("Results in %f\n", run(code, 16.0, 2.0));

        return 0;
}