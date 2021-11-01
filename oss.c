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

void createprocess(long int);

int randomtime(int, int);

char *logfilename; // = "mess"; to save the log file name passed on the cmd line.

int ossclockid, processtableid; unsigned int *ossclockaddress, *osstimeseconds, *osstimenanoseconds;
int messageqid; char logstring[1024]; int processcount = 0;

struct msg {

  long  int msgtype;
  long  int msgcontent;
};

struct msg message; 

//Process Control Block Data Structure

typedef struct processes{
    
    long int processid;
    int timeqused;
    int timeqleft;
    int priority;
} process;

process userprocess[18]; //Array representing Process Control Tab
process *processtableaddress;

int main(int argc, char *argv[]){

    int option; //for tracking the command line argument provided
    int oss_run_timeout = oss_wait_timeout; //time before oss times out and kill all processes and exit. should be handled in signal handler
    long int userprocessid; int ossofflinesecondclock, ossofflinenanosecondclock;

    processtableid = shmget(processtablekey, (sizeof(process)*18), IPC_CREAT|0766); //create a shared memory 

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
        struct msg usermessage; int messageid; int runtime; int processindex; process *proctableaddress;
    
        messageid = msgget(messageqkey, 0); //returns the key of an existing message queue
        printf("\nuser process id is %d\n", getpid());
        msgrcv(messageid, &usermessage, sizeof(usermessage), getpid(), 0); //receives a message type where mtype is the user process id
        printf("\nIn user process, message type received from OSS is %d and message content is %d\n", usermessage.msgtype, usermessage.msgcontent);

        //get process index from the process control table

        proctableaddress = shmat(processtableid, NULL, 0); //attach to the process table shared memory

        if (proctableaddress == (void *) -1){

            perror("\noss: Error: In user process, proc table address shmat() failed");
            exit(1);
        }

        for (int i = 0; i < 18; i++){       //traverse the process table to locate the user process index
            if (((proctableaddress+i)->processid) == (getpid())){
                printf("\nuser process id from process table is %d", (proctableaddress+i)->processid);
                printf("\nuser process time left from process table is %d", (proctableaddress+i)->timeqleft);
                processindex = i;
                break;
            }
        }
        
        runtime = randomtime(0, (proctableaddress+processindex)->timeqleft); //generate random time between 0 and time left unused

        printf("\nruntime is %d\n", runtime);

        usermessage.msgcontent = runtime; usermessage.msgtype = getpid(); //sets the usermessage before sending it to OSS
        msgsnd(messageid, &usermessage, sizeof(usermessage), IPC_NOWAIT); //sends a message back to OSS

        if ((shmdt(proctableaddress)) == -1){    //detaching from the process table shared memory

            perror("\noss: Error: In user process() section, process table shmdt()) failed. process table shared memory cannot be detached");
            exit(1);
        }
        
        return 0;
    }

    processcount = processcount + 1; //incrememt to track the total processes in the system; maximum allowed is 18

    processtableaddress = shmat(processtableid, NULL, 0); //shmat returns the address of the shared memory

    if (processtableaddress == (void *) -1){

        perror("\noss: Error: In main, process table address shmat() failed");
        exit(1);

    }

    (processtableaddress+(processcount-1))->processid = userprocessid; //access each structure elements in the process table kept in shared memory
    (processtableaddress+(processcount-1))->timeqleft = 10;
    (processtableaddress+(processcount-1))->timeqused = 0;
    (processtableaddress+(processcount-1))->priority = 0;

    printf("\nattached processid is %d\n", (processtableaddress+(processcount-1))->processid);

    printf("\nOSS process ID is %d\n", getpid());

    snprintf(logstring, sizeof(logstring), "\nOSS: User Process %d created at time %d second and %d nanosecond", (processtableaddress+(processcount-1))->processid, *osstimeseconds, *osstimenanoseconds);

    logmsg(logfilename, logstring); //write logstring above to log file

    *osstimeseconds = *osstimeseconds + 1; //increment oss clock by 1 second

    message.msgtype = (processtableaddress+(processcount-1))->processid; //sends a message where the mtype is the user process id
    printf("\nOSS sends message type %d\n", message.msgtype);
    message.msgcontent = (processtableaddress+(processcount-1))->timeqleft;
    msgsnd(messageqid, &message, sizeof(message), IPC_NOWAIT);

    snprintf(logstring, sizeof(logstring), "\nOSS: OSS Process %d sends time quantum %d nanoseconds to User Process %d at time %d seconds and %d nanosecond", getpid(), message.msgcontent, message.msgtype, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    *osstimeseconds = *osstimeseconds + 1; //increment oss clock by 1 second
    msgrcv(messageqid, &message, sizeof(message), 0, 0);
    snprintf(logstring, sizeof(logstring), "\nOSS: OSS Process %d receives runtime of %d nanoseconds from User Process %d at time %d seconds and %d nanosecond", getpid(), message.msgcontent, message.msgtype, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    *osstimeseconds = *osstimeseconds + 1;

    //Clean Up Block....Message queues and Shared Memory are freed here

    cleanup(); //called to free up Message Queue

    if ((shmdt(processtableaddress)) == -1){    //detaching from the process table shared memory

        perror("\noss: Error: In cleanup() section, process table shmdt()) failed. process table shared memory cannot be detached");
        exit(1);
    }

    printf("\nprocess table shared memory was detached.\n");

    if (shmctl(processtableid, IPC_RMID, NULL) != 0){      //shmctl() marks the oss process table shared memory for destruction so it can be deallocated from memory after no process is using it
        perror("\noss: Error: In cleanup() section, process table shmctl() call failed. Segment cannot be marked for destruction\n"); //error checking shmctl() call
        exit(1);
    }

    printf("\nprocess table shared memory was deleted.\n\n");

    snprintf(logstring, sizeof(logstring), "\nOSS: Process Table Shared Memory ID %d has been detached and deleted at time %d seconds and %d nanoseconds.", processtableid, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    *osstimeseconds = *osstimeseconds + 1; //increment oss clock by 1 second

    ossofflinesecondclock = *osstimeseconds;
    ossofflinenanosecondclock = *osstimenanoseconds;

    snprintf(logstring, sizeof(logstring), "\nOSS: OSS Clock Shared Memory ID %d has been detached and deleted at time %d seconds and %d nanoseconds.", ossclockid, *osstimeseconds, *osstimenanoseconds);
    logmsg(logfilename, logstring);

    if ((shmdt(ossclockaddress)) == -1){    //detaching from the oss clock shared memory

        perror("\noss: Error: In cleanup() section, shmdt()) failed. OSS clock address shared memory cannot be detached");
        exit(1);
    }

    printf("\nOSS clock shared memory was detached.\n");

    if (shmctl(ossclockid, IPC_RMID, NULL) != 0){      //shmctl() marks the oss clock shared memory for destruction so it can be deallocated from memory after no process is using it
        perror("\noss: Error: In cleanup() section, OSS clockid shmctl() call failed. Segment cannot be marked for destruction\n"); //error checking shmctl() call
        exit(1);
    }

    printf("\nOSS clock shared memory was deleted.\n\n");

    snprintf(logstring, sizeof(logstring), "\nOSS: OSS suucessfully completed execution at %d seconds and %d nanoseconds\n", ossofflinesecondclock + 1, ossofflinenanosecondclock);
    logmsg(logfilename, logstring);

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

int randomtime(int lowertimelimit, int uppertimelimit){

    int randsec;
    srand(time(NULL));          //initilize the rand function
    randsec = (rand() % ((uppertimelimit - lowertimelimit) + 1)) + lowertimelimit; //this logic produces a number between lowertimelit and uppertimelimit
    return randsec;

}

void cleanup(void){ //cleans up the message queue

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