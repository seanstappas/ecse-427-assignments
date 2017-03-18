/*
	Creates the file system
*/
void mkssfs(int fresh) {

}            

/*
	Open the given file
*/
int ssfs_fopen(char *name) {
	
}

/*
	Closes the given file
*/
int ssfs_fclose(int fileID) {
	
}        

/*
	Seek (read) to the location from beginning
*/
int ssfs_frseek(int fileID, int loc) {
	
}

/*
	Seek (write) to the location from beginning
*/
int ssfs_fwseek(int fileID, int loc) {
	
}

/*
	Write buf characters into disk
*/
int ssfs_fwrite(int fileID, char *buf

/*
	Read characters from disk into buf
*/
int ssfs_fread(int fileID, char *buf,

/*
	Removes a file from the filesystem
*/
int ssfs_remove(char *file) {
	
}         

/*
	Create a shadow of the file system
*/
int ssfs_commit() {
	
}                   

/*
	Restore the file system to a previous shadow
*/
int ssfs_restore(int cnum) {
	
}          
