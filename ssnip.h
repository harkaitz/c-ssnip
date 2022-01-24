#ifndef SSNIP_H
#define SSNIP_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <regex.h>

#define SSNIP_TEMPLATE_LIMIT 256

typedef struct ssnip      ssnip;
typedef struct ssnip_tmpl ssnip_tmpl;

struct ssnip_tmpl {
    char    *filename_m;
    char    *name_s;
    bool     failed;
    bool     was_read;

    char    *interpreter_m;
    char    *open_regex_m;
    regex_t  open_regex;
    char    *quit_regex_m;
    regex_t  quit_regex;
    char    *inside_regex_m;
    regex_t  inside_regex;
    char    *filename_regex_m;
    regex_t  filename_regex;
    
    long     shift;
};

struct ssnip {
    bool       quiet;
    bool       interactive;
    size_t     tsz;
    ssnip_tmpl t[SSNIP_TEMPLATE_LIMIT];
};

bool ssnip_create          (ssnip **_s, const char *_t_dir);
void ssnip_destroy         (ssnip  *_s);
int  ssnip_print_templates (ssnip  *_s, FILE *_o_fp);
bool ssnip_tmpl_load       (ssnip_tmpl *_s);
bool ssnip_tmpl_write      (ssnip_tmpl *_s, const char *_opt_old_value,  const char *_opt_i_filename, int _fd1);
void ssnip_search_tmpl     (ssnip  *_s, const char *_name, ssnip_tmpl **_tmpl);
bool ssnip_process_fp      (ssnip  *_s, const char _i_filename[], FILE *_i_fp, int _o_fd, int *_o_count, bool *_o_valid);
bool ssnip_cat             (ssnip  *_s, const char _i_filename[], int _o_fd, int *_o_count, bool *_o_valid);
bool ssnip_replace         (ssnip *_s, const char _io_filename[]);
#endif
