#include<stdio.h>
#include<stdlib.h>
#include <sys/ipc.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<strings.h>
#include<time.h>
#include<sys/wait.h>
#include<signal.h>
#include<sys/msg.h>
#include<getopt.h>
#include "config.h"

void timeouthandler(int sig);   //timeout handler function declaration

void initclock(void);   //function to initialize the two clocks in shared memory

char *logfilename; // = "mess"; to save the log file name passed on the cmd line.

int ossclockid; unsigned int ossclockaddress;


int main(int argc, char *argv[]){

    int option; //for tracking the command line argument provided
    int oss_run_timeout = oss_wait_timeout; //time before oss times out and kill all processes and exit. should be handled in signal handler

    ossclockid = shmget(ossclockkey, 8, IPC_CREAT|0766);//getting the oss clock shared memory clock before initializing it so that the id can become available to the child processes
    if (ossclockid == -1){  //checking if shared memory id was successfully created

        printf("\noss: Error: In main function, shmget() call failed");
        exit(1);
    }

    signal(SIGALRM, timeouthandler); //handles the timeout signal
    
    if (argc == 1){     //testing if there is any command line argument
        perror("\noss: Error: Missing command line argument. Use 'oss -h' for help.");   //use of perror
        exit(1);
    }


    if (argc > 3){     //using argc to test if more than required arguments are passed
        perror("\noss: Error: Invalid number of arguments. Use 'oss -h' for help.");
        exit (1);
    }


    while ((option = getopt(argc, argv, ":hs:l:")) != -1){   //starting the options with a ':' turns off getopt's error so we can just print our own perror messages
            switch (option){

                    case 'h':       //-h argument displays the help
                        printf("\noss [-h] [-s t] [-l f]\n");
                        printf("\noss -h: displays help\n\noss -s t_sec: maximum number of seconds before system terminates\n\noss -l filename: specifies a name for the log file\n");
                        exit(1);

                    case 's':       //-s t_sec specifies maximum number of seconds before system terminates
                        oss_run_timeout = strtol(optarg, NULL, 10); //strtol() converts optarg string to long int
                        printf("\nYou chose s and its parameter is %d\n", oss_run_timeout); //strtol converts string to integer. it converts the time argument from character to integer
                        break;

                    case 'l':       //-l filename specifies a name for the log file
                        logfilename = optarg;
                        printf("\nYou chose l and your log file name is %s\n", logfilename); //strtol converts string to integer. it converts the time argument from character to integer
                        break;

                    default: //if none of the correct arguments were passed
                        perror("\n\noss: Error: You have entered an invalid argument/parameter. Use 'oss -h' for help"); //use of perror
                        exit(1);
            }

        }

    alarm(oss_run_timeout); //fires timeout alarm after oss_run_timeout seconds defined in the config.h file

    printf("\nOutside of switch option while block\n");

    //logmsg(logfilename, "OSS: Writes to logfile"); //testing logging capability

    return 0;


}


void timeouthandler(int sig){   //this function is called if the program times out after oss_run_timeout seconds. Handle killing child processes and freeing resources in here later

    printf("\noss: Error: System Timeout: In timeout handler. Aborting Child and Parent Processes..\n");

    logmsg(logfilename, "\nOSS Times out at getTime() from scheduler");

    exit(1);

    }

void initclocks(void){

    ossclockaddress = shmat(ossclockid, 0, 0);
    if (ossclockaddress == -1){

        perror("\noss: Error")
    }


}