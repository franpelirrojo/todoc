#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "bttrtime.h"

#define TODOC_VERSION 1
#define TODOC_DEFAULT_SIZE 10

//#define DEBUG
#ifdef DEBUG
#define TODOC_LOCAL_PATH "./"
#else
#define TODOC_LOCAL_PATH ".local/share/todoc"
#endif /* ifdef DEBUG */

#define TODOC_SAVE_FILE "todoc.bin"
#define TODOC_LIST_SEPARATOR "»"

struct task_file_header_t {
  unsigned short version;
  unsigned short count;
};

struct task_t {
  char *title;
  char *content;
  time_t date;
};

#define TODOC_NO_DATE 0

static int fd;
static unsigned short task_count = 0;
static unsigned short task_list_size = TODOC_DEFAULT_SIZE;
static struct task_t *task_list = NULL;

void print_task(struct task_t task, int counter) {
  char time[80];
  if (task.content != NULL) {
    printf("%d. %s %s %s", counter + 1, task.title, TODOC_LIST_SEPARATOR,
           task.content);
  } else {
    printf("%d. %s", counter + 1, task.title);
  }

  if (task.date != TODOC_NO_DATE) {
    strftime(time, sizeof(time), "%F", localtime(&task.date));
    printf(" %s\n", time);
  } else {
    printf("\n");
  }
}

void print_task_list(struct task_t *task_list) {
  for (int i = 0; i < task_count; i++) {
    print_task(task_list[i], i);
  }
}

void print_tasks_deadline(struct task_t *task_list, time_t deadline) {
  if (deadline == TODOC_NO_DATE) {
    if (time(&deadline) == -1) {
      perror("time");
      return;
    }
  }

  for (int i = 0; i < task_count; i++) {
    if (datecmp(task_list[i].date, deadline) == 0 ||
        datecmp(task_list[i].date, deadline) == 1) {
      print_task(task_list[i], i);
    }
  }
}

void delete_task(int index) {
  if (task_count == 0) return;

  free(task_list[index].title);
  task_list[index].title = NULL;
  free(task_list[index].content);
  task_list[index].content = NULL;
  task_list[index].date = TODOC_NO_DATE;

  for (int i = index; i < task_count; i++) {
    task_list[i].title = task_list[i + 1].title;
    task_list[i].content = task_list[i + 1].content;
    task_list[i].date = task_list[i + 1].date;
  }

  task_count--;
}

void clear_task_list() {
  if (task_count == 0) return;

  for (int i = 0; i < task_count; i++) {
    free(task_list[i].title);
    task_list[i].title = NULL;
    free(task_list[i].content);
    task_list[i].content = NULL;
    task_list[i].date = TODOC_NO_DATE;
  }

  task_count = 0;
}

bool create_task(char *title, char *content, time_t date) {
  if (task_count >= task_list_size) {
    struct task_t *temp =
        realloc(task_list,
                sizeof(struct task_t) * (task_list_size + TODOC_DEFAULT_SIZE));
    if (temp == NULL) {
      perror("realloc");
      return false;
    }

    task_list = temp;
  }

  task_list[task_count].title = strdup(title);
  if (task_list[task_count].title == NULL) {
    perror("strdup");
    printf("No se pudo guardar el título de la tarea.\n");
    return false;
  }

  if (content != NULL) {
    task_list[task_count].content = strdup(content);
    if (task_list[task_count].content == NULL) {
      perror("strdup");
      printf("No se puedo guardar el contenido de la tarea.\n");
      free(task_list[task_count].title);
      task_list[task_count].title = NULL;
      return false;
    }
  } else {
    task_list[task_count].content = NULL;
  }

  task_list[task_count].date = date;

  task_count++;
  return true;
}

int init_data() {
  char dirpath[255];
  char filepath[255];
  char *home = getenv("HOME");
  if (home != NULL) {
    int n;
    n = snprintf(dirpath, sizeof(dirpath), "%s/%s", home, TODOC_LOCAL_PATH);
    if (n < 0 || n > sizeof(dirpath)) {
      perror("Se truncó el path al directorio de guardado.");
      return -1;
    }
    n = snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, TODOC_SAVE_FILE);
    if (n < 0 || n > sizeof(filepath)) {
      perror("Se truncó el path al archivo de guardado.");
      return -1;
    }
  } else {
    perror("No se encontró la variable de entorno HOME.");
    return -1;
  }

  fd = open(filepath, O_RDWR);
  if (fd == -1) {
    if (errno == ENOENT) {
      if (mkdir(dirpath, 0700) == -1) {
        if (errno != EEXIST) {
          perror(
              "No se puedoc crear el directorio local para guardar los datos.");
          return -1;
        }
      }

      fd = open(filepath, O_CREAT | O_RDWR, 0644);
      if (fd == -1) {
        perror("Error al crear el archivo de datos.");
        return -1;
      }

      return 0;
    } else {
      perror("open");
      return -1;
    }
  }

  struct task_file_header_t header;
  int bread = read(fd, &header, sizeof(struct task_file_header_t));
  if (bread == -1) {
    perror("read");
    close(fd);
    return -1;
  }

  if (header.version != TODOC_VERSION) {
    fprintf(stderr,
            "No es la versión correcta.\nVersón del programa %d. Versión "
            "del fichero %d\n",
            TODOC_VERSION, header.version);
    return -1;
  }

  if (header.count > 0) {
    for (int i = 0; i < header.count; i++) {
      unsigned long size_title;
      unsigned long size_content;
      char *title = NULL;
      char *content = NULL;
      time_t date;

      int bread = read(fd, &size_title, sizeof(unsigned long));
      if (bread == -1) {
        perror("read");
        return -1;
      }

      title = malloc(size_title + 1);
      if (title == NULL) {
        perror("malloc");
        printf("Error al asignar memoria para el título");
        close(fd);
        return -1;
      }

      bread = read(fd, title, size_title);
      if (bread != size_title) {
        perror("read");
        printf("Error al leer el título. Bytes leidos = %d tamaño del título "
               "%ld\n",
               bread, size_title);
        close(fd);
        return -1;
      }

      title[size_title] = '\0';

      bread = read(fd, &size_content, sizeof(unsigned long));
      if (bread == -1) {
        perror("read");
        return -1;
      }

      if (size_content != 0) {
        content = malloc(size_content + 1);
        if (content == NULL) {
          perror("Error al asignar memoria para el título");
          close(fd);
          return -1;
        }

        bread = read(fd, content, size_content);
        if (bread != size_content) {
          perror("Error al leer el contenido.");
          close(fd);
          return -1;
        }

        content[size_content] = '\0';
      }

      bread = read(fd, &date, sizeof(time_t));
      if (bread != sizeof(time_t)) {
        perror("Error al leer la fecha.");
        close(fd);
        return -1;
      }

      if (!create_task(title, content, date)) {
        printf("No se pudo cargar la tarea %s | %s", title, content);
      }

      free(title);
      free(content);
    }
  }

  return 0;
}

int save_data() { // puedo guardar toda la estructura?
  if (ftruncate(fd, 0) == -1) {
    perror("ftruncate");
    close(fd);
    return -1;
  }

  if (lseek(fd, 0, SEEK_SET) == -1) {
    perror("lseek");
    close(fd);
    return -1;
  }

  struct task_file_header_t header = {TODOC_VERSION, task_count};
  if (write(fd, &header, sizeof(struct task_file_header_t)) == -1) {
    perror("write");
    close(fd);
    return -1;
  }

  for (int i = 0; i < task_count; i++) {
    unsigned long size_title =
        task_list[i].title != NULL ? strlen(task_list[i].title) : 0;
    unsigned long size_content =
        task_list[i].content != NULL ? strlen(task_list[i].content) : 0;

    if (write(fd, &size_title, sizeof(unsigned long)) == -1) {
      perror("write");
      close(fd);
      return -1;
    }

    if (write(fd, task_list[i].title, size_title) == -1) {
      perror("write");
      close(fd);
      return -1;
    }

    if (write(fd, &size_content, sizeof(unsigned long)) == -1) {
      perror("write");
      close(fd);
      return -1;
    }

    if (task_list[i].content != 0) {
      if (write(fd, task_list[i].content, size_content) == -1) {
        perror("write");
        close(fd);
        return -1;
      }
    }

    if (write(fd, &task_list[i].date, sizeof(time_t)) == -1) {
      perror("write");
      close(fd);
      return -1;
    }
  }

  close(fd);
  return 0;
}

void free_all() {
  for (int i = 0; i < task_count; i++) {
    free(task_list[i].title);
    free(task_list[i].content);
  }
  free(task_list);
}

time_t parsedate(char *date) {
  time_t new_date;

  if (date == NULL) {
    new_date = time(NULL);
    struct tm *broke_date = localtime(&new_date);
    broke_date->tm_mday++;
    new_date = mktime(broke_date);
    return new_date;
  }

  int counter = 0;
  char digit[4] = "";
  int digitcounter = 0;
  int fecha[] = {TODOC_NO_DATE, TODOC_NO_DATE, TODOC_NO_DATE};
  int fechacounter = 0;
  bool end = false;

  while (!end) {
    switch (date[counter]) {
    case '\0':
      fecha[fechacounter] = atoi(digit);
      end = true;
      break;
    case '-':
      fecha[fechacounter] = atoi(digit);
      digitcounter = 0;
      counter++;
      fechacounter++;
      break;
    default:
      if (date[counter] >= '0' && date[counter] <= '9') {
        digit[digitcounter] = date[counter];
      }

      digitcounter++;
      counter++;
    }
  }

  new_date = time(NULL);
  struct tm *broke_date = localtime(&new_date);
  if (fecha[0] != TODOC_NO_DATE) {
    broke_date->tm_mday = fecha[0];
  }
  if (fecha[1] != TODOC_NO_DATE) {
    broke_date->tm_mon = fecha[1] - 1;
  }
  if (fecha[2] != TODOC_NO_DATE) {
    broke_date->tm_year = fecha[2] - 1900;
  }
  new_date = mktime(broke_date);

  return new_date;
}

int main(int argc, char *argv[]) {
  task_list = malloc(sizeof(struct task_t) * task_list_size);
  if (init_data() == -1) {
    free(task_list);
    return -1;
  }

  if (argc == 1) {
    print_tasks_deadline(task_list, TODOC_NO_DATE);
  }

  int c;
  time_t taskdate = TODOC_NO_DATE;
  bool end = false;

  while ((c = getopt(argc, argv, ":d::r:l")) != -1 && !end) {
    switch (c) {
    case 'l':
      print_task_list(task_list);
      end = true;
      break;
    case 'r':
      if (strcmp(optarg, "a") == 0) {
        printf("borrar todo por argumento: %s\n", optarg);
        clear_task_list();
      } else {
        int integer = atoi(optarg);
        printf("borrar la tarea: %d\n", integer);
        delete_task(integer - 1);
      }
      end = true;
      break;
    case 'd':
      if (optarg != 0) {
        taskdate = parsedate(optarg);
      } else {
        taskdate = parsedate(NULL);
      }
      end = true;
      break;
    case '?':
      printf("Error: Opción desconocida: -%c.\n", optopt);
      break;
    case ':':
      printf("Error: Falta opción: -%c.\n", optopt);
      break;
    }
  }

  if ((argc - optind) >= 2) {
    create_task(argv[optind], argv[optind+1], taskdate);
  } else if (argc - optind > 0){
    create_task(argv[optind], NULL, taskdate);
  }

  if (save_data() == -1) {
    printf("No se ha podido guardar el estado correctamente.\n");
  }

  free_all();
  return 0;
}
