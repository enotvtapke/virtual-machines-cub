//
// Created by enotvtapke on 10/25/25.
//

#include <climits>
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "analizer.h"

/* Gets a string from a string table by an index */
const char * get_string(const bytefile * f, const unsigned int pos) {
  if (pos >= f->stringtab_size) {
    failure("*** FAILURE: invalid string index");
  }
  return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
const char *get_public_name(const bytefile *f, const unsigned  int i) {
  if (i >= f->public_symbols_number) {
    failure("*** FAILURE: invalid public symbol index");
  }
  return get_string(f, f->public_ptr[i * 2]);
}

/* Gets an offset for a public symbol */
int get_public_offset(const bytefile *f, const unsigned int i) {
  if (i >= f->public_symbols_number) {
    failure("*** FAILURE: invalid public symbol index");
  }
  return f->public_ptr[i * 2 + 1];
}

/* Reads a binary bytecode file by name and unpacks it */
const bytefile *read_file(const char * fname) {
  FILE *f = fopen(fname, "rb");
  long size;

  if (f == 0) {
    failure("%s\n", strerror(errno));
  }

  if (fseek(f, 0, SEEK_END) == -1) {
    failure("%s\n", strerror(errno));
  }

  struct stat st;
  stat(fname, &st);
  if (st.st_size > LONG_MAX)
    failure("Bytecode file too large: %lld", st.st_size);

  bytefile *file = (bytefile *) malloc(sizeof(void *) * 5 + sizeof(long) + sizeof(int) + (size = ftell(f)));

  if (file == 0) {
    failure("*** FAILURE: unable to allocate memory.\n");
  }

  rewind(f);

  if (size != fread(&file->stringtab_size, 1, size, f)) {
    failure("%s\n", strerror(errno));
  }

  fclose(f);

  if (file->stringtab_size < 0 ||
      file->global_area_size < 0 ||
      file->public_symbols_number < 0 ||
      (long) file->stringtab_size + file->public_symbols_number * 2 * sizeof(int32_t) > size
  ) {
    failure("*** FAILURE: invalid file format.\n");
  }

  file->string_ptr = &file->buffer[file->public_symbols_number * 2 * sizeof(int)];
  file->public_ptr = (int *) file->buffer;
  file->code_ptr = &file->string_ptr[file->stringtab_size];
  file->code_size = size - ((size_t) file->code_ptr - (size_t) &file->stringtab_size);

  *(file->code_ptr - 1) = '\0';
  file->entrypoint_offset = -1;
  for (int i = 0; i < file->public_symbols_number; i++) {
    if (strcmp(get_public_name(file, i), "main") == 0) {
      file->entrypoint_offset = get_public_offset(file, i);
      break;
    }
  }
  if (file->entrypoint_offset == -1) {
    failure("*** FAILURE: main function not found.\n");
  }
  if (file->entrypoint_offset >= file->code_size) {
    failure("*** FAILURE: Wrong main function offset.\n");
  }
  return file;
}

void disassemble(FILE *output, const bytefile *bytefile) {
  size_t current_offset = 0;
  while (current_offset < bytefile->code_size) {
    fprintf(output,"%s:\t", hex8(current_offset).c_str());
    auto instruction = decodeInstruction(bytefile, current_offset);
    fprintf(output,"%s\n", instruction.to_string(bytefile).c_str());
    current_offset += instruction.length();
  }
}

void dump_file(FILE *f, const bytefile *bf)
{
  fprintf(f, "String table size       : %d\n", bf->stringtab_size);
  fprintf(f, "Global area size        : %d\n", bf->global_area_size);
  fprintf(f, "Number of public symbols: %d\n", bf->public_symbols_number);
  fprintf(f, "Public symbols          :\n");

  for (int i = 0; i < bf->public_symbols_number; i++)
    fprintf(f, "   0x%.8x: %s\n", get_public_offset(bf, i), get_public_name(bf, i));

  fprintf(f, "Code:\n");
  disassemble(f, bf);
}
