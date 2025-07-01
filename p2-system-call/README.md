---
title: CS 537 Project 2
layout: default
---

# CS537 Spring 2025, Project 2

## Updates
* Updated README to specify crash string: `it's a feature, not a bug!`
* While running the test script, make sure the only userprogram you have is crashtest. Too many userprograms can affect `run-tests.sh` 



## Learning Objectives

In this assignment, you will create a system call in xv6 which crashes the system if a special phrase has been written previously by a process using the  `write()` system call.

* Understand the xv6 OS, a simple UNIX operating system.
* Learn to build and customize the xv6 OS
* Understand how system calls work in general
* Understand how to implement a new system call
* Be able to navigate a larger codebase and find the information needed to make changes.



## Project Overview

In this project you will add a new system call to the xv6 operating system. More specifically, you will have to implement a system call named sys_crash with the following signature:
int crash(void)
In this project you will implement a system call `crash()` that when called will the ability to crash the system. Whether `crash()` crashes the system depends on whether the process uses the `write()` call to write the crash string `"it's a feature, not a bug!"`.
```
int fd_stdout = 1; //file descriptor for stdout stream
char *crash_string = "it's a feature, not a bug!";
write(fd_stdout, crash_string, strlen(crash_string));
crash();
```

If the process has used `write()` to crash string, then using `crash()` should result in an output:

```
$ ./crashtest_prototype
lapicid 0: panic: XV6_TEST_ERROR CRASH SYSTEM CALL INITIATED. CRASHING SYSTEM.

 801058a9 80104a89 80105b1d 801058be 0 0 0 0 0 0QEMU: Terminated
```
*Output taken from a prototype of crashtest*

### Task 1: Download and Run xv6

The xv6 operating system is present inside `p2/solution` folder . This directory also contains instructions on how to get the operating system up and running in the `README.md` file.

- Simply use `git clone` to acquire a fresh copy of p2 and make your changes to the `solution` folder as mentioned above.

You may find it helpful to go through some of these videos from earlier semesters:

1.  [Discussion video](https://www.youtube.com/watch?v=vR6z2QGcoo8&ab_channel=RemziArpaci-Dusseau) - Remzi Arpaci-Dusseau. 
2. [Discussion video](https://mediaspace.wisc.edu/media/Shivaram+Venkataraman-+Psychology105+1.30.2020+5.31.23PM/0_2ddzbo6a/150745971) - Shivaram Venkataraman.
3. [Some background on xv6 syscalls](https://github.com/remzi-arpacidusseau/ostep-projects/blob/master/initial-xv6/background.md) - Remzi Arpaci-Dusseau.
4. [xv6 Debugging guide](https://git.doit.wisc.edu/cdis/cs/courses/cs537/spring25/public/discussion/-/blob/main/xv6-debugging.md?ref_type=heads)

**To exit QEMU type ctrl+a, and press x**

### Task 2: Modify the `write()` system call
You will need to modify the `write()` system call to detect the crash string and remember if it was ever passed.

The crash string is valid if it the string **starts with and contains** `"it's a feature, not a bug!"`.

You will have to find the code for the `write()` system call and add code to see if the proved buffer contains the crash string. If it does, you should set a flag in the process structure remembering that the process can now be crashed (and make sure to initialize it to zero for a new process)

You should also make sure the changes made to process structure are reflected in the `userinit()`, `fork()` and `exec()` implementations as they deal with starting/modifying processes as well. A process inherits it's crash behavior from it's parent, unless a new executable is run within the process, in which case, the crash behavior resets.

### Task 3: Create the crash system call
You will have to add a new system call and modify the system call table and handler to contain the new call.

When `crash()` is called, it should check if the process previously passed the crash string to `write()`; if so, it should crash the system. If not, it should return an error.

When crashing, xv6 must print **XV6_TEST_ERROR CRASH SYSTEM CALL INITIATED. CRASHING SYSTEM.**

You will need to add a userprogram called crashtest that effectively demonstrates the functionality of the crash system call. You will need to show:
- `crash()` returns -1 when no `write()` system call has been made
- `crash()` returns -1 when `write()` system call is made with something other than crash string
- `crash()` crashes the system after `write()` system call is made with crash string


### Task 4: Add crashtest user level program
#### Adding a user-level program to xv6

As an example we will add a new user level program called `test` to the xv6 operating system.

```
// test.c
{
  printf(1, "The process ID is: %d\n", getpid());
  exit();
}
```
    

Now we need to edit the Makefile so our program compiles and is included in the file system when we build XV6. Add the name of your program to `UPROGS` in the Makefile, following the format of the other entries.

Run `make qemu-nox` again and then run `test` in the xv6 shell. It should print the PID of the process.

You may want to write some new user-level programs to help test your implementation of `crash()`.

#### crashtest

You will need to add a userprogram called crashtest that effectively demonstrates the functionality of the crash system call. You will need to show:

* `crash()` returns -1 when no `write()` system call has been made
* `crash()` returns -1 when `write()` system call is made with something other than crash string
* `crash()` crashes the system after `write()` system call is made with crash string

## Project Details

*   Your project should (hopefully) pass the tests we supply.
*   **Your code will be tested in the CSL Linux environment (Ubuntu 22.04.3 LTS). These machines already have qemu installed. Please make sure you test it in the same environment.**
*   Include a file called README.md describing the implementation in the top level directory. This file should include your name, your cs login, you wisc ID and email, and the status of your implementation. If it all works then just say that. If there are things which don't work, let us know. Please **list the names of all the files you changed in `solution`**, with a brief description of what the change was. This will **not** be graded, so do not sweat it.
*   Please note that `solution` already has a file called README, do not modify or delete this, just include a separate file called README.md with the above details, it will not impact the other readme or cause any issues. If you remove the xv6 README, it will cause compilation errors.
*   If applicable, a **document describing online resources used** called **resources.txt**. You are welcome to use online resources that can help you with your assignment. **We don't recommend you use Large-Language Models such as ChatGPT.**

* For this course in particular we have seen these tools give close, but not quite right examples or explanations, that leave students more confused if they don't already know what the right answer is. Be aware that when you seek help from the instructional staff, we will not assist with working with these LLMs and we will expect you to be able to walk the instructional staff member through your code and logic. 
    * Online resources (e.g. stack overflow) and generative tools are transforming many industries including computer science and education.  However, if you use online sources, you are required to turn in a document describing your uses of these sources. 
    * Indicate in this document what percentage of your solution was done strictly by you and what was done utilizing these tools. Be specific, indicating sources used and how you interacted with those sources. 
    * Not giving credit to outside sources is a form of plagiarism. It can be good practice to make comments of sources in your code where that source was used. 
    * You will not be penalized for using LLMs or reading posts, but you should not create posts in online forums about the projects in the course. The majority of your code should also be written from your own efforts and you should be able to explain all the code you submit.

## Suggested Workflow
- Add a system call to crash the system
- Create a user program (`crashtest.c`) to test whether crash functionality works.
- Modify the OS to keep track of whether a process has written the crash string or not.
- Make changes to the write call such that it can inform the OS that a process has written the crash string
- Modify the crash system to call to check whether the process has written the crash string


## Administrivia 
- **Due Date** by February 13, 2025 at 11:59 PM 
- Questions: We will be using Piazza for all questions.
- Collaboration: The assignment has to be done by yourself. Copying code (from others) is considered cheating. [Read this](http://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not. Please help us all have a good semester by not doing this.
- This project is to be done on the [lab machines](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/), so you can learn more about programming in C on a typical UNIX-based platform (Linux).
- A few sample tests are provided in the project repository. To run them, execute `run-tests.sh` in the `tests/` directory. Try `run-tests.sh -h` to learn more about the testing script. Note these test cases are not complete, and you are encouraged to create more on your own. 
- **Slip Days**: 
  - In case you need extra time on projects, you each will have 2 slip days for the first 3 projects and 2 more for the final three. After the due date we will make a copy of the handin directory for on time grading. 
  - To use a slip days or turn in your assignment late you will submit your files with an additional file that contains **only a single digit number**, which is the number of days late your assignment is (e.g. 1, 2, 3). Each consecutive day we will make a copy of any directories which contain one of these `slipdays.txt` files. 
  - `slipdays.txt` must be present at **the top-level directory** of your submission. 
  - Example project directory structure. (some files are omitted)
  ```
  p2/
  ├─ solution/
  │  ├─ README.md
  │  ├─ crashtest.c
  │  ├─ Makefile
  |  ├─ (rest of xv6, with your modifications)
  ├─ tests/
  ├─ README.md
  ├─ slipdays.txt
  ```
  - We will track your slip days and late submissions from project to project and begin to deduct percentages after you have used up your slip days.
  - After using up your slip days you can get up to 80% if turned in 1 day late, 60% for 2 days late, and 40% for 3 days late, but for any single assignment we won't accept submissions after the third days without an exception. This means if you use both of your individual slip days on a single assignment you can only submit that assignment one additional day late for a total of 3 days late with a 20% deduction.
  - Any exception will need to be requested from the instructors.

  - Example of `slipdays.txt`:

```sh
$ cat slipdays.txt
1
```

## Submitting your work
- Run `submission.sh` 
- Download generated p2.tar file
- Upload it to Canvas
  * Links to Canvas assignment: 
  * [Prof. Mike Swift's class](https://canvas.wisc.edu/courses/434150/assignments/2627847) 
  * [Prof. Ali Abedi's class](https://canvas.wisc.edu/courses/434155/assignments/2627854) 


## Notes and Hints
- Take a look at `sysfile.c` and `sysproc.c` to find tips on how to implement syscall logic. 
- `proc.c` and `proc.h` are files you can look into to get an understanding of how process related structs look like. You can use the proc struct to store information.
- It is important to remember that the flavour of C used in xv6 differs from the standard C library (stdlib.c) that you might be used to. For example, in the small userspace example shown above, notice that printf takes in an extra first argument(the file descriptor), which differs from the standard printf you might be used to. It is best to use `grep` to search around in xv6 source code files to familiarize yourself with how such functions work.
- `user.h` contains a list of system calls and userspace functions (defined in `ulib.c`) available to you, while in userspace. It is important to remember that these functions can't be used in kernelspace and it is left as an exercise for you to figure out what helper functions are necessary(if any) in the kernel for successful completion of this project.
- You will need to add a new system call, but also modify the `write()` system call so it can scan incoming buffers to check if the crash string is found in the buffer or not.
- You will need to find a way to keep track of whether the process has written the crash string or not. 
- You will have to make use of the `panic()` function to crash the system.

