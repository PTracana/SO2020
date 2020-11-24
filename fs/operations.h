#ifndef FS_H
#define FS_H
#include <pthread.h>
#include "state.h"

void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries);
int create(char *name, type nodeType);
int delete(char *name);
int lookup(char *name);
int lookupWrite(char *name, int inumbers[INODE_TABLE_SIZE], int count);
int move(char* name, char* name2);
void print_tecnicofs_tree(FILE *fp);

#endif /* FS_H */
