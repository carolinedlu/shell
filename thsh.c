/* PURPOSE: Creation of a functional C shell to mimic behavior of Bash as closely as possible, per Dr. Porter's 530 Lab 1 rubric. Requirements include 
command parsing, changing the working directory, adding current working directory to prompt, adding debugging messages, and implementing redirection 
and scripting support. */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#define WHITESPACE " \t\n\v\f\r"
#define MAX_INPUT 1024
#define SHELLNAME "thsh> "

// * USE: Keep track of time and print time statements when in timekeeping mode (start shell with -t) *
void timeKeeper(int timeKeeping, struct tms startTime, struct tms endTime, struct timeval sTime, struct timeval eTime) {
  if (timeKeeping == 1) { //Print time-keeping stats
    gettimeofday(&eTime, NULL);
    times(&endTime);
    double realTime = eTime.tv_sec - sTime.tv_sec;
    double userTime = (endTime.tms_utime - startTime.tms_utime)/((double) sysconf(_SC_CLK_TCK));
    double sysTime = (endTime.tms_stime - startTime.tms_stime)/((double) sysconf(_SC_CLK_TCK)); 
    printf("TIMES: real=%.1f", realTime);
    printf("s user=%.1f", userTime);
    printf("s system=%.1f", sysTime);
    printf("\n"); 
  }
}

int 
main (int argc, char ** argv, char **envp) {  
  int finished;
  int keepTime = 0;
  int redirectInput = 0;
  int containsSlash = 0;
  int debugging = 0;
  struct tms startTime, endTime;
  char cmd[MAX_INPUT]; //holds char input
  char controlD = 4;
  char *token, *cwd;
  char *args[MAX_INPUT];
  int status, childPID;
  int execResult; //Is this a problem?
  char* curr = malloc(MAX_INPUT);
  curr = getcwd(curr, MAX_INPUT);
  char* newPrompt = malloc(1 + strlen(curr) + strlen(SHELLNAME) + 2); //Add CWD to prompt
  strcpy(newPrompt, "["); //Concatenate prompt
  strcat(newPrompt, curr);
  strcat(newPrompt, "] ");
  strcat(newPrompt, SHELLNAME);
  struct timeval sTime, eTime;  

  for (int i = 0; i < argc; i++) {
    if (argc > 1) {
      if (strcmp(argv[i],"-d")==0) {
	debugging = 1; //Activate debugging mode
      }
      if (strcmp(argv[i],"-t")==0) { 
	keepTime = 1; //Activate time-keeping mode
      }
    }
  }

  //* Tar Heel Graphic *
  char *tarHeel = "................................................................................\n"
    ".........................................MMM....................................\n"
    ".........................................MM.....................................\n"
    "........................................ M......................................\n"
    ".........................................M......................................\n"
    "........................................MM......................................\n"
    "....................................MMM M.......................................\n"
    ".................................... MMMM.......................................\n"
    "...................................MMMMMM.......................................\n"
    "..................................MMMMMMM ......................................\n"
    ".................................MM..MMMM.......................................\n"
    "................................M...MMMMM ......................................\n"
    "...............................MM...MMMMMM......................................\n"
    "..................................MMMMMMMMMMM...................................\n"
    "................................MMMM......MMMMM.................................\n"
    "............................ MMMM. ..........MMMM...............................\n"
    "............................MMM ................MM..............................\n"
    ".........................MMM......................MMM...........................\n"
    "................................................................................\n"
    "................................................................................\n"
    "............MM...MMMMMMMMM...MMMMMMMMM...MMMMMMMMM ....MMM .....MM....MM .......\n"
    "............MM...MM.....MM...MM.....MM...MM.....MM....MM..MM....MMMM..MM .......\n"
    "............MM...MM.....MM...MM...MMM ...MM.....MM...MMMMMMMN...MM..MMMM .......\n"
    "......MMMMMMM....:MMMMMMMM...MM.....MMM. MMMMMMMMM..MM......MM..MM....MM........\n"
    "................................................................................\n";
  
  while (!finished) {
    int needToExec = 1; //Assume command will not be built-in
    char *cursor;
    char last_char;
    int rv, count;
    rv = write(1, newPrompt, strlen(newPrompt)); //Print prompt
    if (!rv) { //If we are finished taking in input
      finished = 1;
      break;
    }   
    for(rv = 1, count = 0, 
	  cursor = cmd, last_char = 1;
	rv 
	  && (++count < (MAX_INPUT-1))
	  && (last_char != '\n');
	cursor++) {
      
      rv = read(0, cursor, 1); //Read input
      last_char = *cursor;
      cmd[count] = last_char; //Add char to cmd array
      count++;
      
      gettimeofday(&sTime, NULL);
      times(&startTime); //Start keeping time (system and user)
      
      //* BUILT-IN COMMAND: Control D *
      if (last_char == controlD) { 
	needToExec = 0;
	printf("\n");
	exit(0);
      }
    } printf("\n");
    
    *cursor = '\0'; //Reset pointer
    
    if (!rv) { //End of input
      finished = 1;
      break;
    }
    
    //* Determine if redirection or piping is necessary *
    for (int i = 0; i < MAX_INPUT && cmd[i] != '\0'; i++) {
      if (cmd[i] == '<') { //Redirect input
	redirectInput = 1;
      }
    }
    
    char cmdForRedir[MAX_INPUT]; //Copy cmd to use for redirection/piping
    strcpy(cmdForRedir, cmd);
    
    char* inputRedirFile = malloc(sizeof(cmdForRedir)); //Hold name of file for input redirection
//    char* outputRedirFile = malloc(sizeof(cmdForRedir)); //Hold name of file for output redirection (did not have time to finish implementing)
    
    //* Find and save name of file for input redirection *
    if (redirectInput == 1) {
      for (int i = 1; i < strlen(cmdForRedir); i++) {
	if (cmdForRedir[i-1] == '<') {
	  int k = 0;
	  cmd[i-1] = ' '; //Change '<' to whitespace so that it is not included in args
	  for (int j = i; j < strlen(cmdForRedir) && cmdForRedir[j] != '>' && cmdForRedir[j] != '|'; j++) {
	    inputRedirFile[k] = cmdForRedir[j];
	    k++;
	    cmd[j] = '\0'; //Get rid of redirection file name so that it is not included in args
	  }
	}
      }
    }
    
    //* Remove whitespace from inputRedirFile *
    int inputLength = strlen(inputRedirFile); //Get rid of whitespace in inputRedirFile
    for (int i = 0; i < inputLength; i++) {
      if (inputRedirFile[i] == ' ') {
	for (int j = i; j < inputLength-1; j++) {
	  inputRedirFile[i] = inputRedirFile[i+1];
	  inputRedirFile[inputLength-1] = '\0';
	}
      }
    }
    
    //* Remove whitespace from outputRedirFile * (did not have time to finish implementing)
    
    
    //* TOKEN-IZE cmd *
    token = strtok(cmd, WHITESPACE); //First token is command 
    args[0] = token; //Assign command to args[0]
    int i = 1;
    while (token !=NULL) {
      token = strtok(NULL, WHITESPACE);
      args[i] = token;
      i++;
    }
    
    // * BUILT-IN COMMAND: Exit * 
    if (strcmp(args[0], "exit") == 0) {
      printf("exit\n");
      exit(0);
    }     
    
    //* BUILT-IN COMMAND: goheels *    
    if (strcmp(args[0], "goheels") == 0) { //BUILT-IN COMMAND:  goheels
      printf("%s", tarHeel);
      needToExec = 0;
    }
    
    // * BUILT-IN COMMAND: cd (change directory) *
    if (strcmp(args[0], "cd") == 0) {
      needToExec = 0;
      if (args[1] == NULL) { //cd with no argument
	char *dir = getenv("HOME");
	cwd = getenv("PWD"); //Get current working directory
	setenv("OLDPWD",cwd,1); //Change old PWD to current wd
	chdir(dir);
	setenv("PWD",dir,1);        
	args[1] = cwd; //Is this necessary?
      } else if (args[1] != NULL && strcmp(args[1], "-")==0) { //Move to last directory
	if (getenv("OLDPWD")==NULL) {
	  printf("cd: OLDPWD not set");
          timeKeeper(keepTime, startTime, endTime, sTime, eTime);
	} if (getenv("OLDPWD") != NULL) { 
	  char *dir = getenv("OLDPWD");
	  cwd = getenv("PWD");
	  setenv("OLDPWD",cwd,1);
	  chdir(dir);
	  setenv("PWD",dir,1);
	}	
      } else if ((strcmp(args[1],"-") != 0) && args[1] != NULL) { //Move to specified directory
	cwd = getenv("PWD"); //Get current working directory
	setenv("OLDPWD",cwd,1); //Change old PWD to current wd
	chdir(args[1]);
	setenv("PWD",args[1],1);	
      }
        timeKeeper(keepTime, startTime, endTime, sTime, eTime);
   }
    
    //* DEBUGGING FUNCTION *
    if (debugging == 1 && needToExec == 1) { //Print debugging messages
      printf("RUNNING: %s", args[0]);
      printf("\n");
      if(needToExec == 0) {
	printf("No need to fork detected.\n");
      } if (needToExec == 1) {
	printf("Need to fork detected.\n");
      }
      printf("Good luck with your debugging!♥♥♥\n");
    }
    	
    //* Determine if command contains '/' *
    if (needToExec == 1) {
      for (int i = 0; i < strlen(cmd); i++) {
	if (cmd[i] == '/') {
	  containsSlash = 1;
	  break;
	}
      }
    }
    

 int *wStatus = malloc(MAX_INPUT);
    //* NON-BUILT-IN FUNCTIONS (need to fork and exec) *
    if (needToExec == 1 && redirectInput != 1) {
      childPID = fork();
      args[MAX_INPUT -1] = NULL;
      
      if (childPID == 0) { //Successfully forked
	
	if (containsSlash == 1) { //Command contains slash
	  execResult = execv(args[0], args);
	} else {
	  execResult = execvp(args[0], args);
	}	
	
	if (execResult == -1) { //Did not successfully exec
	  perror("Can\'t execvp\n");
	  if (debugging == 1) { //Debugging mode
	    printf("ENDED with error: %s; ", args[0]);
	    wStatus = &status;
	    waitpid(-1, wStatus, 0);
	    printf("ret=%d\n", *wStatus);
	  }
	}
	exit(1);
      } 
      else if (execResult != -1) { //Successfully exec-ed
	if (debugging == 1) { //Debugging mode
	  printf("ENDED: %s; ", args[0]);
	  wStatus = &status;
	  waitpid(-1, wStatus, 0);
	  printf("ret=%d\n", *wStatus);
	}
	if (debugging == 0) { //Not in debugging mode
	  wStatus = &status;
	  waitpid(-1, wStatus, 0);
	} 
      }
    } 
    else { //Did not fork correctly; need to wait
      wStatus = &status;
      waitpid(-1, wStatus, 0);
      if (debugging == 1) {
	printf("ENDED: %s; ", args[0]);
	printf("ret=%d\n", *wStatus);
      }
    } 
    if (needToExec == 1 && redirectInput == 1) {
      childPID = fork();
      args[MAX_INPUT -1] = NULL;
      
      //* INPUT REDIRECTION *
      if (redirectInput == 1) { //Redirect input
        int inFd = open(inputRedirFile, O_RDONLY);
        dup2(inFd,0);
        close(inFd);
	if (childPID == 0) {
	  execvp(args[0], args);
	} else {  
	  wStatus = &status;
	  waitpid(-1, wStatus, 0);	  
	}
      }
    } timeKeeper(keepTime, startTime, endTime, sTime, eTime);    
  }
  return 0;
}
