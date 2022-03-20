TP4: Sistema de archivos e intérprete de comandos
========================

Lecturas y escrituras
---------

#### Se recomienda leer la función diskaddr() en el archivo fs/bc.c. ¿Qué es super->s_nblocks? ¿Dónde y cómo se configura este bloque especial?.

`super` es el super bloque que contiene información del file system. `super->s_nblocks` es la cantidad de bloques del disco.

Se configura en la función `opendisk`.
 
