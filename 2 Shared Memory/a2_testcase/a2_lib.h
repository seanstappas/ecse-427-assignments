#define _XOPEN_SOURCE 700

#define MAX_KEY_SIZE 32
#define MAX_VALUE_SIZE 256
#define POD_SIZE 256
#define NUMBER_OF_PODS 256 // k pods. could be a multiple of 16

unsigned long hash(unsigned char *str);
int kv_store_create(char *name);
int kv_store_write(char *key, char *value);
char *kv_store_read(char *key);
char **kv_store_read_all(char *key);
int kv_delete_db();
static void handlerSIGINT(int sig);