// 
// tsh - A tiny shell program with job control
// 
// Gabriela Tolosa Ramirez - ramirez-gabriela27
// Justin Murillo - jumu3668
// Ara Anner - aran3148
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;

//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char *argv[]) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
  char *argv[MAXARGS];

  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
  int bg = parseline(cmdline, argv); //true if bg job, false if fg job 
    
    pid_t pid;
    struct job_t *job;
    
    if(!builtin_cmd(argv)){
        //fork and exec a child process
        pid = fork();
        if(pid==0){ //if = 0, it tells us we are inside the child process
            setpgid(0,0); //group id set for fg so bg don't associate
            if(execv(argv[0], argv) < 0){ //returns a negative value when exec fails
                printf("%s: Command not found\n",argv[0]); //if command is not found
                exit(0); //exit so we don't run multiple instances of the shell 
            }            
        }else{ //otherwise, we're in the parent process
            addjob(jobs, pid, bg ? BG : FG, cmdline);//add job to struct based on bg||fg 
            if(!bg){ //if it's parent process, its fg
                waitfg(pid);//parent process will wait for child process and reap it at the same time - avoid zombies
            }else{
                job = getjobpid(jobs,pid); // get job id
                printf("[%d] (%d) %s", job->jid, pid, cmdline); //print status message
                waitfg(pid);
            }
        }
    };
    
    if (argv[0] == NULL){  
        return;   /* ignore empty lines */
    }

  return;
}


/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//
int builtin_cmd(char **argv) 
{
  string cmd(argv[0]); // so you don't have to use string compare all the time
    
    if(cmd == "quit"){//quit command
        exit(0); 
    }else if(cmd == "bg"|| cmd == "fg"){ //foreground or backgound command -> didn't seem to work without strcmp ? I don't get it... **JK, it works now, idk why
        do_bgfg(argv); //call function to execute builtin bg and fg commands
        return 1; 
    }else if(cmd == "jobs"){//jobs command
        listjobs(jobs);
        return 1; 
    }   
     return 0; //not a built in command
    /*creates a child process and exec requested program in child */
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv) 
{
  struct job_t *jobp=NULL;
    
  /* Ignore command if no argument */
  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }
    
  /* Parse the required PID or %JID arg */
  if (isdigit(argv[1][0])) {
    pid_t pid = atoi(argv[1]);
    if (!(jobp = getjobpid(jobs, pid))) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  else if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    if (!(jobp = getjobjid(jobs, jid))) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  }	    
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  // 
  string cmd(argv[0]); //didn't see this before
    
    if(cmd == "bg"){ //background
        jobp->state = BG; //state is set to bg
        kill(-jobp->pid, SIGCONT); //run the job again
        printf("[%d] (%d) %s", jobp->jid, jobp->pid, jobp->cmdline);
    }
    
    if(cmd == "fg"){ //foregrounds
        jobp->state = FG; //state is set to fg
        kill(-jobp->pid, SIGCONT); //run the job again
        waitfg(jobp->pid); //we need to have only one at a time
    }
    
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid){
    while(fgpid(jobs) == pid){ //while pid is == to fg pid, don't do anything. 
        sleep(1); 
    }//when it's not a fg pid stop sleeping
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  
//
void sigchld_handler(int sig) 
{
    pid_t pid;  //specifies the child process the caller wants to wait for
                // pid==-1 -> let child process end before reaping 
                
    int status; //where waitpid can store a status value
                //value of 0 = child process returns 0
                // else, it can be analyzed
                
//WNOHANG checks child process without suspending caller ||||| WUNTRACED is for stopped processes    
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
        if(WIFSTOPPED(status)){ //if proccess is stoped, change state to stopped
            struct job_t *job = getjobpid(jobs,pid);
            job->state = ST; //set state to stopped
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid, pid,WSTOPSIG(status));
            return; //process has stopped, do not delete job
        }else if(WIFSIGNALED(status)){//catches interupt signal - crtl-c to terminate
            struct job_t *job = getjobpid(jobs,pid);
            printf("Job [%d] (%d) terminated by signal %d\n",job->jid, pid, WTERMSIG(status));
            deletejob(jobs,pid); // delete it
            return;
        }else{
            deletejob(jobs, pid); 
        }        
    }
    return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
//
void sigint_handler(int sig) 
{
    pid_t pid = fgpid(jobs);
    if(pid != 0){ //0 means no fg process found, has to be >
        kill(-pid, sig); // kill process, default for SIGINT
    }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//
void sigtstp_handler(int sig) 
{
    pid_t pid = fgpid(jobs);
    if(pid != 0){ //0 means no fg process found, has to be >
        kill(-pid, sig); // kill process, default for SIGINT
    }
  return;
}

/*********************
 * End signal handlers
 *********************/




