# TP4: Sistema de archivos e intérprete de comandos

En este trabajo se agrega el soporte para manejar y configurar un sistema de archivos. También, al contar con funciones de lectura y escritura en disco, se pueden cargar programas de usuario sin necesidad de compilarlos junto con la imagen del _kernel_.

Para ello será necesario tener una _syscall_ similar a `exec(3)` con la cual poder reemplazar el espacio de direcciones de un proceso previamente creado.

Finalmente, se agrega la capacidad de crear y manejar _pipes_, con su _syscall_ correspondiente `pipe(2)`. Esto posibilita, el armado de un intérprete de comandos, también conocido como _shell_.

**Importante:** Las pruebas del TP3 solamente pasarán si se comentan las siguientes dos líneas (que deberán ser descomentadas una vez pasadas las pruebas del TP3):

```
--- kern/init.c
+++ kern/init.c
@@ -56,7 +56,6 @@ i386_init(void)
 boot_aps();

 // Start fs.
-ENV_CREATE(fs_fs, ENV_TYPE_FS);

 #if defined(TEST)
 // Don't touch -- used by grading script!
--- lib/exit.c
+++ lib/exit.c
@@ -4,7 +4,6 @@
 void
 exit(void)
 {
-close_all();
 sys_env_destroy(0);
 }
```

Luego, ejecutar las pruebas del TP3 de la siguiente forma (no se puede utilizar `make grade` ya que ejecutaría las pruebas de este TP):

```
$ make clean
$ ./grade-lab4
...
Score: 20/20
```

<br/>

## Parte 0: Privilegio I/0

En JOS, el sistema de archivos se implementa en modo usuario mediante un entorno dedicado que actúa de _file server_ para el resto de entornos. El kernel no participa en la lectura y escritura en disco.

Acceder a disco, no obstante, es una operación privilegiada que un proceso común no puede realizar. Pero sí es posible dotar a un proceso de privilegios adicionales para realizar operaciones de entrada y salida.

Este nivel de “privilegio I/O” es distinto al _“ring”_ de ejecución, pues solo afecta a operaciones de entrada y salida. Así, un proceso común puede acceder a disco pero seguir ejecutándose en el _ring_ 3.

#### Tarea: fs\_iopl

Desde la función `i386_init()` se lanza al arrancar el sistema el proceso _file server_ para que esté disponible una vez se empiecen a lanzar el resto de procesos:

```C
void i386_init(void) {
    // ...

    // Start fs.
    ENV_CREATE(fs_fs, ENV_TYPE_FS);

    // ...
}
```

Como se ve, el proceso no es de tipo `ENV_TYPE_USER` sino de un nuevo tipo `ENV_TYPE_FS`. Asignar un tipo distinto a este proceso permite dos cosas:

1.  Que el resto de procesos con necesidad de acceder a disco puedan ubicarle fácilmente. (Ver la nueva función `ipc_find_env()` en _lib/ipc.c_.)
    
2.  Que, en el momento de su creación, el kernel pueda asignar privilegios de entrada y salida a procesos de este tipo.
    

Esto último es lo que se pide en esta tarea, mediante el valor [IOPL](https://en.wikipedia.org/wiki/Protection_ring#IOPL) de la arquitectura x86. (Ver macros `FL_IOPL_N` en _inc/mmu.h_.)

**Auto-corrección:** Tras implementar esta tarea debería pasar el ítem `fs i/o` de las pruebas:

```console
$ make grade
internal FS tests [fs/test.c]: OK
  fs i/o: OK
  ... FAIL
  ...
```

<br/>

## Parte 1: caché de bloques

Una vez el proceso `ENV_TYPE_FS` cuenta con acceso directo al disco y ya se escribieron funciones para leer y escribir de vía el protocolo IDE (ver archivo _fs/ide.c_), se necesitan implementar al menos tres componentes:

1.  manejo consistente de acceso a bloques lógicos (interno a `ENV_TYPE_FS`)
    
2.  conversión de bloques de filesystem a directorios y archivos (interno a `ENV_TYPE_FS`)
    
3.  recepción de peticiones de lectura/escritura por parte de otros procesos (interno-externo; comunicación con otros procesos).
    

El punto 1 se implementará en esta parte, y los dos siguientes puntos en las partes 2 y 3.

##### DISKMAP

Para facilitarse la tarea, el proceso `ENV_TYPE_FS` dedica la mayor parte de su espacio de direcciones a mantener una “imagen virtual” del disco; la imagen virtual se mantiene entre 0x10000000 (constante `DISKMAP`) hasta 0xD0000000 (valor de `DISKMAP + DISKSIZE`).

Esto quiere decir que, para el proceso `ENV_TYPE_FS`, el byte de memoria `DISKMAP + i` siempre contiene (o representa) el byte i-ésimo del disco.

De esta manera, la implementación del sistema de archivos puede referirse a posiciones de memoria y usar punteros libremente, sabiendo que numéricamente corresponden a posiciones particulares en el disco.

##### Lecturas y escrituras

Para mantener esta “imagen virtual” de manera perezosa se usa el mecanismo de page faults implementado en el trabajo práctico anterior. Esto es, en ningún momento se lee de disco por adelantado; solamente en el momento que es necesario. Esto último se detecta porque se genera una falla en una dirección comprendida en el rango `[DISKMAP, DISKMAP + DISKSIZE)`. Cuando esto ocurre, el manejador de fallas _bc\_pgfault()_ (a implementar en una de las tareas siguientes) lee de disco el bloque afectado y lo inserta en la posición de memoria correspondiente.

-   Se recomienda leer la función _diskaddr()_ en el archivo _fs/bc.c_. Responder:
    -   ¿Qué es `super->s_nblocks`?
    -   ¿Dónde y cómo se configura este bloque especial?

Las escrituras, por su parte, se manejan con una función específica _flush\_block()_ (también a implementar en una de la tareas siguientes). Otras partes de la implementación deberán llamar a esta función para asegurarse que los cambios llegan a disco (por ejemplo, al cerrar un archivo o cambiar sus metadatos).

Como se verá, la función _flush\_block()_ solamente escribe a disco si los contenidos del bloque realmente cambiaron.

#### Tarea: bc\_pgfault

Se pide implementar la función `bc_pgfault()` en _fs/bc.c_. Esta función es un _page fault handler_ como los implementados en el trabajo práctico anterior. Ante una falla, debe leer el bloque correspondiente de disco e insertarlo en una página nueva en la dirección correspondiente.

Se debe usar la función `ide_read()` para leer de disco. Esta función toma un número de sector inicial y el número de sectores a leer. Normalmente, un bloque lógico de _filesystem_ lo componen más de un sector de disco (pues estos son de tamaño menor); la proporción entre bloque y sector lo da la constante `BLKSECTS` (“sectores por bloque”).

#### Tarea: flush\_block

La función `flush_block()` recibe una dirección en el rango `[DISKMAP, DISKMAP + DISKSIZE)` y escribe a disco el bloque que contiene a esa dirección.

Para evitar escrituras innecesarias, la función se fija en el bit _“dirty”_ del PTE (representado por la constante `PTE_D`); la MMU pone este bit a 1 cada vez que hay una escritura en la página correspondiente.

Una vez escrito el bloque, se debe poner el bit _dirty_ a 0 usando `sys_page_map()`, tal y como se hace al final de la función _bc\_pgfault()_.

**Auto-corrección:** Tras implementar estas tareas deberían pasar los siguentes ítems de las pruebas:

```console
$ make grade
internal FS tests [fs/test.c]: OK
  fs i/o: OK
  check_bc: OK
  check_super: OK
  check_bitmap: OK
```

<br/>

## Parte 2: Bloques de dato

En el archivo _fs/fs.c_ se implementa el grueso del sistema de archivos, en particular:

-   manejo interno de los bloques de cada archivo
    
-   conversión de rutas (`/a/b/c/...`) a una secuencia de directorios y un archivo final en disco
    
-   todas las operaciones de bajo nivel sobre archivos (_open_, _create_, _read_, _write_, _close_).
    
Los dos últimos ítems ya están implementados, y esta parte se centra en el primero. Dado un struct _File_:

```C
struct File {
    char f_name[MAXNAMELEN]; // filename
    off_t f_size;            // file size in bytes
    uint32_t f_type;         // file type

    // Block pointers.
    // A block is allocated iff its value is != 0.
    uint32_t f_direct[NDIRECT]; // direct blocks
    uint32_t f_indirect;        // indirect block
};
```

lo que se desea es tener la operación: “dirección en memoria del bloque i-ésimo de f”. Esta primitiva facilita la escritura a bloques “logicamente contiguos” de un archivo, pero que en disco no lo están. Por ejemplo, el siguiente pseudocódigo:

```
escribir DATA en F:
  nblocks := ⌈len(DATA) / BLKSIZE⌉

  for i := 0 .. nblocks
    addr := file_get_block(F, i)
    memmove(addr, DATA + i*BLKSIZE, BLKSIZE)
```

Esta primitiva _file\_get\_block()_ se basará a su vez en otra _file\_block\_walk()_ a implementar en una de las tareas siguientes.

#### Tarea: alloc\_block

Las primitivas anteriormente descritas a veces reservarán un bloque nuevo para la posición _i_ cuando aún no existe uno. Para ello, es necesario consultar el bitmap de bloques libres y asignar uno.

En esta tarea se pide implementar la función `alloc_block()` en _fs/fs.c_ que hace precisamente eso.

Se recomienda leer las funciones _block\_is\_free()_ y _free\_block()_ y entender su implementación, pues son el complemento de _alloc\_block()_.

#### Tarea: file\_block\_walk

Si la función _file\_get\_block()_ realiza la tarea descrita en la introducción (dirección del bloque i-ésimo de un archivo), la función a implementar aquí `file_block_walk()` realiza la tarea inmediatamente anterior: determinar el número de bloque global que corresponde al bloque i-ésimo del archivo.

Sin embargo, y de manera similar al _pgdir\_walk()_ del TP1, en lugar de devolver el valor del bloque global, devuelve la posición en memoria donde se almacena; esto es, un puntero.

Así, si con la llamada `file_block_walk(f, 0)` piden el bloque global del primer bloque de _f_, la respuesta es simplemente `&f->f_direct[0]`.

La única complicación ocurre cuando _i >= NDIRECT_, pues la dirección a devolver estará en medio del bloque de punteros indirectos. Se recomienda usar la función `diskaddr()` mencionada anteriormente.

#### Tarea: file\_get\_block

Haciendo uso de la primitiva anterior _file\_block\_walk()_, implementar la función `file_get_block()`, ya descrita en párrafos anteriores.

**Auto-corrección:** Tras implementar estas tareas deberían pasar los siguentes ítems adicionales:

```console
$ make grade
internal FS tests [fs/test.c]: OK
  ...
  alloc_block: OK
  file_open: OK
  file_get_block: OK
  file_flush/file_truncate/file rewrite: OK
```

<br/>

## Parte 3: interfaz IPC

Una vez implementada el sistema de archivos propiamente dicho, queda exponerlo al resto de procesos de usuario del sistema. En JOS, esto se hace en dos _layers_ o capas:

-   en primer lugar, el proceso `ENV_TYPE_FS` ofrece una interfaz con una serie de primitivas para leer, escribir y modificar archivos. Esta capa:
    
    -   está implementada en _fs/serv.c_
    -   pasa como mensaje un _union Fsipc_, definido en _inc/fs.h_, que refleja todas las operaciones posibles, y sus argumentos
    -   se identifica a los archivos mediante un argumento `fileid` devuelto por la operación `FSREQ_OPEN`; este identificador **no** es un file descriptor y solo es válido para la comunicación directa, vía IPC, con `ENV_TYPE_FS`.
-   en segundo lugar, la biblioteca estándar de JOS implementa una interfaz de filesystem compatible con Unix y basada en _file descriptors_. Esta capa:
    
    -   está implementada en _lib/fd.c_
    -   asigna números de _file descriptor_ únicos (por proceso) y mantiene la correspondencia entre estos y los archivos abiertos en `ENV_TYPE_FS`
    -   no realiza directamente llamadas IPC a `ENV_TYPE_FS` sino que usa la abstracción _Device_ (ver _struct Dev_ en _inc/fd.h_)
    -   gracias a esta abstracción, permite usar la misma interfaz para distintos tipos de “open file” (por ejemplo, ya implementados en JOS: _pipes_ y consola); tal y como se hace en Unix. Véase estas tres implementaciones de _device_:
        -   _lib/file.c_
        -   _lib/console.c_
        -   _lib/pipe.c_

**En esta parte nos centraremos solamente en el primer ítem, interfaz IPC; y no en la capa de _file descriptors_.**

Lo anteriormente descrito se puede esquematizar en el siguiente diagrama:

```
     Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read    |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       | RPC mechanism
   |  devfile_read  |   |  serve_read   |
   |  (lib/file.c)  |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c)  |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+
```

#### Tarea: serve\_read

En esta tarea se pide implementar, desde _fs/serv.c_, la respuesta a una petición de lectura de un archivo ya abierto. La petición contiene el identificador del archivo y la cantidad de bytes a leer; se debe escribir la respuesta en el mismo _union Fsipc_ de entrada, en el campo _readRet_.

Importante: no escribir más allá del tamaño de _ret\_buf_.

**Ayuda:** Usar la función _file\_read()_ y examinar la función _serve\_set\_size()_ para observar el uso de _openfile\_lookup()_ (la función que transforma un “file id” en un _struct Openfile_).

#### Tarea: serve\_write

En esta tarea se pide:

-   la implementación de `serve_write()` en _fs/serv.c_, similar a _serve\_read()_ del punto anterior
    
-   completar la implementación del _Device_ archivo en _lib/file.c_, añadiendo el código faltante en la función `devfile_write()`. Esta es la función que se invoca desde _lib/fd.c_ cuando se invoca a _write()_ sobre un file descriptor que resulta ser un archivo.
    

**Auto-corrección:** Tras implementar estas tareas debe obtenerse:

```console
$ make grade
internal FS tests [fs/test.c]: OK (1.5s)
  fs i/o: OK
  check_bc: OK
  check_super: OK
  check_bitmap: OK
  alloc_block: OK
  file_open: OK
  file_get_block: OK
  file_flush/file_truncate/file rewrite: OK
testfile: OK (1.1s)
  serve_open/file_stat/file_close: OK
  file_read: OK
  file_write: OK
  file_read after file_write: OK
  open: OK
  large file: OK
```

<br/>

## Parte 4: spawn() y shell

El objetivo de esta parte es tener un sistema operativo que, tras arrancar, permita algún tipo de interacción con el usuario. Para ello hace falta:

-   definir qué programa se ejecutará en _userspace_ al finalizar con el arranque
    
-   permitir, mediante un driver de consola, que dicho programa tenga acceso al teclado (hasta ahora, el manejo de teclado de JOS se realizaba exclusivamente en el kernel)
    
-   si se va a permitir la ejecución de programas adicionales, tener una manera de leer ejecutables y lanzarlos a ejecución desde el sistema de archivos
    

El programa que JOS va a ejecutar tras finalizar el arranque es un _shell_ o intérprete de comandos, ya implementado en _user/sh.c_ (a falta de una pequeña parte en la última tarea del TP).

Lanzar programas a ejecución desde el sistema de archivos está implementado en _lib/spawn.c_, excepto por los dos ítems que se describen a continuación.

#### Tarea: sys\_env\_set\_trapframe

El código de la función `spawn()` es similar al código de `load_icode()` en el TP2. Simplemente, en lugar de leer el ejecutable de memoria, se lee mediante la función `read()`.

Y si bien _spawn()_ se basa en `sys_exofork()`, surgen las siguientes dos dificultades con respecto a la implementación de `fork()`:

-   se debe dar un _stack_ al nuevo proceso que no sea copia del _stack_ actual. Es más, el proceso esperará recibir en su stack los argumentos correspondientes a la función _umain_: `int argc` y `char *argv[]`.
    
    _spawn()_ realiza esta tarea en la función `init_stack()`. Esta funcion ya está implementada y no es el objetivo de esta tarea; sin embargo, su lectura y comprensión es _altamente_ instructiva.
    
-   por otra parte, se desea que el nuevo proceso arranque su ejecución en el _entry point_ del binario ELF; pero esto es algo que, hasta ahora, un proceso no podía modificar. (En _fork()_ se resuelve fácil porque el comportamiento de _sys\_exofork()_ de copiar los registros es precisamente lo que precisa _fork()_ para su implementación.)
    

Para este segundo punto se introduce **una nueva system call** `sys_env_set_trapframe()` que permite a un proceso modificar, directamente, el _struct Trapframe_ de otro proceso, para que tenga efecto la próxima vez que ese proceso se ejecute.

Para asegurarse un uso seguro de la llamada, el kernel tiene que realizar tres comprobaciones o modificaciones:

-   que el _Trapframe_ recibido como parámetro apunta a memoria de usuario válida
    
-   que el proceso no va a salir del _ring_ 3 (esto es, que el CPL de todos sus segmentos es 3)
    
-   que sale con interrupciones habilitadas e IOPL a 0
    

Una característica que tienen los _file descriptors_ en JOS es que su estado se mantiene en memoria de usuario y no en el kernel (en tablas y páginas mantenidas por _lib/fd.c_). Es típico, no obstante, que tras una llamada a _fork()_ o _spawn()_ los _file descriptors_ se preserven.

Actualmente, _fork()_ marcaría esas regiones como _copy-on-write_, mientras que _spawn()_ no las copiaría en absoluto. Esto quiere decir que un proceso lanzado con _spawn()_ arrancaría sin archivos abiertos; y, en el caso de _fork()_, la copia realizada al modificar un _file descriptor_ haría que estos cambios ya no se comuniquen a otros procesos con el mismo archivo abierto, ni por ejemplo a `ENV_TYPE_FS`.

La solución propuesta es la siguiente: marcar este tipo de memoria que _debería_ compartirse entre padres e hijos con un bit especial `PTE_SHARE` (similar a `PTE_COW` en el TP3, uno de los bits en `PTE_AVAIL`) de manera que las funciones que crean procesos sepan cuándo marcar _copy-on-write_ y cuándo simplemente compartir.

En esta tarea se pide:

-   modificar la implementación de `fork()` del TP anterior (en particular `duppage()` para que, si una página tiene el bit `PTE_SHARE`, se comparta con el hijo con los mismos permisos.
    
-   implementar, en _lib/spawn.c_ la función `copy_shared_pages()` que simplemente recorre el espacio de direcciones del padre en busca de páginas marcadas `PTE_SHARE` para compartirlas.
    

#### Tarea: kbd\_interrupt

El driver de consola ya se encuentra implementado en JOS (tanto la parte del kernel como en la biblioteca estándar), y solo resta empezar a responder a las interrupciones del teclado, y también las del puerto serie (para que QEMU atienda al teclado en la terminal).

Simplemente:

-   añadir un manejador para `IRQ_OFFSET+IRQ_KBD` que, una vez en _trap.c_, desemboque en _kbd\_intr()_
-   añadir otro manejador para `IRQ_OFFSET+IRQ_SERIAL` que desemboque en _serial\_intr()_

Con esta tarea implementada ya se puede ejecutar `make qemu` e interaccionar con el intérprete de comandos, por ejemplo ejecutando `ls`.

#### Tarea: sh\_redir

La implementación de _user/sh.c_ funciona y solo se pide agregarle la siguiente funcionalidad: poder invocarla mediante `sh <script.txt`. Para ello, se debe revisar el _switch_ de la función `runcmd()` y manejar el caso del caracter `'<'`.

Se debe abrir el archivo `t` asegurándose que termina en el descriptor 0 (de tal manera que _sh_ lo tome como su entrada estándar). Para ello, será necesario usar la función `dup()` de JOS, similar a la función [`dup2()`](http://man7.org/linux/man-pages/man2/dup.2.html) ya existente en Unix.

**Auto-corrección:** Con el TP completado, deben pasar el resto de pruebas y alcanzarse el puntaje de 150:

```console
$ make grade
...
spawn via spawnhello: OK (0.8s)
Protection I/O space: OK (1.0s)
PTE_SHARE [testpteshare]: OK (1.0s)
PTE_SHARE [testfdsharing]: OK (1.0s)
start the shell [icode]: Timeout! OK (5.5s)
testshell: OK (1.8s)
primespipe: OK (7.4s)
Score: 150/150
```