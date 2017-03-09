#ifndef __A2_SEANSTAPPAS__
#define __A2_SEANSTAPPAS__

int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);
int kv_delete_db();

#endif
