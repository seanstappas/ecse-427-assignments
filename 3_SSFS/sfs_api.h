//Functions you should implement. 
//Return -1 for error besides mkssfs
void mkssfs(int fresh);
int ssfs_get_next_file_name(char *fname);
int ssfs_get_file_size(char* path);
int ssfs_fopen(char *name);
int ssfs_fclose(int fileID);
int ssfs_frseek(int fileID, int loc);
int ssfs_fwseek(int fileID, int loc);
int ssfs_fwrite(int fileID, char *buf, int length);
int ssfs_fread(int fileID, char *buf, int length);
int ssfs_remove(char *file);
int ssfs_commit();
int ssfs_restore(int cnum);