#define max_number_of_processes 18  //maximum number of processes allowed in the system
#define oss_wait_timeout 180 //time for the oss process to wait before killing all child processes and freeing up resources
#define message_queue_key 
#define ossclockkey 1980725
#define messageqkey 1302456
void logmsg(char *filename, const char *msg);