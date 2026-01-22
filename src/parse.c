#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "parse.h"

int create_db_header(/*int fd, */struct dbheader_t **headerOut) {
  
  // if (fd < 0) {
  //   printf("Invalid file descriptor\n");
  //   return STATUS_ERROR;
  // };

  struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));

  if (header == (void *)-1) {
    printf("Malloc failed to create db header\n");
    return STATUS_ERROR;
  };
  header->version = 0x1;
  header->count = 0;
  header->magic = HEADER_MAGIC;
  header->filesize = sizeof(struct dbheader_t);

  *headerOut = header;

  return STATUS_SUCCESS;
};

int validate_db_header(int fd, struct dbheader_t **headerOut) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
  if (header == (void *)-1) {
    printf("Malloc failed create a db header\n");
    return STATUS_ERROR;
  }

  if (read(fd, header, sizeof(struct dbheader_t)) !=
      sizeof(struct dbheader_t)) {
    perror("read");
    free(header);
    printf("Failed to read full header from file\n");
    return STATUS_ERROR;
  }

  header->version = ntohs(header->version);
  header->count = ntohs(header->count);
  header->magic = ntohl(header->magic);
  header->filesize = ntohl(header->filesize);

  if (header->magic != HEADER_MAGIC) {
    printf("Impromper header magic\n");
    free(header);
    return -1;
  }

  if (header->version != 1) {
    printf("Improper header version\n");
    free(header);
    return -1;
  }

  struct stat dbstat = {0};
  fstat(fd, &dbstat);
  if (header->filesize != dbstat.st_size) {
    printf("Corrupted database\n");
    free(header);
    return -1;
  }

  *headerOut = header;
  return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr,
                   struct employee_t **employeeOut) {
  if (fd < 0) {
    printf("Invalid / Missing File Descriptor\n");
    return STATUS_ERROR;
  }

  int count = dbhdr->count;

  struct employee_t *employees = calloc(count, sizeof(struct employee_t));
  if (employees == NULL) {
    printf("Malloc failed to create employee array\n");
    return STATUS_ERROR;
  };

  read(fd, employees, count * sizeof(struct employee_t));

  int i = 0;
  for (; i < count; i++) {
    employees[i].hours = ntohl(employees[i].hours);
  }

  *employeeOut = employees;

  return STATUS_SUCCESS;
};

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees,
                 char *addstring) {

  if (dbhdr == NULL || employees == NULL || *employees == NULL ||
      addstring == NULL) {
    printf("Invalid parameters to add_employee\n");
    return STATUS_ERROR;
  }

  printf("Adding employee...\n");

  struct employee_t *e = *employees;
  unsigned int new_count = dbhdr->count + 1;
  struct employee_t *tmp = realloc(e, sizeof(struct employee_t) * new_count);
  if (tmp == NULL) {
    printf("Realloc failed to expand employee array\n");
    return STATUS_ERROR;
  }
  e = tmp;
  dbhdr->count = new_count;

  char *name = strtok(addstring, ",");
  char *address = strtok(NULL, ",");
  char *hours_str = strtok(NULL, ",");

  if (name == NULL || address == NULL || hours_str == NULL) {
    printf("Invalid addstring format. Expected format: name,address,hours\n");
    free(e);
    return STATUS_ERROR;
  }

  printf("Name: %s\n", name);
  printf("Address: %s\n", address);
  printf("Hours: %s\n", hours_str);

  strncpy(e[dbhdr->count - 1].name, name, sizeof(e[dbhdr->count - 1].name) - 1);
  strncpy(e[dbhdr->count - 1].address, address,
          sizeof(e[dbhdr->count - 1].address) - 1);

  e[dbhdr->count - 1].hours = atoi(hours_str);

  *employees = e;

  return STATUS_SUCCESS;
}

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {
  if (dbhdr == NULL || employees == NULL) {
    printf("Invalid parameters to list_employees\n");
    return;
  }
  int i = 0;
  printf("Employee List:\n");
  for (; i < dbhdr->count; i++) {
    printf("Employee %d:\n", i + 1);
    printf("\tName: %s\n", employees[i].name);
    printf("\tAddress: %s\n", employees[i].address);
    printf("\tHours: %d\n", employees[i].hours);
  }
}

int output_file(int fd, struct dbheader_t *dbhdr,
                struct employee_t *employees) {

  if (fd < 0) {
    printf("Invalid / Missing File Descriptor\n");
    return STATUS_ERROR;
  }

  int realcount = dbhdr->count;

  dbhdr->magic = htonl(dbhdr->magic);
  dbhdr->filesize = htonl(sizeof(struct dbheader_t) +
                          (sizeof(struct employee_t) * realcount));
  dbhdr->count = htons(dbhdr->count);
  dbhdr->version = htons(dbhdr->version);

  lseek(fd, 0, SEEK_SET);

  if (write(fd, dbhdr, sizeof(struct dbheader_t)) !=
      sizeof(struct dbheader_t)) {
    printf("Failed to write header to file\n");
    return STATUS_ERROR;
  } /* else {
    printf("Wrote header to file\n");
  } */

  int i = 0;
  for (; i < realcount; i++) {

    employees[i].hours = htonl(employees[i].hours);
    write(fd, &employees[i], sizeof(struct employee_t));
  }

  return STATUS_SUCCESS;
}
