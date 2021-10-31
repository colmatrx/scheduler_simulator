#include<stdio.h>
#include<stdlib.h>
#include <sys/ipc.h>
#include<sys/shm.h>
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

void cleanup(void);

char *logfilename; // = "mess"; to save the log file name passed on the cmd line.

int ossclockid; unsigned int *ossclockaddress, *osstimeseconds, *osstimenanoseconds;
int messageqid; char logstring[1024];

struct msg {

  long  int msgtype;
  long  int msgcontent;
};

struct msg message; 


int main(int argc, char *argv[]){

    int option; //for tracking the command line argument provided
    int oss_run_timeout = oss_wait_timeout; //time before oss times out and kill all processes and exit. should be handled in signal handler
    long int userprocessid;

    ossclockid = shmget(ossclockkey, 8, IPC_CREAT|0766);//getting the oss clock shared memory clock before initializing it so that the id can become available to the child processes
    if (ossclockid == -1){  //checking if shared memory id was successfully created

        printf("\noss: Error: In main function, shmget() call failed");
        exit(1);
    }

    initclock(); //calls function to initialize the oss seconds and nanoseconds clocks

    printf("\nClock initialization completed");

    messageqid = msgget(messageqkey, IPC_CREAT|0766); //creates the message queue

    if (messageqid == -1){

        perror("\noss: Error: In Parent Process. Message queue creation failed\n");

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

    //fork a user process and share messages between it and oss

    *osstimeseconds = *osstimeseconds + 1; //increment oss clock by 1 second

    userprocessid = fork(); //forkig a child process at 1 second

    if (userprocessid < 0){

        perror("\noss: Error: user process creation failed!");
        exit(1);
    }

    if (userprocessid == 0){

        //inside user process
        struct msg usermessage; int messageid;
    
        messageid = msgget(messageqkey, 0); //returns the key of an existing message queue
        printf("\nuser process id is %d\n", getpid());
        msgrcv(messageid, &usermessage, sizeof(usermessage), getpid(), 0); //receives a message type where mtype is the user process id
        printf("\nIn user process, message type received from OSS is %d and message content is %d\n", usermessage.msgtype, usermessage.msgcontent);
        usermessage.msgcontent = 2; usermessage.msgtype = getpid(); //sets the usermessage before sending it to OSS
        msgsnd(messageid, &usermessage, sizeof(usermessage), IPC_NOWAIT); //sends a message back to OSS
        
        return 0;
    }

    printf("\nOSS process ID is %d\n", getpid());

    snprintf(logstring, sizeof(logstring), "\nOSS: User Process %d created at time %d second and %d nanosecond", userprocessid, *osstimeseconds, *osstimenanoseconds);

    logmsg(logfilename, logstring); //write logstring above to log file

    *osstimeseconds = *osstimeseconds + 1; //increment oss clock by 1 second

    message.msgtype = userprocessid; //sends a message where the mtype is the user process id
    message.msgcontent = 10;
    msgsnd(messageqid, &message, sizeof(message), IPC_NOWAIT);

    snprintf(logstring, sizeof(logstring), "\nOSS: OSS Process %d sends message '%d' to User Process %d at time %d seconds and %d nanosecond", getpid(), message.msgcontent, userprocessid, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    *osstimeseconds = *osstimeseconds + 1; //increment oss clock by 1 second
    msgrcv(messageqid, &message, sizeof(message), 0, 0);
    snprintf(logstring, sizeof(logstring), "\nOSS: OSS Process %d receives message '%d' from User Process %d at time %d seconds and %d nanosecond", getpid(), message.msgcontent, message.msgtype, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    *osstimeseconds = *osstimeseconds + 1;

    //Clean Up Block....Message queues and Shared Memory are freed here

    cleanup(); //called to free up Message Queue

    snprintf(logstring, sizeof(logstring), "\nOSS: Shared Memory ID %d has been detached and deleted at time %d seconds and %d nanoseconds.\n", ossclockid, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    if ((shmdt(ossclockaddress)) == -1){

        perror("\noss: Error: In cleanup(), shmdt()) failed. Shared memory cannot be detached in initclock()");
        exit(1);
    }

    printf("\nOSS clock shared memory was detached.\n");

    if (shmctl(ossclockid, IPC_RMID, NULL) != 0){      //shmctl() marks the oss clock shared memory for destruction so it can be deallocated from memory after no process is using it
        perror("\noss: Error: In cleanup(), shmctl() call failed. Segment cannot be marked for destruction\n"); //error checking shmctl() call
        exit(1);
    }

    printf("\nOSS clock shared memory was deleted.\n\n");
    
    return 0;


}


void timeouthandler(int sig){   //this function is called if the program times out after oss_run_timeout seconds. Handle killing child processes and freeing resources in here later

    printf("\noss: Error: System Timeout: In timeout handler. Aborting Child and Parent Processes..\n");

    logmsg(logfilename, "\nOSS Times out at getTime() from scheduler");

    exit(1);

    }

void initclock(void){ //initializes the seconds and nanoseconds parts of the oss

    ossclockaddress = shmat(ossclockid, NULL, 0); //shmat returns the address of the shared memory
    if (ossclockaddress == (void *) -1){

        perror("\noss: Error: In initclocks, shmat() failed");
        exit(1);

    }

    osstimeseconds = ossclockaddress + 0;   //the first 4 bytes of the address stores the seconds part of the oss clock, note the total address space is for 8 bytes from shmget above
    osstimenanoseconds = ossclockaddress + 1;   //the second 4 bytes of the address stores the seconds part of the oss clock

    *osstimeseconds = 0;    //storing integer data in the seonds part of the oss clock
    *osstimenanoseconds = 0;    //storing int data in the nanoseconds part of the oss clock

}

void cleanup(void){

    printf("\nCleaning up used resources....\n");

    if ( msgctl(messageqid, IPC_RMID, 0) == 0){

        printf("\nMessage Queue ID %d has been removed.\n", messageqid);

        snprintf(logstring, sizeof(logstring), "\nOSS: Message Queue ID %d has been removed at time %d seconds and %d nanoseconds.", messageqid, *osstimeseconds, *osstimenanoseconds);
        logmsg(logfilename, logstring);
        *osstimenanoseconds = *osstimenanoseconds + 15;
    }

    else{    
        printf("\noss: Error: In cleanup(), Message Queue removal failed!\n\n");
        exit(1);
    }

}