# Shell
#### Project carried out as part of Operating Systems class.
#### The code I wrote is in the files: command.c, jobs.c, shell.c and is marked by the directive `#ifdef STUDENT`.

[detailed description](https://github.com/julgitt/Shell/blob/main/projekt-shell.pdf)

----

#### It handles among others job control with the use of the commands:
  - fg [n]:  changes a stopped or running background job to a foreground job,
  - bg [n]: changes the state of the secondary job from stopped to active,
  - kill %n: kills the processes belonging to the job with the given number,
  - jobs: displays the status of secondary jobs.

#### Pipes and redirection, e.g:
    grep foo test.txt > test.txt | wc -l.
  
