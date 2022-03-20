TP2: Procesos de usuario
========================

env_alloc
---------

#### 1. ¿Qué identificadores se asignan a los primeros 5 procesos creados? (Usar base hexadecimal.)

Al estar accediendo a las posiciones del arreglo por primera vez e->env_id sera inicialmente 0 para las 5 direcciones.

Los IDs se setean en:
```C 
generation = (e->env_id + (1 << ENVGENSHIFT)) & ~(NENV - 1);
if (generation <= 0)  // Don't create a negative env_id.
	generation = 1 << ENVGENSHIFT;
e->env_id = generation | (e - envs);
```

Donde ~(NENV - 1) = 0xFC00, (1 << ENVGENSHIFT) = 0x1000 y env_size = 0x0060.

**Proceso 1**

generation = (0 + 0x1000) & 0xFC00 = 0x1000 \
e->env_id = 0x1000 | (0*0xEC40) = 0x1000

**Proceso 2**

generation = (0 + 0x1000) & 0xFC00 = 0x1000 \
e->env_id = 0x1000 | (1*0xEC40) = 0xFC40

**Proceso 3**

generation = (0 + 0x1000) & 0xFC00 = 0x1000 \
e->env_id = 0x1000 | (2*0xEC40) = 0x1D880

**Proceso 4**

generation = (0 + 0x1000) & 0xFC00 = 0x1000 \
e->env_id = 0x1000 | (3*0xEC40) = 0x2D4C0
        
**Proceso 5**

generation = (0 + 0x1000) & 0xFC00 = 0x1000 \
e->env_id = 0x1000 | (4*0xEC40) = 0x3B100

#### 2. Supongamos que al arrancar el kernel se lanzan NENV procesos a ejecución. A continuación se destruye el proceso asociado a envs[630] y se lanza un proceso que cada segundo muere y se vuelve a lanzar (se destruye, y se vuelve a crear). ¿Qué identificadores tendrán esos procesos en las primeras cinco ejecuciones?
    
**Ejecucion 1** 

envs[630] es el unico lugar libre, el nuevo proceso se lanzara en este lugar. Su ID actual es e->env_id = 0x1000 + 630*0xEC40 = 0xFC40. 

Luego, su nuevo ID:

generation =(0xFC40 + 0x1000) & 0xFC00 = 0x00C00 \
e->env_id = 0x00C00 | 0x0EC40 = 0x0EC40

**Ejecucion 2**

Analogamente, pero ahora el id actual es e->env_id = 0x0EC40.

generation = (0x0EC40 + 0x1000) & 0xFC00 = 0x0FC00 \
e->env_id = 0x0FC00 | 0x0EC40 = 0x0FC40

**Ejecucion 3**

Analogamente, pero ahora el id actual es e->env_id = 0x0FC40.

generation = (0x0FC40 + 0x1000) & 0x0FC00 = 0x0C00 \
e->env_id = 0x00C00 | 0x0EC40 = 0xEC40

**Ejecucion 4**

Analogamente, pero ahora el id actual es e->env_id = 0xEC40.

generation = (0x0EC40 + 0x1000) & 0x0FC00 = 0x0FC00 \
e->env_id = 0x0FC00 | 0x0EC40 = 0x0FC40

**Ejecucion 5**

Analogamente, pero ahora el id actual es e->env_id = 0x0FC40.

generation = (0x0FC40 + 0x1000) & 0x0FC00 = 0x0C00 \
e->env_id = 0x00C00 | 0x0EC40 = 0xEC40


env_init_percpu
---------------

#### ¿Cuántos bytes escribe la función lgdt, y dónde?
#### ¿Qué representan esos bytes?

lgdt carga el registro de la Global Descriptor Table, recibe 6 bytes de los cuales 4 corresponden a la direccion y 2 al limite (figura 2-5 del manual de intel).

env_pop_tf
----------

#### La función env_pop_tf() ya implementada es en JOS el último paso de un context switch a modo usuario. Dada la secuencia de instrucciones assembly en la función, describir qué contiene durante su ejecución:
#### el tope de la pila justo antes popal

Contiene todo el contexto (registros generales) del proceso en el momento en que ocurrio la interrupcion.

#### el tope de la pila justo antes iret

El return intruction pointer.

#### el tercer elemento de la pila justo antes de iret

Los EFLAGS.

#### ¿Cómo determina la CPU (en x86) si hay un cambio de ring (nivel de privilegio)? Ayuda: Responder antes en qué lugar exacto guarda x86 el nivel de privilegio actual. ¿Cuántos bits almacenan ese privilegio?

El nivel de privilegio se guarda en los dos bits de CPL.La CPU sabe que hay un cambio de ring si cambiaron estos bits en el code segment selector luego del popal.

gdb_hello
---------

#### 1. Poner un breakpoint en env_pop_tf() y continuar la ejecución hasta allí.

```console
(gdb) b env_pop_tf
Breakpoint 1 at 0xf0102ed1: file kern/env.c, line 471.
(gdb) continue
Continuing.
The target architecture is set to "i386".
=> 0xf0102ed1 <env_pop_tf>:	push   %ebp

Breakpoint 1, env_pop_tf (tf=0xf01b5000) at kern/env.c:471
471	{
```

#### 2. En QEMU, entrar en modo monitor (Ctrl-a c), y mostrar las cinco primeras líneas del comando info registers.

```console
(qemu) info registers
EAX=003bc000 EBX=f01b5000 ECX=f03bc000 EDX=00000219
ESI=00000002 EDI=f03bc000 EBP=f0119fd8 ESP=f0119fac
EIP=f0102ed1 EFL=00000096 [--S-AP-] CPL=0 II=0 A20=1 SMM=0 HLT=0
ES =0010 00000000 ffffffff 00cf9300 DPL=0 DS   [-WA]
CS =0008 00000000 ffffffff 00cf9a00 DPL=0 CS32 [-R-]
```

#### 3. De vuelta a GDB, imprimir el valor del argumento tf.

```console
(gdb) p tf
$1 = (struct Trapframe *) 0xf01b5000
```

#### 4. Imprimir, con x/Nx tf tantos enteros como haya en el struct Trapframe donde N = sizeof(Trapframe) / sizeof(int).

```console
(gdb) print sizeof(struct Trapframe) / sizeof(int)
$2 = 17
(gdb) x/17x tf
0xf01b5000:	0x00000000	0x00000000	0x00000000	0x00000000
0xf01b5010:	0x00000000	0x00000000	0x00000000	0x00000000
0xf01b5020:	0x00000023	0x00000023	0x00000000	0x00000000
0xf01b5030:	0x00800020	0x0000001b	0x00000000	0xeebfe000
0xf01b5040:	0x00000023
```

#### 5. Avanzar hasta justo después del movl ...,%esp, usando si M para ejecutar tantas instrucciones como sea necesario en un solo paso.

```console
(gdb) disas
Dump of assembler code for function env_pop_tf:
=> 0xf0102eca <+0>:	push   %ebp
   0xf0102ecb <+1>:	mov    %esp,%ebp
   0xf0102ecd <+3>:	sub    $0xc,%esp
   0xf0102ed0 <+6>:	mov    0x8(%ebp),%esp
   0xf0102ed3 <+9>:	popa   
   0xf0102ed4 <+10>:	pop    %es
   0xf0102ed5 <+11>:	pop    %ds
   0xf0102ed6 <+12>:	add    $0x8,%esp
   0xf0102ed9 <+15>:	iret   
   0xf0102eda <+16>:	push   $0xf010567b
   0xf0102edf <+21>:	push   $0x1e0
   0xf0102ee4 <+26>:	push   $0xf0105626
   0xf0102ee9 <+31>:	call   0xf01000a9 <_panic>
End of assembler dump.
(gdb) si 4
=> 0xf0102eda <env_pop_tf+9>:	popa   
0xf0102eda	472		asm volatile("\tmovl %0,%%esp\n"
```

#### 6. Comprobar, con x/Nx $sp que los contenidos son los mismos que tf (donde N es el tamaño de tf).

```console
(gdb) x/17x $sp
0xf01b5000:	0x00000000	0x00000000	0x00000000	0x00000000
0xf01b5010:	0x00000000	0x00000000	0x00000000	0x00000000
0xf01b5020:	0x00000023	0x00000023	0x00000000	0x00000000
0xf01b5030:	0x00800020	0x0000001b	0x00000000	0xeebfe000
0xf01b5040:	0x00000023
```

#### 7. Describir cada uno de los valores. Para los valores no nulos, se debe indicar dónde se configuró inicialmente el valor, y qué representa.

#### 8. Continuar hasta la instrucción iret, sin llegar a ejecutarla. Mostrar en este punto, de nuevo, las cinco primeras líneas de info registers en el monitor de QEMU. Explicar los cambios producidos.

```console
(gdb) si 4
=> 0xf0102ee0 <env_pop_tf+15>:	iret   
0xf0102ee0	472		asm volatile("\tmovl %0,%%esp\n"
(gdb) disas
Dump of assembler code for function env_pop_tf:
   0xf0102ed1 <+0>:	push   %ebp
   0xf0102ed2 <+1>:	mov    %esp,%ebp
   0xf0102ed4 <+3>:	sub    $0xc,%esp
   0xf0102ed7 <+6>:	mov    0x8(%ebp),%esp
   0xf0102eda <+9>:	popa   
   0xf0102edb <+10>:	pop    %es
   0xf0102edc <+11>:	pop    %ds
   0xf0102edd <+12>:	add    $0x8,%esp
=> 0xf0102ee0 <+15>:	iret   
   0xf0102ee1 <+16>:	push   $0xf010567b
   0xf0102ee6 <+21>:	push   $0x1e1
   0xf0102eeb <+26>:	push   $0xf0105626
   0xf0102ef0 <+31>:	call   0xf01000a9 <_panic>
End of assembler dump.
```

```console
(qemu) info registers
EAX=00000000 EBX=00000000 ECX=00000000 EDX=00000000
ESI=00000000 EDI=00000000 EBP=00000000 ESP=f01b5030
EIP=f0102ee0 EFL=00000096 [--S-AP-] CPL=0 II=0 A20=1 SMM=0 HLT=0
ES =0023 00000000 ffffffff 00cff300 DPL=3 DS   [-WA]
CS =0008 00000000 ffffffff 00cf9a00 DPL=0 CS32 [-R-]
```

Casi todos los registros pasaron a 0. Esto es porque se creo un nuevo contexto para el proceso y esta listo para pasar a modo usuario con iret.

#### 9. Ejecutar la instrucción iret. En ese momento se ha realizado el cambio de contexto y los símbolos del kernel ya no son válidos. Imprimir el valor del contador de programa con p $pc o p $eip. Cargar los símbolos de hello con el comando add-symbol-file. Volver a imprimir el valor del contador de programa. Mostrar una última vez la salida de info registers en QEMU, y explicar los cambios producidos.

```console
(gdb) si 1
=> 0x800020:	cmp    $0xeebfe000,%esp
0x00800020 in ?? ()
(gdb) p $pc
$1 = (void (*)()) 0x800020
(gdb) add-symbol-file obj/user/hello 0x800020
add symbol table from file "obj/user/hello" at
	.text_addr = 0x800020
(y or n) y
Reading symbols from obj/user/hello...
(gdb) p $pc
$2 = (void (*)()) 0x800020 <_start>
```

```console
(qemu) info registers
EAX=00000000 EBX=00000000 ECX=00000000 EDX=00000000
ESI=00000000 EDI=00000000 EBP=00000000 ESP=eebfe000
EIP=00800020 EFL=00000002 [-------] CPL=3 II=0 A20=1 SMM=0 HLT=0
ES =0023 00000000 ffffffff 00cff300 DPL=3 DS   [-WA]
CS =001b 00000000 ffffffff 00cffa00 DPL=3 CS32 [-R-]
```

Cambio el stack pointer y el code segment, ademas CPL paso de 0 a 3 indicando que se paso de modo kernel a modo usuario, para empezar a ejecutar el nuevo proceso o job.

#### 10. Poner un breakpoint temporal (tbreak, se aplica una sola vez) en la función syscall() y explicar qué ocurre justo tras ejecutar la instrucción int $0x30. Usar, de ser necesario, el monitor de QEMU.

```console
(gdb) tbreak syscall
Temporary breakpoint 2 at 0x8009c9: syscall. (2 locations)
(gdb) continue
Continuing.
=> 0x8009c9 <syscall+17>:	mov    0x8(%ebp),%ecx

Temporary breakpoint 2, syscall (num=0, check=-289415544, a1=4005551752, a2=13, a3=0, a4=0, a5=0) at lib/syscall.c:23
23		asm volatile("int %1\n"
(gdb) si 4
=> 0x8009d5 <syscall+29>:	int    $0x30
0x008009d5	23		asm volatile("int %1\n"
(gdb) si 1
=> 0xf0103804 <trap_syscall>:	push   $0x0
trap_syscall () at kern/trapentry.S:68
68	TRAPHANDLER_NOEC(trap_syscall, T_SYSCALL)
```

```console
(qemu) info registers
EAX=00000000 EBX=00000000 ECX=0000000d EDX=eebfde88
ESI=00000000 EDI=00000000 EBP=eebfde40 ESP=efffffec
EIP=f0103804 EFL=00000096 [--S-AP-] CPL=0 II=0 A20=1 SMM=0 HLT=0
ES =0023 00000000 ffffffff 00cff300 DPL=3 DS   [-WA]
CS =0008 00000000 ffffffff 00cf9a00 DPL=0 CS32 [-R-]
```

EL cambio mas significativo se observa en la linea 3 de los registros, donde CPL paso de 3 a 0, indicando que se paso de modo kernel a modo usuario.


kern_idt
---------------

#### ¿Cómo decidir si usar TRAPHANDLER o TRAPHANDLER_NOEC? ¿Qué pasaría si se usara solamente la primera?

Se decide segun lo que especifique el hardware para la interrupcion. Las interrupciones para las que el manual de intel especifica un codigo de error se definen usando la macro TRAPHANDLER, en caso contrario se usa TRAPHANDLER_NOEC. 
TRAPHANDLER_NOEC se comporta de manera similar a TRAPHANDLER con la diferencia de que pushea un 0 en el lugar donde la CPU pushea un codigo de error en el caso de las interrupciones que generan uno. Al usar solamente TRAPHANDLER en el caso de las interrupciones que no generan un codigo de error se interpretaria erroneamente informaciom al faltar los bits del error.

#### ¿Qué cambia, en la invocación de handlers, el segundo parámetro (istrap) de la macro SETGATE? ¿Por qué se elegiría un comportamiento u otro durante un syscall?

El parametro istrap define si se permiten interrupciones anidadas o no. Se eligiria un comportamiento anidado si se quisieran priorizar ciertas interrupciones por sobre otras, como por ejemplo interrupciones de disco, asi se podria pedir que se maneje inmediatamente.

#### Leer user/softint.c y ejecutarlo con make run-softint-nox. ¿Qué interrupción trata de generar? ¿Qué interrupción se genera? Si son diferentes a la que invoca el programa… ¿cuál es el mecanismo por el que ocurrió esto, y por qué motivos? ¿Qué modificarían en JOS para cambiar este comportamiento?

```console
Incoming TRAP frame at 0xefffffbc
TRAP frame at 0xf01b5000
  edi  0x00000000
  esi  0x00000000
  ebp  0xeebfdff0
  oesp 0xefffffdc
  ebx  0x00000000
  edx  0x00000000
  ecx  0x00000000
  eax  0x00000000
  es   0x----0023
  ds   0x----0023
  trap 0x0000000d General Protection
  err  0x00000072
  eip  0x00800033
  cs   0x----001b
  flag 0x00000082
  esp  0xeebfdfd4
  ss   0x----0023
[00001000] free env 00001000
```

Intenta generar una interrupcion Page Fault pero genera una General Protection. Esto ocurre porque el Descriptor Priviledge Level de la interrupcion Page Fault es 0 por lo que solo puede ser generada por el kernel, pero la ejecucion del programa ocurre en modo usuario. Entonces interviene el kernel y genera la interrupcion General Protection.
Para cambiar este comportamiento en JOS modificaria el DPL de la interrupcion Page Fault a 3. Ejecutando el programa nuevamente con esta modificacion:

```console
TRAP frame at 0xefffffc0
  edi  0x00000000
  esi  0x00000000
  ebp  0xeebfdff0
  oesp 0xefffffe0
  ebx  0x00000000
  edx  0x00000000
  ecx  0x00000000
  eax  0x00000000
  es   0x----0023
  ds   0x----0023
  trap 0x0000000e Page Fault
  cr2  0x00000000
  err  0x00800035 [user, read, protection]
  eip  0x0000001b
  cs   0x----0082
  flag 0xeebfdfd4
  esp  0x00000023
  ss   0x----ff53
[00001000] free env 00001000
```

user_evil_hello
---------------

#### Ejecutar el siguiente programa y describir qué ocurre:

```C
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    char *entry = (char *) 0xf010000c;
    char first = *entry;
    sys_cputs(&first, 1);
}
```

```console
Incoming TRAP frame at 0xefffffbc
[00001000] user fault va f010000c ip 00800039
TRAP frame at 0xf01b5000
  edi  0x00000000
  esi  0x00000000
  ebp  0xeebfdfd0
  oesp 0xefffffdc
  ebx  0x00000000
  edx  0x00000000
  ecx  0x00000000
  eax  0x00000000
  es   0x----0023
  ds   0x----0023
  trap 0x0000000e Page Fault
  cr2  0xf010000c
  err  0x00000005 [user, read, protection]
  eip  0x00800039
  cs   0x----001b
  flag 0x00000082
  esp  0xeebfdfb0
  ss   0x----0023
[00001000] free env 00001000
Destroyed the only environment 
```

Se genero una interrupcion Page Fault.

#### ¿En qué se diferencia el código de la versión en evilhello.c mostrada arriba?

En evilhello.c se pasa direccion 0xf010000c a sys_cputs sin usar variables de por medio.

#### ¿En qué cambia el comportamiento durante la ejecución?

Ejecutando evilhello.c:

```console
[00000000] new env 00001000
Incoming TRAP frame at 0xefffffbc
[00001000] user_mem_check assertion failure for va f010000c
[00001000] free env 00001000
```

Ya no tira una execpcion de Page Fault sino que falla el user_mem_assert al ejecutar la syscall.

#### ¿Por qué? ¿Cuál es el mecanismo?

Porque al ejecutar el programa del enunciado se entiende que &first es una direccion del usuario y el querer desreferenciar esta direccion estando en el ring 3 se provoca una execepcion. En cambio en evilhello.c la direccion es un literal en el programa y se detecta el problema en user_mem_assert.
