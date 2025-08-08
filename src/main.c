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

#define VERSION 1
#define DEFAULT_SIZE 10
#define LOCAL_PATH ".local/share/todoc"
#define SAVE_FILE "todoc.bin"
#define LIST_SEPARATOR "»"

struct task_file_header_t {
  unsigned short version;
  unsigned short count;
};

struct task_t {
  char *title;
  char *content;
  time_t date;
};

static int fd;
static time_t today;
static unsigned short task_count = 0;
static unsigned short task_list_size = DEFAULT_SIZE;
static struct task_t *task_list = NULL;

void print_task(struct task_t task, int counter) {
  char time[80];
  if (task.content != NULL) {
    printf("%d. %s %s %s", counter + 1, task.title, LIST_SEPARATOR,
           task.content);
  } else {
    printf("%d. %s", counter + 1, task.title);
  }

  if (task.date != -1) {
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
  for (int i = 0; i < task_count; i++) {
    if (task_list[i].date > deadline || task_list[i].date == -1) {
      print_task(task_list[i], i);
    }
  }
}

void delete_task(int index) {
  if (task_count == 0) {
    return;
  }

  free(task_list[index].title);
  task_list[index].title = NULL;
  free(task_list[index].content);
  task_list[index].content = NULL;

  for (int i = index; i < task_count; i++) {
    task_list[i].title = task_list[i + 1].title;
    task_list[i].content = task_list[i + 1].content;
  }

  task_count--;
}

bool create_task(char *title, char *content, time_t date) {
  if (task_count >= task_list_size) {
    struct task_t *temp = realloc(
        task_list, sizeof(struct task_t) * (task_list_size + DEFAULT_SIZE));
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
    n = snprintf(dirpath, sizeof(dirpath), "%s/%s", home, LOCAL_PATH);
    if (n < 0 || n > sizeof(dirpath)) {
      perror("Se truncó el path al directorio de guardado.");
      return -1;
    }
    n = snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, SAVE_FILE);
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

  if (header.version != VERSION) {
    fprintf(stderr,
            "No es la versión correcta.\nVersón del programa %d. Versión "
            "del fichero %d\n",
            VERSION, header.version);
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

  struct task_file_header_t header = {VERSION, task_count};
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
  int fecha[] = {-1, -1, -1};
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
  if (fecha[0] != -1) {
    broke_date->tm_mday = fecha[0];
  }
  if (fecha[1] != -1) {
    broke_date->tm_mon = fecha[1] - 1;
  }
  if (fecha[2] != -1) {
    broke_date->tm_year = fecha[2] - 1900;
  }
  new_date = mktime(broke_date);

  return new_date;
}

int main(int argc, char *argv[]) {
  time(&today);
  task_list = malloc(sizeof(struct task_t) * task_list_size);
  if (init_data() == -1) {
    free(task_list);
    return -1;
  }

  switch (argc) { // errores
  case 1:
    print_tasks_deadline(task_list, today);
    break;
  case 2:
    if (strcmp(argv[1], "-l") == 0) {
      print_task_list(task_list);
    } else {
      printf("todoc -l"
             "todoc -r <id>"
             "todoc -t <titulo> <comentario> -d <dd-mm-yyyy>");
    }
  case 3:
    if (strcmp(argv[1], "-t") == 0) {
      create_task(argv[2], NULL, -1);
    } else if (strcmp(argv[1], "-r") == 0) {
      int task_id = atoi(argv[2]);
      delete_task(task_id - 1);
    }
    break;
  case 4:
    if (strcmp(argv[3], "-d") == 0) {
      create_task(argv[2], NULL, parsedate(NULL));
    } else {
      create_task(argv[2], argv[3], -1); // TODO: Permitir varios
    }
    break;
  case 5:
    if (strcmp(argv[4], "-d") == 0) {
      create_task(argv[2], argv[3], parsedate(NULL));
    } else {
      create_task(argv[2], NULL, parsedate(argv[4]));
    }
    break;
  case 6:
    create_task(argv[2], argv[3], parsedate(argv[5]));
    break;
  default:
    printf("todoc -l"
           "todoc -r <id>"
           "todoc -t <titulo> <comentario> -d <dd-mm-yyyy>");
  }

  if (save_data() == -1) {
    printf("No se ha podido guardar el estado correctamente.\n");
  }

  free_all();
  return 0;
}
