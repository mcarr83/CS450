#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

//OMG!!!!! This a lot of stuff!!!!!!!

//Using MAX_ARGS as a preprocessor variable
#define MAX_ARGS 3

//No idea why this works but it does so go with it
#define USER_VADDR_BOTTOM ((void *) 0x08048000)

//struct made from synch.h
struct lock filesys_lock;

//new struct to process a file
struct process_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};

int user2kernel_ptr(const void* vaddr);
int pro_ADD_FILE (struct file *f);

static void syscall_handler (struct intr_frame *);
void get_arg (struct intr_frame *f, int *arg, int n);
void valid_ptr (const void *vaddr);
void check_valid_buffer (void* buffer, unsigned size);

struct file* pro_GET_FILE(int fd);

/*
One of the only functions still from the orginal file
 */

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}



static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int arg[MAX_ARGS];
  //Checks to see if f->esp is a valid pointer. 
  valid_ptr((const void*) f->esp);
  //How to was in Pintos Guide on Piazza
  switch (* (int *) f->esp){
    case SYS_HALT:{
		halt(); 
		break;
      }
    case SYS_EXIT:{
		get_arg(f, &arg[0], 1);
		exit(arg[0]);
		break;
      }
    case SYS_EXEC:{
		get_arg(f, &arg[0], 1);
		arg[0] = user2kernel_ptr((const void *) arg[0]);
		f->eax = exec((const char *) arg[0]); 
		break;
      }
    case SYS_WAIT:{
		get_arg(f, &arg[0], 1);
		f->eax = wait(arg[0]);
		break;
      }
    case SYS_CREATE:{
		get_arg(f, &arg[0], 2);
		arg[0] = user2kernel_ptr((const void *) arg[0]);
		f->eax = create((const char *)arg[0], (unsigned) arg[1]);
		break;
      }
    case SYS_REMOVE:{
		get_arg(f, &arg[0], 1);
		arg[0] = user2kernel_ptr((const void *) arg[0]);
		f->eax = remove((const char *) arg[0]);
		break;
      }
    case SYS_OPEN:{
		get_arg(f, &arg[0], 1);
		arg[0] = user2kernel_ptr((const void *) arg[0]);
		f->eax = open((const char *) arg[0]);
		break; 		
      }
    case SYS_FILESIZE:{
		get_arg(f, &arg[0], 1);
		f->eax = file_size(arg[0]);
		break;
      }
    case SYS_READ:{
		get_arg(f, &arg[0], 3);
		check_valid_buffer((void *) arg[1], (unsigned) arg[2]);
		arg[1] = user2kernel_ptr((const void *) arg[1]);
		f->eax = read(arg[0], (void *) arg[1], (unsigned) arg[2]);
		break;
      }
    case SYS_WRITE:{ 
		get_arg(f, &arg[0], 3);
		check_valid_buffer((void *) arg[1], (unsigned) arg[2]);
		arg[1] = user2kernel_ptr((const void *) arg[1]);
		f->eax = write(arg[0], (const void *) arg[1], (unsigned) arg[2]);
		break;
      }
    case SYS_SEEK:{
		get_arg(f, &arg[0], 2);
		seek(arg[0], (unsigned) arg[1]);
		break;
      } 
    case SYS_TELL:{ 
		get_arg(f, &arg[0], 1);
		f->eax = tell(arg[0]);
		break;
      }
    case SYS_CLOSE:{ 
		get_arg(f, &arg[0], 1);
		close(arg[0]);
		break;

      }
  default:
    printf("Invalid system call\n");
    kill();
    break;
  }
}

/*
  Reading the instructions revealed a very simple way to halt the program
*/
void halt (void){
	//In shutdown.c
  shutdown_power_off();
}

/*
Checks if current thread parent is alive and if it is sets return status back to the kernal. 
Also in instructions. Status of 0 is success and non zero indicates errors.
*/
void exit (int status){
  struct thread *cur = thread_current();
  if (thread_alive(cur->parent))
    {
      cur->cp->status = status;
    }
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}
/*
Instructions in for this in guide.
Runs the executable whose name is given in cmd_line, passing any given arguments, and
returns the new process’s program ID (pid). Must return pid -1, which otherwise should
not be a valid pid, if the program cannot load or run for any reason. Thus, the parent
process cannot return from the exec until it knows whether the child process successfully
loaded its executable.
*/
pid_t exec (const char *cmd_line){
	//Intitalize somne stuff to use
  pid_t pid = process_execute(cmd_line);
  struct child_process* childProcess = get_child_process(pid);
  ASSERT(childProcess);
  while (childProcess->load == NOT_LOADED){
	  /*
		Barrier is a special statement that prevents the compiler from making assumptions
		about the state of memory across the barrier. The compiler will not reorder 
		reads or writes of variables across the barrier or assume that a variable’s 
		value is unmodified across the barrier, except for local variables whose address 
		is never taken.
	  */
	  //barrier() used instead of thread_yeild(), tried thread_yeild() and no workie.
      barrier();
    }
  if (childProcess->load == LOAD_FAIL){
      return ERROR;
    }
  return pid;
}

/*
Do I need to explain this? In guide as well.
Waits for a child process pid and retrieves the child’s exit status.
If pid is still alive, waits until it terminates. Then, returns the status that pid passed 
to exit. If pid did not call exit(), but was terminated by the kernel (e.g. killed due to 
an exception), wait(pid) must return -1. It is perfectly legal for a parent process to wait 
for child processes that have already terminated by the time the parent calls wait, but the 
kernel must still allow the parent to retrieve its child’s exit status, or learn that the 
child was terminated by the kernel. Process_wait is in process.c.
*/
int wait (pid_t pid){
  return process_wait(pid);
}

/*
In guide.
Creates a new file called file initially initial_size bytes in size. Returns true if successful,
false otherwise. Creating a new file does not open it: opening the new file is a separate
operation which would require a open system call.
*/

bool create (const char *file, unsigned initial_size){
  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return success;
}

/*
In guide:
Deletes the file called file. Returns true if successful, false otherwise. A file may be
removed regardless of whether it is open or closed, and removing an open file does not close
it.
*/

bool remove (const char *file){
  lock_acquire(&filesys_lock);
  bool success = filesys_remove(file);
  lock_release(&filesys_lock);
  return success;
}

/*
In guide.
Opens the file called file. Returns a nonnegative integer handle called a "file descriptor"
(file_d), or -1 if the file could not be opened.File descriptors numbered 0 and 1 are reserved for the console: file_d 0 (STDIN_FILENO) is
standard input, file_d 1 (STDOUT_FILENO) is standard output. T*/
int open (const char *file){
  lock_acquire(&filesys_lock);
  
  int file_d;

  //use filesys_open to open file and set it to a struct file
  struct file *fileahhh = filesys_open(file);
  
  //Error checking, if not there you would always add a file.
  if (!fileahhh){
      lock_release(&filesys_lock);
      return ERROR;
    }

  //see below on what this does
  file_d = pro_ADD_FILE(fileahhh);

  //Regardless release the lock
  lock_release(&filesys_lock);
  return file_d;
}

/*
In guide.
Returns the size, in bytes, of the file open as fd.
*/
int file_size (int file_d){
  lock_acquire(&filesys_lock);
  struct file *files = pro_GET_FILE(file_d);
  
  //All this is doing is error checking that is it.
  if (!files){
      lock_release(&filesys_lock);
      return ERROR;
    }
  
  //set to size and return it.
  int size = file_length(files);
  lock_release(&filesys_lock);
  return size;
}
/*
Part of SYS_READ
In guide.
Reads size bytes from the file open as file_d into buffer. Returns the number of bytes actually
read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of
file). File_d 0 reads from the keyboard using input_getc().
*/
int read (int file_d, void *buffer, unsigned size){
	struct file* files;
	int bytes;

	//Has to be equal to 0, if not like 1 it is stdin
  if (file_d == STDIN_FILENO){
      unsigned i;
	  //local booth is homage to E-40
      uint8_t* local_booth = (uint8_t *) buffer;
      for (i = 0; i < size; i++){
		 local_booth[i] = input_getc();
		}
      return size;
    }
  
  //Probably seen this many times now
  lock_acquire(&filesys_lock);
  files = pro_GET_FILE(file_d);

  //If not file
  if (!files){
      lock_release(&filesys_lock);
      return ERROR;
    }

  bytes = file_read(files, buffer, size);

  lock_release(&filesys_lock);
  return bytes;
}

/*
Part of SYS_WRITE, almost like read
In guide.

Writes size bytes from buffer to the open file file_d. Returns the number of bytes actually written, 
which may be less than size if some bytes could not be written. 

Writing past end-of-file would 
normally extend the file, but file growth is not implemented by the basic file system. The expected 
behavior is to write as many bytes as possible up to end-of-file and return the actual number written, 
or 0 if no bytes could be written at all. 

File_d 1 writes to the console. Your code to write to the 
console should write all of buffer in one call to putbuf(), at least as long as size is not bigger 
than a few hundred bytes. (It is reasonable to break up larger buffers.) Otherwise, lines of text 
output by different processes may end up interleaved on the console, confusing both human readers 
and our grading scripts.
*/
int write (int file_d, const void *buffer, unsigned size){
	struct file* files;
	int bytes;

	//This should equal 1
  if (file_d == STDOUT_FILENO){
      //here is putbuf
	  putbuf(buffer, size);
      return size;
    }

  //omg again, yes again!
  lock_acquire(&filesys_lock);
  files = pro_GET_FILE(file_d);
  if (!files){
      lock_release(&filesys_lock);
      return ERROR;
    }
  
  //This is new!!!
  bytes = file_write(files, buffer, size);
  lock_release(&filesys_lock);
  return bytes;
}

/*
In guide
Changes the next byte to be read or written in open file file_d to position, expressed in bytes
from the beginning of the file. (Thus, a position of 0 is the file’s start.)

A seek past the current end of a file is not an error. A later read obtains 0 bytes, indicating
end of file. A later write extends the file, filling any unwritten gap with zeros. (However,
in Pintos files have a fixed length until project 4 is complete, so writes past end of file will
return an error.) These semantics are implemented in the file system and do not require any
special effort in system call implementation.
*/

void seek (int file_d, unsigned position){
  
	//Almost the same as the above functions
	lock_acquire(&filesys_lock);
  struct file *files = pro_GET_FILE(file_d);
  if (!files){
      lock_release(&filesys_lock);
      return;
    }
  file_seek(files, position);
  lock_release(&filesys_lock);
}

/*
In guide 
Returns the position of the next byte to be read or written in open file file_d, expressed in bytes
from the beginning of the file.
*/
unsigned tell (int file_d){
  
	//Are we there yet? Have you really read all this just curious
	//Yes this is how all this should look.
	lock_acquire(&filesys_lock);
  struct file *files = pro_GET_FILE(file_d);
  off_t position;
  if (!files){
      lock_release(&filesys_lock);
      return ERROR;
    }

  //this is in file.c and file.h and returns a position
  position = file_tell(files);
  lock_release(&filesys_lock);
  return position;
}

/*
In guide
Closes file descriptor file_d. Exiting or terminating a process implicitly closes all its open file
descriptors, as if by calling this function for each one.
*/

void close (int file_d){

	//short and sweet.
  lock_acquire(&filesys_lock);
  process_close_file(file_d);
  lock_release(&filesys_lock);
}

/*
Checks to see if f->esp is a valid pointer.
Helper function used in above functions.
*/

void valid_ptr (const void *vaddr){
  if (!is_user_vaddr(vaddr) || vaddr < USER_VADDR_BOTTOM){
    exit(ERROR);
  }
  //return (is_user_vaddr(vaddr) && 
    //pagedir_get_page(thread_current()->pagedir,vaddr)!=NULL);
  
}



int user2kernel_ptr(const void *vaddr){
  //TODO: Need to check if all bytes within range are correct
  //for strings + buffers
//this doesn't check if pointer in the system call is within PHYS_BASE
	//nor does the pointer requested belongs to a page in the virtual memory
	//does check if it is a valid pointer so 
	/*
________$$$$
_______$$__$
_______$___$$
_______$___$$
_______$$___$$
________$____$$
________$$____$$$
_________$$_____$$
_________$$______$$
__________$_______$$
____$$$$$$$________$$
__$$$_______________$$$$$$
_$$____$$$$____________$$$
_$___$$$__$$$____________$$
_$$________$$$____________$
__$$____$$$$$$____________$
__$$$$$$$____$$___________$
__$$_______$$$$___________$
___$$$$$$$$$__$$_________$$
____$________$$$$_____$$$$
____$$____$$$$$$____$$$$$$
_____$$$$$$____$$__$$
_______$_____$$$_$$$
________$$$$$$$$$$
	*/

  valid_ptr(vaddr);

  void *pointer = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!pointer){
      exit(ERROR);
    }
  return (int) pointer;
}

/*
Simple function to add process files. OG_file is from the function above.
*/
int pro_ADD_FILE (struct file *OG_file){

	//Making room with a new struct
  struct process_file *processFile = malloc(sizeof(struct process_file));
//sete OG_file to processFile,ile
  processFile->file = OG_file;

  //set current thread file descriptor to the process file descriptor
  processFile->fd = thread_current()->fd;

  //increment current thread file descriptor 
  thread_current()->fd++;

  //push the current thread pointer file list and processFile element
  list_push_back(&thread_current()->file_list, &processFile->elem);

  //return processFile file descriptor
  return processFile->fd;
}

/*
Process get file function. Essentially a function that will populate a file and return 
the struct containing the data.
*/
struct file* pro_GET_FILE (int file_d){
  struct thread *thread = thread_current();
  struct list_elem *elem;
  struct process_file* processFile;

  for (elem = list_begin (&thread->file_list); elem != list_end (&thread->file_list);
       elem = list_next (elem)){
          processFile = list_entry (elem, struct process_file, elem);
          if (file_d == processFile->fd)
			return processFile->file;
	    }
  return NULL;
}
/*
-----------------------------------------------------------------
*/

void process_close_file (int file_d)
{
  struct thread *thread = thread_current();
  struct list_elem *next, *e = list_begin(&thread->file_list);
  struct process_file* processFile;

  while (e != list_end (&thread->file_list)){
      next = list_next(e);
      processFile = list_entry (e, struct process_file, elem);
	  if (file_d == processFile->fd || file_d == CLOSE_ALL) {
		  file_close(processFile->file);
		  list_remove(&processFile->elem);
		  free(processFile);
		  if (file_d != CLOSE_ALL)
			  return;
	  }
      e = next;
    }
}
/*
This initializes a child process and returns it as a struct. Used in syscall.c and thread.c
*/
struct child_process* ADD_childProcess (int pid){
  
	//Making room for a new struct
	struct child_process* childProcess = malloc(sizeof(struct child_process));
  
	//pid set to the child process pid
	childProcess->pid = pid;
  
	//rest of the values set as default values
	childProcess->load = NOT_LOADED;
	childProcess->wait = false;
	childProcess->exit = false;

  //Initializes a lock for this process
  lock_init(&childProcess->wait_lock);

  //push!
  list_push_back(&thread_current()->child_list,
		 &childProcess->elem);

  //return the struct
  return childProcess;
}

/*
All this does is return a child process
*/
struct child_process* get_child_process (int pid){
  //set up a couple structs, one set to the current thread
	struct thread *thread = thread_current();
  struct list_elem *elem;

  //go through each process
  for (elem = list_begin (&thread->child_list); elem != list_end (&thread->child_list);
       elem = list_next (elem)){
      
	  //save the process struct
	  struct child_process *childProcess = list_entry (elem, struct child_process, elem);
      
	  //if the child pid is the same and the pid coming into the function return it
	  if (pid == childProcess->pid)
			return childProcess;
	    }
  return NULL;
}
/*
removes only one process
*/
void remove_child_process (struct child_process *childProcess){
  list_remove(&childProcess->elem);
  free(childProcess);
}
/*
Removes multiple child processes
*/
void remove_child_processes (void){
  struct thread *threads = thread_current();
  struct list_elem *next, *elem = list_begin(&threads->child_list);

  while (elem != list_end (&threads->child_list)){
      next = list_next(elem);
      struct child_process *childProcess = list_entry (elem, struct child_process,
					     elem);
      list_remove(&childProcess->elem);
      free(childProcess);
      elem = next;
    }
}

/*
Getting an arguement function
*/
void get_arg (struct intr_frame *frame, int *arg, int num){
  int i;
  int *pointer;
  
  //go through every frame stack pointer, check if it is valid then set the pointer address
  //to arg[i]
  for (i = 0; i < num; i++){
      pointer = (int *) frame->esp + i + 1;
      valid_ptr((const void *) pointer);
      arg[i] = *pointer;
    }
}

/*
Checks if a buffer is valid or not
*/
void check_valid_buffer (void* buffer, unsigned size){
  unsigned i;
  char* locBuffer = (char *) buffer;

  //Go through the size and check if the local buffer is valid or not and just
  //keep going.
  for (i = 0; i < size; i++){
      valid_ptr((const void*) locBuffer);
      locBuffer++;
    }
}

void kill (){
  exit(-1);
}
