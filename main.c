#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION 1
#define DEFAULT_SIZE 10
#define DATA_PATH "todoc.data"

struct task_file_header_t {
  unsigned short version;
  unsigned short count;
};

struct task_t {
  char *title;
  char *content;
};

static int fd;
static unsigned short task_count = 0;
static unsigned short task_list_size = DEFAULT_SIZE;
static struct task_t *task_list = NULL;

void print_tasks(struct task_t *task_list) {
  for (int i = 0; i < task_count; i++) {
    if (task_list[i].content != NULL) {
      printf("%d. %s | %s\n", i + 1, task_list[i].title, task_list[i].content);
    } else {
      printf("%d. %s \n", i + 1, task_list[i].title);
    }
  }
}

void delete_task(int index) { // TODO: liberar esto bien
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

bool add_task(char *title, char *content) {
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

  task_count++;
  return true;
}

int init_data() {
  fd = open(DATA_PATH, O_RDWR);
  if (fd == -1) {
    if (errno == ENOENT) {
      fd = open(DATA_PATH, O_CREAT | O_RDWR, 0644);
      if (fd == -1) {
        perror("Error al crear el archivo de datos");
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
          perror("Error al leer el título");
          close(fd);
          return -1;
        }

        content[size_content] = '\0';
      }

      if (!add_task(title, content)) {
        printf("No se pudo cargar la tarea %s | %s", title, content);
      }

      free(title);
      free(content);
    }
  }

  return 0;
}

int save_data() {
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

int main(int argc, char *argv[]) {
  task_list = malloc(sizeof(struct task_t) * task_list_size);

  if (init_data() == -1) {
    free(task_list);
    return -1;
  }

  if (argc < 2) {
    print_tasks(task_list);
  } else if (strcmp(argv[1], "-t") == 0) {
    if (argc < 3) {
      printf(
          "-t requiere un nombre para la tarea y una descripción opcional.\n");
    } else if (argc < 4) {
      add_task(argv[2], NULL);
    } else {
      add_task(argv[2], argv[3]);
    }
  } else if (strcmp(argv[1], "-d") == 0) {
    if (argc < 3) {
      printf("-d requiere un identificador.\n");
    } else {
      int task_id = (int)strtol(argv[2], NULL, 10);
      delete_task(task_id - 1);
    }
  }

  if (save_data() == -1) {
    printf("No se ha podido guardar el estado correctamente.\n");
  }

  free_all();

  return 0;
}
