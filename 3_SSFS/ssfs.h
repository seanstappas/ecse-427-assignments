//Functions you should implement. 
//Return -1 for error besides mkssfs
void mkssfs(int fresh);                              // creates the file system
int ssfs_fopen(char *name);                          // opens the given file
int ssfs_fclose(int fileID);                         // closes the given file
int ssfs_frseek(int fileID, int loc);                // seek (Read) to the location from beginning
int ssfs_fwseek(int fileID, int loc);                // seek (Write) to the location from beginning
int ssfs_fwrite(int fileID, char *buf, int length);  // write buf characters into disk
int ssfs_fread(int fileID, char *buf, int length);   // read characters from disk into buf
int ssfs_remove(char *file);                         // removes a file from the filesystem
int ssfs_commit();                                   // create a shadow of the file system
int ssfs_restore(int cnum);                          // restore the file system to a previous shadow
