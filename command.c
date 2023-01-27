#include "shell.h"

typedef int (*func_t)(char **argv);

typedef struct {
  const char *name;
  func_t func;
} command_t;

static int do_quit(char **argv) {
  shutdownjobs();
  exit(EXIT_SUCCESS);
}

/*
 * Change current working directory.
 * 'cd' - change to $HOME
 * 'cd path' - change to provided path
 */
static int do_chdir(char **argv) {
  char *path = argv[0];
  if (path == NULL)
    path = getenv("HOME");
  int rc = chdir(path);
  if (rc < 0) {
    msg("cd: %s: %s\n", strerror(errno), path);
    return 1;
  }
  return 0;
}

/*
 * Displays all stopped or running jobs.
 */
static int do_jobs(char **argv) {
  watchjobs(ALL);
  return 0;
}

/*
 * Move running or stopped background job to foreground.
 * 'fg' choose highest numbered job
 * 'fg n' choose job number n
 */
static int do_fg(char **argv) {
  int j = argv[0] ? atoi(argv[0]) : -1;

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);
  if (!resumejob(j, FG, &mask))
    msg("fg: job not found: %s\n", argv[0]);
  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return 0;
}

/*
 * Make stopped background job running.
 * 'bg' choose highest numbered job
 * 'bg n' choose job number n
 */
static int do_bg(char **argv) {
  int j = argv[0] ? atoi(argv[0]) : -1;

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);
  if (!resumejob(j, BG, &mask))
    msg("bg: job not found: %s\n", argv[0]);
  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return 0;
}

/*
 * Make stopped background job running.
 * 'bg' choose highest numbered job
 * 'bg n' choose job number n
 */
static int do_kill(char **argv) {
  if (!argv[0])
    return -1;
  if (*argv[0] != '%')
    return -1;

  int j = atoi(argv[0] + 1);

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);
  if (!killjob(j))
    msg("kill: job not found: %s\n", argv[0]);
  Sigprocmask(SIG_SETMASK, &mask, NULL);

  return 0;
}

static command_t builtins[] = {
  {"quit", do_quit}, {"cd", do_chdir},  {"jobs", do_jobs}, {"fg", do_fg},
  {"bg", do_bg},     {"kill", do_kill}, {NULL, NULL},
};

int builtin_command(char **argv) {
  for (command_t *cmd = builtins; cmd->name; cmd++) {
    if (strcmp(argv[0], cmd->name))
      continue;
    return cmd->func(&argv[1]);
  }

  errno = ENOENT;
  return -1;
}

noreturn void external_command(char **argv) {
  const char *path = getenv("PATH");

  if (!index(argv[0], '/') && path) {
    /* TODO: For all paths in PATH construct an absolute path and execve it. */
#ifdef STUDENT
    int size_of_current_path;
    char *current_path;
    // czytamy zmienna srodowiskowa PATH, az do natrafienia na znak konca linii
    // (lub do znalezienia katalogu dla polecenia, ktore chcemy wykonac)
    while (strcmp(path, "\0")) {

      // strcspn zwraca nam indeks pierwszego ":" w path
      // (wiec zwraca nam dlugosc pierwszej sciezki w czytanym stringu)
      size_of_current_path = strcspn(path, ":");

      // zapisujemy do zmiennej sciezke ze zmiennej PATH
      // (az do znaku ":" rozdzielajacego kolejne sciezki)
      current_path = strndup(path, size_of_current_path);

      // przesuwamy wskaznik na kolejna sciezke
      path += size_of_current_path + 1;

      // dodajemy do sciezki "/{polecenie wpisane przez uzytkownika}"
      strapp(&current_path, "/");
      strapp(&current_path, argv[0]);

      // probujemy wykonac polecenie
      // (jezeli jest w znalezionym katalogu ze zmiennej PATH,
      // to wykona sie, jezeli nie, to zwroci -1,
      // a my bedziemy szukac dalej
      execve(current_path, argv, environ);

      // polecenie strndup allocuje pamiec,
      // wiec musimy zadbac o uwolnienie tej zajmowanej pamieci
      free(current_path);
    }
#endif /* !STUDENT */
  } else {
    (void)execve(argv[0], argv, environ);
  }

  msg("%s: %s\n", argv[0], strerror(errno));
  exit(EXIT_FAILURE);
}
