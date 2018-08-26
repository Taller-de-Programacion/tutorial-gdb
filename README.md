<!--

$ git checkout master stack.c       # byexample: +pass

-->

Compilamos

```shell
$ gcc -std=c99 -ggdb -O0 -o stack stack.c

```

Cada numero pasado por parametro es puesto (``stack_push``)
en el stack imprimiendose en cada iteracion el mismo (``stack_dump``)

```shell
$ ./stack 1 2 3
0001 ]
0001 0002 ]
0001 0002 0003 ]

```

Con un parametro distinto de un numero, el el ultimo elemento del stack
es removido (``stack_pop``)

```shell
$ ./stack 1 2 3 - - 7 - -
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 ]
0001 ]
0001 0007 ]
0001 ]
]

```

Realizar un ``stack_pop`` con un stack vacio finaliza la ejecucion imprimiendo
un mensaje de error y retornando un codigo de -1 (255).

```shell
$ ./stack 1 - -
0001 ]
]
stack_pop failed: Invalid argument

$ echo $?
255

```

Pero en ciertos escenarios el programa termina en un crash

```shell
$ ./stack 1 2 3 4 5 6 7 8
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 0003 0004 ]
<...>
Aborted

```


## Valgrind

Un crash como el visto es un claro ejemplo de corrupcion de memoria
pero no necesariamente es el problema.

Una corrupcion de memoria puede no implicar un crash inmediato pero va a dejar
al programa en un estado invalido, erratico.

Subsecuentes instrucciones pueden terminar en un crash, pero solo
es un sintoma de un error que sucedio antes.

``valgrind`` permite ver que corrupciones sucedieron, no solo el crash

```shell
$ valgrind ./stack 1 2 3 4 5 6 7 8                  # byexample: +timeout=4
<...>
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 0003 0004 ]
==<...>== Invalid write of size 8
==<...>==    at 0x<...>: stack_push (stack.c:60)
==<...>==    by 0x<...>: main (stack.c:139)
<...>
==<...>== Invalid read of size 8
==<...>==    at 0x<...>: stack_dump (stack.c:107)
==<...>==    by 0x<...>: main (stack.c:145)
==<...>==  <...> after a block of size <...> alloc'd
==<...>==    at 0x<...>: realloc (<...>)
==<...>==    by 0x<...>: stack_push (stack.c:52)
==<...>==    by 0x<...>: main (stack.c:139)
<...>


```

Como vemos el primer indicio de corrupción de memoria sucede luego de haber
agregado el elemento 4 al stack con un *invalid write*
por parte de ``stack_push``.

Luego tenemos un *invalid read* por ``stack_dump`` que
posiblemente este relacionado con el error anterior al intentar leer
e imprimir el stack en la salida estandar.

El programa continua erraticamente hasta que sucede el crash.

## GDB

Lanzamos ``gdb`` y cargamos el ejecutable

```
(gdb) file ./stack
Reading symbols from ./stack...done.

```

Con el comando run iniciaremos la ejecucion donde le pasaremos los
mismos parametros que producian el crash

```
(gdb) run 1 2 3 4 5 6 7 8
Starting program: <...>stack 1 2 3 4 5 6 7 8
<...>
Program received signal SIGABRT, Aborted.
<...>

```

El sistema operativo/libc detecto el estado invalido y le envio una
señal al proceso. En este caso una señal de *abort* (``SIGABRT``)

``gdb`` por default atrapa las señales y las mantiene en espera para que
tengamos oportunidad de hacer algo con ellas o con el proceso que aun sigue
vivo en memoria.

Esto nos permite ver que estaba haciendo el programa en el momento del crash.

Primero, lo mas util para orientarnos es saber en donde estamos parados.
Para ello podemos ver el *backtrace* o el stack de llamadas (tambien
conocido como *call stack*)

```
(gdb) bt
<...>
#<push-frame-num>  stack_push (stack=, val=7) at stack.c:52
#<main-frame-num>  main (argc=9, argv=) at stack.c:139

```

Cada una de las lineas indica que funcion estaba en ejecucion y que funcion
llamo a quien donde la linea mas de abajo es la funcion ``main``.

En terminologia de ``gdb``, cada linea representa un *call frame*
o simplemente un *frame*.

Vemos que la funcion ``main`` llamo a ``stack_push`` con 2 argumentos:
el ``stack`` (una direccion de memoria) y un valor ``val`` igual a 7.

Posiblemente el programa estaba pusheando al stack el valor 7 cuando crasheo.

``gdb`` nos permite movernos de un *call frame* a otro para poder explorar las
variables locales y los argumentos de esa llamada.

```
(gdb) frame 6
<...>  stack_push (stack=, val=7) at stack.c:52
52	        void *tmp = realloc(stack->base, (stack->capacity*2));

(gdb) info args
stack = 
val = 7

(gdb) info locals
tmp = 
l = 6

```

Para tener un poco mas de contexto podemos pedirle a gdb que nos imprima
las lineas de codigo de la funcion actual

```
(gdb) list                                          # byexample: +norm-ws
47       * */
48      int stack_push(struct Stack *stack, long int val) {
49          size_t l = stack_len(stack);
50          if (l >= stack->capacity) {
51              // twice the size
52              void *tmp = realloc(stack->base, (stack->capacity*2));
53              if (!tmp)
54                  return -1;
55
56              stack->base = tmp;

```

Podemos ver ahi los 2 argumentos ``stack`` y ``val`` y las 2 variables locales
``l`` y ``tmp``.

La primera es la longitud del stack (``stack_len``) y la segunda es un puntero
a la potencialmente nueva base del array re-allocado por ``realloc``.

Por lo que se deduce del crash fue al ejecutar el ``realloc`` que
se produjo el crash.

Para ver que valores tienen los argumentos de realloc podemos hacer un print

```
(gdb) print stack->base
$1 = (long *) 

(gdb) print stack->capacity
$2 = 4

```

Okay, aqui hay algo interesante. La capacidad del stack (``stack->capacity``) es
4 mientras que la longitud del mismo (``stack_len``) es 6.

Al menos alguno de los 2 es incorrecto.

Veamos ``stack_len`` primero. Podemos usar el mismo comando ``list`` indicandole
que funcion queremos ver.

En este caso le diremos "listar desde el comienzo de ``stack_len`` hasta 2
lineas mas abajo"

```
(gdb) list stack_len,+2                             # byexample: +norm-ws
40      size_t stack_len(struct Stack *stack) {
41          return stack->top - stack->base;
42      }

```

Vemos que la funcion es demasiado simple para que haya un error ahi.

Incluso podemos ejecutarla y verificar el valor

```
(gdb) print stack->top - stack->base
$3 = 6

```

o incluso podemos llamar a la funcion directamente

```
(gdb) print stack_len(stack)
$4 = 6

```

Pero como estar seguros de que el valor es el correcto?

Bueno, viendo los argumentos de stack_push

```
(gdb) info args
stack = 
val = 7

```

Vemos que el valor a pushear es el 7. Este es el septimo argumento de nuestro
programa y por lo tanto deberia haber 6 elementos previamente pusheados: el 1,
el 2, ... y el 6.

Nuestra atencion ahora se centra en stack->capacity, claramente es incorrecta.

Veamos el codigo completo de stack_push:

```
(gdb) list stack_push,+15                               # byexample: +norm-ws
48      int stack_push(struct Stack *stack, long int val) {
49          size_t l = stack_len(stack);
50          if (l >= stack->capacity) {
51              // twice the size
52              void *tmp = realloc(stack->base, (stack->capacity*2));
53              if (!tmp)
54                  return -1;
55
56              stack->base = tmp;
57              stack->top = stack->base + l;
58          }
59
60          *(stack->top) = val;    // top points to the next free var
61          stack->top += 1;
62          return 0;
63      }

```

Vemos como el codigo sugiere que si la longitud es mayor o igual
a al capacidad del stack este deberia expandirse.

Pero en ningun lado del codigo hay un cambio en la capacidad!
Y peor aun, ``realloc`` recibe como segundo parametro la cantidad en bytes
del nuevo array: ``stack->capacity * 2`` es solo la cantidad de elementos y
nos falta multiplicarla por el ``sizeof(long int)``.

Aplicamos el fix y recompilamos

```shell
$ cat 01.patch                                  # byexample: +rm=~
<...>
@@ -50,8 +50,9 @@ int stack_push(struct Stack *stack, long int val) {
     if (l >= stack->capacity) {
         // twice the size
-        void *tmp = realloc(stack->base, (stack->capacity*2));
+        void *tmp = realloc(stack->base, (stack->capacity*2)*sizeof(*stack->base));
         if (!tmp)
             return -1;
~ 
+        stack->capacity *= 2;
         stack->base = tmp;
         stack->top = stack->base + l;
@@ -82,8 +83,9 @@ int stack_pop(struct Stack *stack, long int *val) {
              stack->capacity > 4) {
~ 
-        void *tmp = realloc(stack->base, (stack->capacity/2));
+        void *tmp = realloc(stack->base, (stack->capacity/2)*sizeof(*stack->base));
         if (tmp)
             return -1;
~ 
+        stack->capacity /= 2;
         stack->base = tmp;
         stack->top = stack->base + l;

```

```shell
$ patch < 01.patch
patching file stack.c

$ gcc -std=c99 -ggdb -O0 -o stack stack.c

```

Funcionara?

Primero debemos finalizar el proceso que aun esta corriendo en ``gdb``.

Recordemos que el proceso fue detenido por que gdb intercepto al señal
SIGABRT.

Podemos entonces continuar la ejecucion con un continue. Por default
gdb propagara la señal y continuara con la ejecucion que terminara en
un abort.

```
(gdb) continue                              # byexample: +rm=~
Continuing.
~
Program terminated with signal SIGABRT, Aborted.
The program no longer exists.

```

Ahora, carguemos el nuevo binario y veamos si el fix fue suficiente

```
(gdb) file ./stack                          # byexample: +timeout=10
Reading symbols from ./stack...done.

(gdb) run 1 2 3 4 5 6 7 8
Starting program: <...>stack 1 2 3 4 5 6 7 8
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 0003 0004 ]
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 0005 0006 ]
0001 0002 0003 0004 0005 0006 0007 ]
0001 0002 0003 0004 0005 0006 0007 0008 ]
[Inferior 1 (process <pid>) exited normally]

```

Perfecto!

Veamos ahora como se comporta el stack cuando luego de una expansion se
le retiran tantos elementos que el stack se achica

```
(gdb) run 1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8 - - - - - - - - - - - - -
Starting program: <...>
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 0003 0004 ]
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 0005 0006 ]
0001 0002 0003 0004 0005 0006 0007 ]
0001 0002 0003 0004 0005 0006 0007 0008 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 0006 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 0006 0007 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 0006 0007 0008 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 0006 0007 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 0006 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 0004 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 0003 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 0002 ]
0001 0002 0003 0004 0005 0006 0007 0008 0001 ]
0001 0002 0003 0004 0005 0006 0007 0008 ]
0001 0002 0003 0004 0005 0006 0007 ]
0001 0002 0003 0004 0005 0006 ]
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 ]
stack_pop failed: Success
[Inferior 1 (process <pid>) exited with code 0377]

```

Eso no esta bien. Por alguna razon el ``stack_pop`` retorno un codigo de error
con un mensaje *muy* confuso ``failed: Success``

```
(gdb) list main,+22                             # byexample: +norm-ws
119     int main(int argc, char* argv[]) {
120         struct Stack stack;
121         if (stack_init(&stack) != 0) {
122             perror("stack_init failed");
123             return -1;
124         }
125
126         char *endptr;
127         long int n;
128         int r = 0;
129         for (int i = 1; i < argc; ++i) {
130             r = -1;
131             n = strtol(argv[i], &endptr, 0);
132             if (argv[i] == endptr) {
133                 // invalid number, do a pop
134                 if (stack_pop(&stack, &n) != 0) {
135                     perror("stack_pop failed");
136                     break;
137                 }
138             }
139             else {
140                 // valid number, do a push
141                 if (stack_push(&stack, n) != 0) {

```

Para poder entender que esta pasando vamos a re ejecutar el programa
haciendo que frene en la linea 135, justo antes de imprimir el
mensaje de error con ``perror``

Esto lo logramos colocando un ``breakpoint``.

```
(gdb) break 135
Breakpoint 1: file stack.c, line 135.

(gdb) run 1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8 - - - - - - - - - - - - -  # byexample: +rm=~
<...>
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 0003 0004 ]
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 0005 0006 ]
<...>
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 ]
~
Breakpoint 1, main (argc=30, argv=) at stack.c:135
135	                perror("stack_pop failed");

```

Ahora, como hicimos anteriormente podemos explorar las variables locales
del *call frame* actual, la funcion ``main``.

```
(gdb) info locals
i = 29
<...>

```

Claramente algo sucede cuando se procesa el argumento 29.

Seria interesante poner un breakpoint *antes* de realizar el pop.

Podriamos pone un *breakpoint* en la linea anterior, en la llamda a
``stack_pop`` pero entonces el programa se detendria en cada pop.

Sabiendo que el problema se produce cuando ``i == 29``, podemos poner un
*breakpoint* condicional

```
(gdb) break 134 if i == 29
Breakpoint 2: file stack.c, line 134.

```

Asi como pusimos un breakpoint tiene sentido borrar el primer breakpoint
que pusimos

```
(gdb) delete breakpoints 1
(gdb) info breakpoints                      # byexample: +norm-ws
Num     Type           Disp Enb What
2       breakpoint     keep y   in main at stack.c:134
        stop only if i == 29

```

Para reiniciar el proceso podemos dejar que continue normalmente con
``continue`` como hicimos anteriormente o bien podemos matar el proceso
para finalizarlo inmediatamente con un ``kill``.

```
(gdb) kill
(gdb) run 1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8 - - - - - - - - - - - - -
<...>
Breakpoint 2, main (argc=30, argv=) at stack.c:134
134	            if (stack_pop(&stack, &n) != 0) {

```

Ahora ejecutaremos el programa paso a paso.

El primer paso que daremos sera entrar en la funcion ``stack_pop`` con ``step``

```
(gdb) step
stack_pop (stack=, val=) at stack.c:71
71	    if (stack_len(stack) == 0) {

(gdb) list stack_pop,+15                     # byexample: +norm-ws
70      int stack_pop(struct Stack *stack, long int *val) {
71          if (stack_len(stack) == 0) {
72              errno = EINVAL;
73              return -1;
74          }
75
76          stack->top -= 1;
77          *val = *(stack->top);
78
79          // shrink if the stack is at 25% or less of its capacity.
80          // reducing it to the half
81          size_t l = stack_len(stack);
82          if (l < (stack->capacity / 4) &&
83                   stack->capacity > 4) {
84
85              void *tmp = realloc(stack->base, (stack->capacity/2)*sizeof(*stack->base));

```

``step`` ejecuta el programa instruccion a instruccion entrando en cada llamada
si es posible. En otros debuggers esto se lo conoce como *step into*.

Como no estamos interesados en el detalle de ``stack_len``, podemos decirle
a ``gdb`` que ejecute instruccion a instruccion sin entrar en ninguna
funcion con ``next``. En otros debuggers esto se lo conoce como *step over*.

Por default tanto para ``step`` como para ``next`` ``gdb`` muestra
la siguiente linea a ejecutar.

```
(gdb) next
76	    stack->top -= 1;
(gdb) next
77	    *val = *(stack->top);

```

```
(gdb) list 77,+18                                   # byexample: +norm-ws
77          *val = *(stack->top);
78
79          // shrink if the stack is at 25% or less of its capacity.
80          // reducing it to the half
81          size_t l = stack_len(stack);
82          if (l < (stack->capacity / 4) &&
83                   stack->capacity > 4) {
84
85              void *tmp = realloc(stack->base, (stack->capacity/2)*sizeof(*stack->base));
86              if (tmp)
87                  return -1;
88
89              stack->capacity /= 2;
90              stack->base = tmp;
91              stack->top = stack->base + l;
92          }
93
94          return 0;
95      }

```

Interesante... el unico return distinto de 0 y que por ende indica un error
es el de la linea 87.

Podemos corroborar esto poniendo un breakpoint ahi pero hay una forma mas
facil

``until`` permite ejecutar el programa hasta que llega a una posicion.

```
(gdb) until 87
stack_pop (stack=, val=) at stack.c:87
87	            return -1;

(gdb) list                                      # byexample: +norm-ws
82          if (l < (stack->capacity / 4) &&
83                   stack->capacity > 4) {
84
85              void *tmp = realloc(stack->base, (stack->capacity/2)*sizeof(*stack->base));
86              if (tmp)
87                  return -1;
88
89              stack->capacity /= 2;
90              stack->base = tmp;
91              stack->top = stack->base + l;

```

Efectivamente y ahi este el error. realloc retorna el puntero a la nueva
base del array re-allocado o NULL si hubo un error.

Deberia ser ``if (!tmp)`` y no ``if (tmp)``

> Encontramos el bug y explicamos por que el programe emite un mensaje
> de error. Pero por que el mensaje es ``failed: Success``?

Aqui tenemos el fix:

```
$ cat 02.patch
<...>
@@ -76,21 +76,21 @@ int stack_pop(struct Stack *stack, long int *val) {
<...>
         void *tmp = realloc(stack->base, (stack->capacity/2)*sizeof(*stack->base));
-        if (tmp)
+        if (!tmp)
             return -1;
<...>

```

```shell
$ patch < 02.patch
patching file stack.c

$ gcc -std=c99 -ggdb -O0 -o stack stack.c

$ ./stack 1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8 - - - - - - - - - - - - -
<...>
0001 ]
0001 0002 ]
0001 0002 0003 ]
0001 0002 0003 0004 ]
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 0005 0006 ]
<...>
0001 0002 0003 0004 0005 ]
0001 0002 0003 0004 ]
0001 0002 0003 ]

$ echo $?
0

```
