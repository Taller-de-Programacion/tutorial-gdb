/*
 * A los alumnos de Taller:
 *      Esta prohibida la copia parcial o total de este archivo
 *      en cualquiera de las instancias de evaluacion y entregas
 *      de trabajos practicos individuales o grupales.
 *
 * Al resto del mundo:
 *      Este archivo esta bajo la licencia Creative Commons.
 *
 **/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct Stack {
    long int *base;
    long int *top;   // next free item
    size_t capacity;
};

#define STACK_CAP_MIN 4

/*
 * Initialize a Stack of long ints structure.
 * Returns 0 on success, -1 on error and errno is set appropriately.
 * */
int stack_init(struct Stack *stack) {
    stack->base = stack->top = malloc(sizeof(long int) * 4);
    stack->capacity = 4;

    return stack->base? 0 : -1;
}

/* Release any resource linked with the Stack. */
void stack_destroy(struct Stack *stack) {
    free(stack->base);
}

/* Return the how many ints the Stack has. */
size_t stack_len(struct Stack *stack) {
    return stack->top - stack->base;
}

/*
 * Push a copy of the integer <val> into the stack
 * Returns 0 on success, -1 on error and errno is set appropriately.
 * */
int stack_push(struct Stack *stack, long int val) {
    size_t l = stack_len(stack);
    if (l >= stack->capacity) {
        // twice the size
        void *tmp = realloc(stack->base, (stack->capacity*2));
        if (!tmp)
            return -1;

        stack->base = tmp;
        stack->top = stack->base + l;
    }

    *(stack->top) = val;    // top points to the next free var
    stack->top += 1;
    return 0;
}

/*
 * Pop an integer from the stack and copy it into <val>
 * Returns 0 on success, -1 on error and errno is set appropriately.
 * */
int stack_pop(struct Stack *stack, long int *val) {
    if (stack_len(stack) == 0) {
        errno = EINVAL;
        return -1;
    }

    stack->top -= 1;
    *val = *(stack->top);

    // shrink if the stack is at 25% or less of its capacity.
    // reducing it to the half
    size_t l = stack_len(stack);
    if (l < (stack->capacity / 4) &&
             stack->capacity > 4) {

        void *tmp = realloc(stack->base, (stack->capacity/2));
        if (tmp)
            return -1;

        stack->base = tmp;
        stack->top = stack->base + l;
    }

    return 0;
}

/*
 * Write to the file <stream> the integers of the stack, mostly for logging,
 * where each integer is print in a <width> zero padded slot.
 * Returns 0 on success, -1 on error and errno is set appropriately.
 * */
int stack_dump(struct Stack *stack, FILE *stream, int width) {
#ifdef HINT
    if(fprintf(stream, "%lu/%lu [", stack_len(stack), stack->capacity) < 0)
        return -1;
#endif

    for (int i = 0; i < stack_len(stack); ++i) {
        if (fprintf(stream, "%0*lx ", width, stack->base[i]) < 0)
            return -1;
    }

    if (fprintf(stream, "]\n") < 0)
        return -1;

    return 0;
}

int main(int argc, char* argv[]) {
    struct Stack stack;
    if (stack_init(&stack) != 0) {
        perror("stack_init failed");
        return -1;
    }

    char *endptr;
    long int n;
    int r = 0;
    for (int i = 1; i < argc; ++i) {
        r = -1;
        n = strtol(argv[i], &endptr, 0);
        if (argv[i] == endptr) {
            // invalid number, do a pop
            if (stack_pop(&stack, &n) != 0) {
                perror("stack_pop failed");
                break;
            }
        }
        else {
            // valid number, do a push
            if (stack_push(&stack, n) != 0) {
                perror("stack_push failed");
                break;
            }
        }

        if (stack_dump(&stack, stdout, 4) != 0) {
            perror("stack_dump failed");
            break;
        }
        r = 0;
    }

    stack_destroy(&stack);
    return r;
}
