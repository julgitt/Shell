#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#define DEBUG 0
#include "shell.h"

sigset_t sigchld_mask;

static void sigint_handler(int sig) {
  /* No-op handler, we just need break read() call with EINTR. */
  (void)sig;
}

/* Rewrite closed file descriptors to -1,
 * to make sure we don't attempt do close them twice. */
static void MaybeClose(int *fdp) {
  if (*fdp < 0)
    return;
  Close(*fdp);
  *fdp = -1;
}

/* Consume all tokens related to redirection operators.
 * Put opened file descriptors into inputp & output respectively. */
static int do_redir(token_t *token, int ntokens, int *inputp, int *outputp) {
  token_t mode = NULL; /* T_INPUT, T_OUTPUT or NULL */
  int n = 0;           /* number of tokens after redirections are removed */

  for (int i = 0; i < ntokens; i++) {
    /* TODO: Handle tokens and open files as requested. */
#ifdef STUDENT

    // jezeli w poprzedniej iteracji znaleziono token T_INPUT
    if (mode == T_INPUT) {
      // zamykamy deskryptor plikow (jezeli jakis jest)
      MaybeClose(inputp);

      // tworzymy nowy deskryptor pliku ze sciezka
      // wskazana przez obecny token (ktory byl poprzedzony przez T_INPUT)
      // oraz z flagą O_RDONLY (otwieranie pliku do odczytu)
      // ostatni argument jest w tym przypadku ignorowany
      // (bo nie tworzymy nowego pliku)
      *inputp = Open(token[i], O_RDONLY, S_IRWXU);
      token[i] = T_NULL;
      mode = NULL;
    } else if (mode == T_OUTPUT) {
      // jezeli w poprzedniej iteracji znaleziono token T_OUTPUT
      // zamykamy deskryptor plikow (jezeli jakis jest)
      MaybeClose(outputp);

      // tworzymy nowy deskryptor pliku ze sciezka
      // wskazana przez obecny token
      // (ktory byl poprzedzony przez T_OUTPUT)
      // oraz z flagami O_WRONLY (otwieranie pliku do zapisu),
      // O_CREAT (tworzenie nowego pliku, jezeli go nie ma),
      // O_APPEND (nie nadpisujemy pliku, tylko dopisujemy na koniec)
      // argument mode jest ustawiony na S_IRWXU
      // - odnosi sie do nadania praw uzytkownikowi
      // (w tym przypadku zapisu, czytania, i wykonywania)
      // dla nowo utworzonego pliku (musimy zapisac ten argument
      // poniewaz mamy flage O_CREAT pozwalajaca na tworzenie nowych plikow)
      *outputp = Open(token[i], O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
      token[i] = T_NULL;
      mode = NULL;
    } else {
      // zwiekszamy zwracana liczbe tokenow
      n++;
    }
    // ustawiamy mode na T_INPUT, T_OUTPUT,
    // aby pamietac token w nastepnej iteracji
    if (token[i] == T_INPUT) {
      mode = T_INPUT;
      token[i] = T_NULL;
    } else if (token[i] == T_OUTPUT) {
      mode = T_OUTPUT;
      token[i] = T_NULL;
    }

#endif /* !STUDENT */
  }

  token[n] = NULL;
  return n;
}

/* Execute internal command within shell's process or execute external command
 * in a subprocess. External command can be run in the background. */
static int do_job(token_t *token, int ntokens, bool bg) {
  int input = -1, output = -1;
  int exitcode = 0;

  ntokens = do_redir(token, ntokens, &input, &output);

  if (!bg) {
    if ((exitcode = builtin_command(token)) >= 0)
      return exitcode;
  }

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Start a subprocess, create a job and monitor it. */
#ifdef STUDENT
  pid_t pid = Fork();
  if (pid) { // parent
    // ustawiamy pgid procesu i w rodzicu i w dziecku
    // aby nie doprowadzic do race condition
    setpgid(pid, pid);
    // tworzymy nowe zadanie z danym pidem
    int j = addjob(pid, bg);
    // dodajemy uworzona procedure
    addproc(j, pid, token);

    // zamykamy deskryptory aby nie bylo wyciekow
    MaybeClose(&input);
    MaybeClose(&output);

    // ustawiamy procedure na pierwszy plan i monitorujemy
    // (jezeli ma byc na pierwszym planie,
    // wpp dajemy informacje ze procedura
    // jest uruchomiona (i dziala w tle))
    if (!bg) {
      setfgpgrp(pid);
      exitcode = monitorjob(&mask);
    } else {
      msg("[%d] running '%s'\n", j, jobcmd(j));
    }
  } else { // child
    // ustawiamy pid i grupe pierwszoplanowa
    // (tez w dziecku aby nie bylo race condition)
    setpgid(pid, pid);
    if (!bg) {
      setfgpgrp(getpid());
    }
    // ustawiamy domyslna obsluge sygnalow SIGTSTP, SIGTTIN, SIGTTOU
    Signal(SIGTSTP, SIG_DFL);
    Signal(SIGTTIN, SIG_DFL);
    Signal(SIGTTOU, SIG_DFL);

    // ustawiamy stdin/stdout na nowo utworzone w funkcji
    // do_redir deskryptory plikow (jezeli jakies byly)
    // funkcja dup2 duplikuje deskryptory input/output i ustawia "stare"
    // deskryptory na te zduplikowane,
    // wiec musimy zadbac o zamykanie tych duplikatow)
    dup2((input != -1) ? input : 0, 0);
    MaybeClose(&input);

    dup2((output != -1) ? output : 1, 1);
    MaybeClose(&output);

    // odblokowujemy sygnaly sigchld i wykonujemy polecenie
    Sigprocmask(SIG_SETMASK, &mask, NULL);

    external_command(token);
  }
#endif /* !STUDENT */

  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return exitcode;
}

/* Start internal or external command in a subprocess that belongs to pipeline.
 * All subprocesses in pipeline must belong to the same process group. */
static pid_t do_stage(pid_t pgid, sigset_t *mask, int input, int output,
                      token_t *token, int ntokens, bool bg) {
  ntokens = do_redir(token, ntokens, &input, &output);

  if (ntokens == 0)
    app_error("ERROR: Command line is not well formed!");

  /* TODO: Start a subprocess and make sure it's moved to a process group. */
  pid_t pid = Fork();
#ifdef STUDENT
  int exitcode = 0;
  // wiekszosc analogiczna do funkcji do_job, z tą roznica, ze
  // jezeli pgid jeszcze nie zostal ustawiony, to ustawiamy go
  // na pid pierwszego procesu nalezacego do pipeline'a
  if (pid) { // parent
    if (pgid == 0) {
      pgid = pid;
    }
    setpgid(pid, pgid);
    if (!bg) {
      setfgpgrp(pgid);
    }
  } else { // child
    if (pgid == 0) {
      pgid = getpid();
    }
    setpgid(pid, pgid);
    if (!bg) {
      setfgpgrp(pgid);
    }

    Signal(SIGTSTP, SIG_DFL);
    Signal(SIGTTIN, SIG_DFL);
    Signal(SIGTTOU, SIG_DFL);

    dup2((input != -1) ? input : 0, 0);
    MaybeClose(&input);

    dup2((output != -1) ? output : 1, 1);
    MaybeClose(&output);

    Sigprocmask(SIG_SETMASK, mask, NULL);

    if ((exitcode = builtin_command(token)) >= 0) {
      exit(exitcode);
    }
    external_command(token);
  }
#endif /* !STUDENT */

  return pid;
}

static void mkpipe(int *readp, int *writep) {
  int fds[2];
  Pipe(fds);
  fcntl(fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, FD_CLOEXEC);
  *readp = fds[0];
  *writep = fds[1];
}

/* Pipeline execution creates a multiprocess job. Both internal and external
 * commands are executed in subprocesses. */
static int do_pipeline(token_t *token, int ntokens, bool bg) {
  pid_t pid, pgid = 0;
  int job = -1;
  int exitcode = 0;

  int input = -1, output = -1, next_input = -1;

  mkpipe(&next_input, &output);

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Start pipeline subprocesses, create a job and monitor it.
   * Remember to close unused pipe ends! */
#ifdef STUDENT
  (void)input;
  (void)job;
  (void)pid;
  (void)pgid;
  (void)do_stage;

  // j oznacza indeks nastepujacy po ostatnim znalezionym
  // T_PIPE'ie, lub 0 jezeli zadnego nie bylo
  int j = 0;
  // iterujemy po tokenach az do natrafienia na T_PIPE
  for (int i = 0; i < ntokens; i++) {
    if (token[i] == T_PIPE) {
      // jezeli jest to pierwsza iteracja to nie wywolujemy
      // mkpipe'a (zostal on wczesniej wywolany)
      if (pgid != 0) {
        mkpipe(&next_input, &output);
      }
      // funkcja do_stage forkuje nam proces i zwraca pid tego procesu
      // do argumentow dajemy pgid, input i output, token
      // (po ostatnim T_PIPE'ie),
      // dlugosc interesujacego nas ciagu tokenow-
      // od ostatniego pipe'a/ od 0 do obecnego pipe'a (wylacznie)
      pid = do_stage(pgid, &mask, input, output, token + j, i - j, bg);
      token[i] = T_NULL;
      if (pgid == 0) {
        // ustawiamy pgid na pid pierwszego z procesow z pipeline'a,
        // jezeli jeszcze nie byl ustawiony
        pgid = pid;
        // tworzymy zadanie z ustawionym pgidem
        job = addjob(pgid, bg);
      }
      MaybeClose(&input);
      MaybeClose(&output);
      // ustawiamy input na next_input zwrocony przez mkpipe'a
      input = next_input;

      // dodajemy nowa procedure do zadania
      addproc(job, pid, token + j);
      // ustawiamy j na indeks po pipe'ie
      j = i + 1;
    }
  }
  // tworzymy proces z tokenow, wystepujacych po ostatnim pipe'ie
  pid = do_stage(pgid, &mask, input, output, token + j, ntokens - j, bg);
  MaybeClose(&input);
  MaybeClose(&output);
  addproc(job, pid, token + j);

  // monitorujemy jezeli zadanie jest na pierwszym planie
  if (!bg) {
    exitcode = monitorjob(&mask);
  } else {
    msg("[%d] running '%s'\n", job, jobcmd(job));
  }
#endif /* !STUDENT */

  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return exitcode;
}

static bool is_pipeline(token_t *token, int ntokens) {
  for (int i = 0; i < ntokens; i++)
    if (token[i] == T_PIPE)
      return true;
  return false;
}

static void eval(char *cmdline) {
  bool bg = false;
  int ntokens;
  token_t *token = tokenize(cmdline, &ntokens);

  if (ntokens > 0 && token[ntokens - 1] == T_BGJOB) {
    token[--ntokens] = NULL;
    bg = true;
  }

  if (ntokens > 0) {
    if (is_pipeline(token, ntokens)) {
      do_pipeline(token, ntokens, bg);
    } else {
      do_job(token, ntokens, bg);
    }
  }

  free(token);
}

#ifndef READLINE
static char *readline(const char *prompt) {
  static char line[MAXLINE]; /* `readline` is clearly not reentrant! */

  write(STDOUT_FILENO, prompt, strlen(prompt));

  line[0] = '\0';

  ssize_t nread = read(STDIN_FILENO, line, MAXLINE);
  if (nread < 0) {
    if (errno != EINTR)
      unix_error("Read error");
    msg("\n");
  } else if (nread == 0) {
    return NULL; /* EOF */
  } else {
    if (line[nread - 1] == '\n')
      line[nread - 1] = '\0';
  }

  return strdup(line);
}
#endif

int main(int argc, char *argv[]) {
  /* `stdin` should be attached to terminal running in canonical mode */
  if (!isatty(STDIN_FILENO))
    app_error("ERROR: Shell can run only in interactive mode!");

#ifdef READLINE
  rl_initialize();
#endif

  sigemptyset(&sigchld_mask);
  sigaddset(&sigchld_mask, SIGCHLD);

  if (getsid(0) != getpgid(0))
    Setpgid(0, 0);

  initjobs();

  struct sigaction act = {
    .sa_handler = sigint_handler,
    .sa_flags = 0, /* without SA_RESTART read() will return EINTR */
  };
  Sigaction(SIGINT, &act, NULL);

  Signal(SIGTSTP, SIG_IGN);
  Signal(SIGTTIN, SIG_IGN);
  Signal(SIGTTOU, SIG_IGN);

  while (true) {
    char *line = readline("# ");

    if (line == NULL)
      break;

    if (strlen(line)) {
#ifdef READLINE
      add_history(line);
#endif
      eval(line);
    }
    free(line);
    watchjobs(FINISHED);
  }

  msg("\n");
  shutdownjobs();

  return 0;
}
