#include "shell.h"

typedef struct proc {
  pid_t pid;    /* process identifier */
  int state;    /* RUNNING or STOPPED or FINISHED */
  int exitcode; /* -1 if exit status not yet received */
} proc_t;

typedef struct job {
  pid_t pgid;            /* 0 if slot is free */
  proc_t *proc;          /* array of processes running in as a job */
  struct termios tmodes; /* saved terminal modes */
  int nproc;             /* number of processes */
  int state;             /* changes when live processes have same state */
  char *command;         /* textual representation of command line */
} job_t;

static job_t *jobs = NULL;          /* array of all jobs */
static int njobmax = 1;             /* number of slots in jobs array */
static int tty_fd = -1;             /* controlling terminal file descriptor */
static struct termios shell_tmodes; /* saved shell terminal modes */

static void sigchld_handler(int sig) {
  int old_errno = errno;
  pid_t pid;
  int status;
  /* TODO: Change state (FINISHED, RUNNING, STOPPED) of processes and jobs.
   * Bury all children that finished saving their status in jobs. */
#ifdef STUDENT
  (void)status;
  (void)pid;

  // liczniki dla ukonczonych, zatrzymanych i uruchomionych procesow
  int finished_n, running_n, stopped_n;

  // iterujemy sie po wszystkich zadaniach
  for (int i = 0; i < njobmax; i++) {
    finished_n = 0;
    running_n = 0;
    stopped_n = 0;

    // iterujemy sie po wszystkich procesach w danym zadaniu
    for (int j = 0; j < jobs[i].nproc; j++) {

      // zwracamy pid dziecka ktorego stan zostal zmieniony,
      // jezeli wystapil blad- zwracamy -1, jezeli dzieci sa,
      // ale ich stan jeszcze sie nie zmienil, zwracamy 0
      if ((pid = waitpid(jobs[i].proc[j].pid, &status,
                         WNOHANG | WUNTRACED | WCONTINUED)) > 0) {

        // jezeli proces zmienil stan na zakonczony,
        // albo proces zostal zabity sygnalem,
        // to zmieniamy stan procesu w liscie zadan na ukonczony,
        // a zmieniony status zapisujemy do zmiennej exitcode
        // inkrementujemy licznik ukonczonych procesow
        // analogicznie dla stanu zmienionego
        // na zakonczony i zmienionego na uruchomiony
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
          jobs[i].proc[j].state = FINISHED;
          jobs[i].proc[j].exitcode = status;
          finished_n++;
        } else if (WIFSTOPPED(status)) {
          jobs[i].proc[j].state = STOPPED;
          stopped_n++;
        } else if (WIFCONTINUED(status)) {
          jobs[i].proc[j].state = RUNNING;
          running_n++;
        }
      } else {

        // jezeli stan procesu nie zmienil sie, to sprawdzamy
        // jaki stan jest zapisany w strukturze jobs
        // nastepnie inkrementujemy odp[owiedni licznik
        if (jobs[i].proc[j].state == RUNNING) {
          running_n++;
        } else if (jobs[i].proc[j].state == STOPPED) {
          stopped_n++;
        } else if (jobs[i].proc[j].state == FINISHED) {
          finished_n++;
        }
      }
    }
    // zmieniamy stan zadania,
    // jezeli wszystkie zyjace procedury maja dany stan
    if (finished_n == jobs[i].nproc) {
      jobs[i].state = FINISHED;
    } else if (stopped_n == jobs[i].nproc) {
      jobs[i].state = STOPPED;
    } else if (running_n == jobs[i].nproc) {
      jobs[i].state = RUNNING;
    }
  }
#endif /* !STUDENT */
  errno = old_errno;
}

/* When pipeline is done, its exitcode is fetched from the last process. */
static int exitcode(job_t *job) {
  return job->proc[job->nproc - 1].exitcode;
}

static int allocjob(void) {
  /* Find empty slot for background job. */
  for (int j = BG; j < njobmax; j++)
    if (jobs[j].pgid == 0)
      return j;

  /* If none found, allocate new one. */
  jobs = realloc(jobs, sizeof(job_t) * (njobmax + 1));
  memset(&jobs[njobmax], 0, sizeof(job_t));
  return njobmax++;
}

static int allocproc(int j) {
  job_t *job = &jobs[j];
  job->proc = realloc(job->proc, sizeof(proc_t) * (job->nproc + 1));
  return job->nproc++;
}

int addjob(pid_t pgid, int bg) {
  int j = bg ? allocjob() : FG;
  job_t *job = &jobs[j];
  /* Initial state of a job. */
  job->pgid = pgid;
  job->state = RUNNING;
  job->command = NULL;
  job->proc = NULL;
  job->nproc = 0;
  job->tmodes = shell_tmodes;
  return j;
}

static void deljob(job_t *job) {
  assert(job->state == FINISHED);
  free(job->command);
  free(job->proc);
  job->pgid = 0;
  job->command = NULL;
  job->proc = NULL;
  job->nproc = 0;
}

static void movejob(int from, int to) {
  assert(jobs[to].pgid == 0);
  memcpy(&jobs[to], &jobs[from], sizeof(job_t));
  memset(&jobs[from], 0, sizeof(job_t));
}

static void mkcommand(char **cmdp, char **argv) {
  if (*cmdp)
    strapp(cmdp, " | ");

  for (strapp(cmdp, *argv++); *argv; argv++) {
    strapp(cmdp, " ");
    strapp(cmdp, *argv);
  }
}

void addproc(int j, pid_t pid, char **argv) {
  assert(j < njobmax);
  job_t *job = &jobs[j];

  int p = allocproc(j);
  proc_t *proc = &job->proc[p];
  /* Initial state of a process. */
  proc->pid = pid;
  proc->state = RUNNING;
  proc->exitcode = -1;
  mkcommand(&job->command, argv);
}

/* Returns job's state.
 * If it's finished, delete it and return exitcode through statusp. */
static int jobstate(int j, int *statusp) {
  assert(j < njobmax);
  job_t *job = &jobs[j];
  int state = job->state;

  /* TODO: Handle case where job has finished. */
#ifdef STUDENT
  (void)exitcode;
  (void)deljob;

  // zwracamy status zadania,
  // jezeli zadanie zostalo ukonczone
  // to usuwamy je i zmieniamy zmienna statusp na exitcode
  if (state == FINISHED) {
    *statusp = exitcode(job);
    deljob(job);
  }
#endif /* !STUDENT */

  return state;
}

char *jobcmd(int j) {
  assert(j < njobmax);
  job_t *job = &jobs[j];
  return job->command;
}

/* Continues a job that has been stopped. If move to foreground was requested,
 * then move the job to foreground and start monitoring it. */
bool resumejob(int j, int bg, sigset_t *mask) {
  if (j < 0) {
    for (j = njobmax - 1; j > 0 && jobs[j].state == FINISHED; j--)
      continue;
  }

  if (j >= njobmax || jobs[j].state == FINISHED)
    return false;

    /* TODO: Continue stopped job. Possibly move job to foreground slot. */
#ifdef STUDENT
  (void)movejob;
  msg("[%d] continue '%s'\n", j, jobcmd(j));

  // zmieniamy stan na running
  jobs[j].state = RUNNING;
  // wysylamy sygnal SIGCONT dla wszystkich procesow
  // z grupy procesow ktora chcemy wznowic
  if (bg) {
    kill(-jobs[j].pgid, SIGCONT);
  }

  // jezeli chcemu zmienic zadanie na pierwszoplanowe,
  // to dodatkowo zmieniamy pozycje w tablicy zadan na 0
  // (odpowiadajace zadaniu pierwszoplanowemu)
  // ustawiamy pierwszoplanowa grupe procesow na to zadanie za pomoca setfgprp
  // oraz obserwujemy zadanie monitorjob'em
  if (!bg) {
    movejob(j, 0);
    setfgpgrp(jobs[0].pgid);
    kill(-jobs[0].pgid, SIGCONT);
    monitorjob(mask);
  }
#endif /* !STUDENT */

  return true;
}

/* Kill the job by sending it a SIGTERM. */
bool killjob(int j) {
  if (j >= njobmax || jobs[j].state == FINISHED)
    return false;
  debug("[%d] killing '%s'\n", j, jobs[j].command);

  /* TODO: I love the smell of napalm in the morning. */
#ifdef STUDENT
  // wysylamy sygnal terminujacy dla wszystkich procesow z danej grupy
  // wysylamy sigcont'a aby procesy,
  // ktore sa zatrzymane mogly zareagowac na sigterm'a
  kill(-jobs[j].pgid, SIGTERM);
  kill(-jobs[j].pgid, SIGCONT);
#endif /* !STUDENT */

  return true;
}

/* Report state of requested background jobs. Clean up finished jobs. */
void watchjobs(int which) {
  for (int j = BG; j < njobmax; j++) {
    if (jobs[j].pgid == 0)
      continue;

      /* TODO: Report job number, state, command and exit code or signal. */
#ifdef STUDENT
    (void)deljob;

    // szukamy zadania o danym stanie
    // (lub raportujemy stan dla wszystkich, jezeli argumentem jest "ALL")
    if (jobs[j].state == which || which == ALL) {
      if (jobs[j].state == FINISHED) {

        // jezeli zadanie zostalo zakonczone
        // to dajemy informacje o statusie z jakim zostal zakonczony
        if (WIFEXITED(exitcode(&jobs[j]))) {
          msg("[%d] exited '%s', status=%d\n", j, jobcmd(j),
              WEXITSTATUS(exitcode(&jobs[j])));
        } else if (WIFSIGNALED(exitcode(&jobs[j]))) {
          // jezeli zostal zabity sygnalem,
          // to dajemy informacje jaki to byl sygnal
          msg("[%d] killed '%s' by signal %d\n", j, jobcmd(j),
              exitcode(&jobs[j]));
        }

        // usuwamy zakonczone zadania
        deljob(&jobs[j]);
      } else if (jobs[j].state == RUNNING) {
        // raportujemy jezeli stan zadania jest "running" lub "stopped"
        msg("[%d] running '%s'\n", j, jobcmd(j));
      } else if (jobs[j].state == STOPPED) {
        msg("[%d] suspended '%s'\n", j, jobcmd(j));
      }
    }
#endif /* !STUDENT */
  }
}

/* Monitor job execution. If it gets stopped move it to background.
 * When a job has finished or has been stopped move shell to foreground. */
int monitorjob(sigset_t *mask) {
  int exitcode = 0, state;

  /* TODO: Following code requires use of Tcsetpgrp of tty_fd. */
#ifdef STUDENT
  (void)jobstate;
  (void)exitcode;
  (void)state;

  // pobieramy stan zadania (jezeli zadanie jest zakonczone
  // to dostajemy rowniez exitcode, a zadanie zostaje usuniete)
  state = jobstate(0, &exitcode);

  // dopoki zadanie nie zostanie zatrzymane
  // lub zakonczone, to czekamy na sygnal SIGCHLD sigsuspendem
  while (state == RUNNING) {
    sigsuspend(mask);
    state = jobstate(0, &exitcode);
  }

  // wyjscie z petli oznacza ze stan procesu zmienil sie
  // na zakonczony lub zatrzymany,
  // wiec ponizej jest obsluga tych przypadkow

  // jezeli zadanie zostanie zatrzymane
  // to dajemy je na drugi plan
  if (state == STOPPED) {
    movejob(0, allocjob());
  }

  // ustawiamy shella na proces pierwszoplanowy
  // i przywracamy domyslne parametry terminala
  setfgpgrp(getpgrp());
  Tcsetattr(tty_fd, TCSADRAIN, &shell_tmodes);
#endif /* !STUDENT */

  return exitcode;
}

/* Called just at the beginning of shell's life. */
void initjobs(void) {
  struct sigaction act = {
    .sa_flags = SA_RESTART,
    .sa_handler = sigchld_handler,
  };

  /* Block SIGINT for the duration of `sigchld_handler`
   * in case `sigint_handler` does something crazy like `longjmp`. */
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  Sigaction(SIGCHLD, &act, NULL);

  jobs = calloc(sizeof(job_t), 1);

  /* Assume we're running in interactive mode, so move us to foreground.
   * Duplicate terminal fd, but do not leak it to subprocesses that execve. */
  assert(isatty(STDIN_FILENO));
  tty_fd = Dup(STDIN_FILENO);
  fcntl(tty_fd, F_SETFD, FD_CLOEXEC);

  /* Take control of the terminal. */
  Tcsetpgrp(tty_fd, getpgrp());

  /* Save default terminal attributes for the shell. */
  Tcgetattr(tty_fd, &shell_tmodes);
}

/* Called just before the shell finishes. */
void shutdownjobs(void) {
  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Kill remaining jobs and wait for them to finish. */
#ifdef STUDENT

  // iterujemy po zadaniach i zabijamy je,
  // czekajac na otrzymanie sygnalu sigsuspendem
  for (int i = 0; i < njobmax; i++) {
    killjob(i);
    while (jobs[i].state == RUNNING) {
      sigsuspend(&mask);
    }
  }
#endif /* !STUDENT */

  watchjobs(FINISHED);

  Sigprocmask(SIG_SETMASK, &mask, NULL);

  Close(tty_fd);
}

/* Sets foreground process group to `pgid`. */
void setfgpgrp(pid_t pgid) {
  Tcsetpgrp(tty_fd, pgid);
}
