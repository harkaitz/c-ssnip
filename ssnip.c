#define _POSIX_C_SOURCE 201000L
#include "ssnip.h"
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define FOREACH(TYPE,E,ARRAY) for(TYPE *E = ARRAY; E<((ARRAY)+(ARRAY ## sz)); E++)
#define VALID_FD(N) ((N)!=-1)

__attribute__((weak)) const char *g_env_prog = "/usr/bin/env";

#define verbose(FMT,...) ({})

bool ssnip_create(ssnip **_s, const char *_t_dir) {

    bool   retval        = false;
    ssnip *self          = NULL;
    int    find_pipe[2]  = {-1,-1};
    pid_t  find_pid      = -1;
    FILE  *find_fp       = NULL;
    size_t t_dirsz       = strlen(_t_dir);
    int    err           = 0;
    
    /* Allocate memory. */
    self = calloc(1, sizeof(struct ssnip));
    if (!self/*err*/) goto cleanup_errno;
    
    /* Fork the 'find' process. */
    err = pipe(find_pipe);
    if (err==-1/*err*/) goto cleanup_errno;
    find_pid = fork();
    if (find_pid==-1/*err*/) goto cleanup_errno;
    if (find_pid == 0) {
        dup2(find_pipe[1], 1);
        close(find_pipe[0]);
        close(find_pipe[1]);
        execl(g_env_prog, g_env_prog, "find", _t_dir, "-type", "f", NULL);
        syslog(LOG_ERR, "Can't execute find.");
        exit(1);
    }
    close(find_pipe[1]); find_pipe[1] = -1;

    /* Read from find in buffered mode. */
    find_fp = fdopen(find_pipe[0], "r");
    if (!find_fp/*err*/) goto cleanup_errno;
    find_pipe[0] = -1;

    /* Read the filenames. */
    while (self->tsz < SSNIP_TEMPLATE_LIMIT) {
        char *line = NULL; size_t linesz = 0;
        ssize_t rbytes = getline(&line, &linesz, find_fp);
        if (rbytes==-1) break;
        if (rbytes > 1 && line[rbytes-1]=='\n') line[rbytes-1]='\0';
        self->t[self->tsz].filename_m = line;
        self->t[self->tsz].name_s     = line+t_dirsz+1;
        self->tsz++;
    }
    
    /* Wait find. */
    fclose(find_fp); find_fp = NULL;
    waitpid(find_pid, NULL, 0); find_pid = -1;
    
    /* Return */
    *_s = self;
    retval = true;

    /* Cleanup. */
 cleanup:
    if (find_fp)          fclose(find_fp);
    if (find_pipe[0]!=-1) close(find_pipe[0]);
    if (find_pipe[1]!=-1) close(find_pipe[1]);
    if (find_pid!=-1)     waitpid(find_pid, NULL, 0);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    if (find_pid!=-1) {
        kill(find_pid, SIGINT);
    }
    if (self) {
        FOREACH(ssnip_tmpl,t,self->t) free(t->filename_m);
        free(self);
    }
    goto cleanup;
}
void ssnip_destroy(ssnip *_s) {
    if (_s) {
        FOREACH(ssnip_tmpl, t, _s->t) {
            free(t->filename_m);
            free(t->interpreter_m);
            free(t->open_regex_m);
            free(t->inside_regex_m);
            free(t->quit_regex_m);
            free(t->filename_regex_m);
        }
        free(_s);
    }
}
int  ssnip_print_templates(ssnip  *_s, FILE *_o_fp) {
    int r = 0;
    FOREACH(ssnip_tmpl,t,_s->t) {
        if (ssnip_tmpl_load(t)) {
            r += printf("[%s]\n", t->name_s);
            if (t->interpreter_m) {
                r += printf("- Interpreter: %s\n", t->interpreter_m);
            } else {
                r += printf("- shift: %li\n", t->shift);
            }
            if (t->open_regex_m) {
                r += printf("- open_regex: %s\n", t->open_regex_m);
            }
            if (t->inside_regex_m) {
                r += printf("- inside_regex: %s\n", t->inside_regex_m);
            }
            if (t->quit_regex_m) {
                r += printf("- quit_regex: %s\n", t->quit_regex_m);
            }
            if (t->filename_regex_m) {
                r += printf("- filename_regex: %s\n", t->filename_regex_m);
            }
        }
    }
    return r;
}
bool ssnip_tmpl_load(ssnip_tmpl *_t) {

    FILE  *fp           = NULL;
    char   b[1024]      = {0};
    int    ln           = 1;
    long   pos          = 0;
    char  *s1,*s2,*s3   = NULL;
    int    rerr         = 0;
    bool   retval       = false;

    /* If it was already read, skip loading. */
    if (_t->failed) {
        return false;
    } else if (_t->was_read) {
        return true;
    }

    /* Initialize as failed. */
    _t->failed = true;
    
    /* Open the template. */
    fp = fopen(_t->filename_m, "r");
    if (!fp) { goto cleanup_errno; }

    /* Read line by line, removing the trailing newline. */
    for (ln=1, pos = ftell(fp); fgets(b, sizeof(b), fp); ln++, pos = ftell(fp)) {
        if ((s1 = strchr(b, '\n'))) { *s1 = '\0'; }
        
        /* Executables. */
        if (ln==1 && b[0]=='#' && b[1]=='!') {
            _t->interpreter_m = strdup(&b[2]);
            if (!_t->interpreter_m/*err*/) { goto cleanup_errno; }
            continue;
        }

        /* Detect the final of the header. */
        if (b[0]=='#' && b[1]=='s' && b[2]==':') {
            s1 = b+3;
        } else if (b[0]=='/' && b[1]=='/' && b[2]=='s' && b[3]==':') {
            s1 = b+4;
        } else {
            break;
        }
        
        /* Get command and argument. */
        s1 = strtok_r(s1, ": \n", &s3);
        if (!s1) continue;
        s2 = strtok_r(NULL, "\n", &s3);
        if (!s2) continue;
        while (*s2 && strchr(" : \n", *s2)) s2++;

        /* Set variables. */
        if (!strcasecmp(s1, "open_regex")) {
            rerr = regcomp(&_t->open_regex, s2, REG_ICASE);
            if (rerr/*err*/) { goto cleanup_regex; }
            _t->open_regex_m = strdup(s2);
            if (!_t->open_regex_m/*err*/) { goto cleanup_errno; }
        } else if (!strcasecmp(s1, "quit_regex")) {
            rerr = regcomp(&_t->quit_regex, s2, REG_ICASE);
            if (rerr/*err*/) { goto cleanup_regex; }
            _t->quit_regex_m = strdup(s2);
            if (!_t->quit_regex_m/*err*/) { goto cleanup_errno; }
        } else if (!strcasecmp(s1, "inside_regex")) {
            rerr = regcomp(&_t->inside_regex, s2, REG_ICASE);
            if (rerr/*err*/) { goto cleanup_regex; }
            _t->inside_regex_m = strdup(s2);
            if (!_t->inside_regex_m/*err*/) { goto cleanup_errno; }
        } else if (!strcasecmp(s1, "filename_regex")) {
            rerr = regcomp(&_t->filename_regex, s2, REG_ICASE);
            if (rerr/*err*/) { goto cleanup_regex; }
            _t->filename_regex_m = strdup(s2);
            if (!_t->filename_regex_m/*err*/) { goto cleanup_errno; }
        } else {
            syslog(LOG_WARNING, "%s: Ignoting directive %s.", _t->filename_m, s1);
        }
    }

    _t->shift    = pos;
    _t->was_read = true;
    _t->failed   = 
        !_t->open_regex_m    &&
        !_t->quit_regex_m    &&
        !_t->inside_regex_m  &&
        !_t->filename_regex_m;

    retval = !_t->failed;
 cleanup:
    if (fp) fclose(fp);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", _t->filename_m, strerror(errno));
    goto cleanup;
 cleanup_regex:
    regerror(rerr, NULL, b, sizeof(b)-1);
    syslog(LOG_ERR, "%s: %s", _t->filename_m, b);
    goto cleanup;
}
bool ssnip_tmpl_write(ssnip_tmpl *_t, const char *_opt_old_value, const char *_opt_i_filename, int _fd1) {
    bool    retval       = false;
    pid_t   child_pid    = -1;
    int     status       = -1;
    char    buffer[1024] = {0};
    int     fd           = -1;
    int     err          = -1; 
    if (_t->interpreter_m) {
        child_pid = fork();
        if (child_pid==-1/*err*/) goto cleanup_errno;
        if (child_pid==0) {
            if (_fd1!=1) {
                dup2(_fd1, 1);
                close(_fd1);
            }
            char   *argv[10]  = {NULL};
            int     argc      = 0;
            setenv("SSNIP_OLD", (_opt_old_value)?_opt_old_value:"", true);
            setenv("SSNIP_FILE", (_opt_i_filename)?_opt_i_filename:"", true);
            argv[argc++] = strtok(_t->interpreter_m, " \t\r\n");
            argv[argc]   = strtok(NULL, "\n");
            if (argv[argc]) argc++;
            argv[argc++] = _t->filename_m;
            argv[argc] = NULL;
            execv(argv[0], argv);
            syslog(LOG_ERR, "%s", strerror(errno));
            exit(1);
        }
        err = waitpid(child_pid, &status, 0); child_pid = -1;
        if (err==-1/*err*/) goto cleanup_errno;
        if (!WIFEXITED(status)/*err*/) goto cleanup_interrupted;
        status = WEXITSTATUS(status);
        if (status!=0 && status!=2/*err*/) goto cleanup_program_failed;
        if (status==2 && _opt_old_value) {
            size_t w1 = strlen(_opt_old_value);
            ssize_t w2 = write(_fd1, _opt_old_value, w1);
            if (w2==-1/*err*/) goto cleanup_errno;
            if (w1!=w2/*err*/) goto cleanup_program_failed;
        }
    } else {
        fd = open(_t->filename_m, O_RDONLY);
        if (fd==-1/*err*/) goto cleanup_errno_template;
        err = lseek(fd, _t->shift, SEEK_SET);
        if (err==-1/*err*/) goto cleanup_errno_template;
        while (1) {
            ssize_t rbytes = read(fd, buffer, sizeof(buffer));
            if (rbytes==-1/*err*/) goto cleanup_errno_template;
            if (rbytes==0) break;
            ssize_t wbytes = write(1, buffer, rbytes);
            if (wbytes==-1/*err*/) goto cleanup_errno;
        }
        close(fd);
        fd = -1;
    }
    fsync(_fd1);
    retval = true;
    goto cleanup;
 cleanup:
    if (fd!=-1)        close(fd);
    if (child_pid!=-1) waitpid(child_pid, NULL, 0);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", (_opt_i_filename)?_opt_i_filename:"-", strerror(errno));
    goto cleanup;
 cleanup_interrupted:
    syslog(LOG_ERR, "The program was interrupted.");
    goto cleanup;
 cleanup_program_failed:
    syslog(LOG_ERR, "%s: %s", (_opt_i_filename)?_opt_i_filename:"-", "Template failed.");
    goto cleanup;
 cleanup_errno_template:
    syslog(LOG_ERR, "%s: %s", _t->filename_m, strerror(errno));
    goto cleanup;
}
bool ssnip_tmpl_match(ssnip_tmpl *_t, regex_t *regex, const char *line) {
    int  r       = 0;
    char b[1024] = {0};
    r = regexec(regex, line, 0, NULL, 0);
    if (r == 0) {
        return true;
    } else if (r == REG_NOMATCH) {
        return false;
    } else {
        regerror(r, regex, b, sizeof(b)-1);
        syslog(LOG_ERR, "%s: %s", _t->name_s, line);
        return false;
    }
}
bool ssnip_tmpl_match_open(ssnip_tmpl *_t, const char *line) {
    if (!ssnip_tmpl_load(_t) || !_t->open_regex_m) {
        return false;
    } else {
        return ssnip_tmpl_match(_t, &_t->open_regex, line);
    }
}
bool ssnip_tmpl_match_quit(ssnip_tmpl *_t, const char *line) {
    if (!ssnip_tmpl_load(_t) || !_t->quit_regex_m) {
        return false;
    } else {
        return ssnip_tmpl_match(_t, &_t->quit_regex, line);
    }
}
bool ssnip_tmpl_match_inside(ssnip_tmpl *_t, const char *line) {
    if (!ssnip_tmpl_load(_t) || !_t->inside_regex_m) {
        return true; /* Consider inside by default. */
    } else {
        return ssnip_tmpl_match(_t, &_t->inside_regex, line);
    }
}
bool ssnip_tmpl_match_filename(ssnip_tmpl *_t, const char *_filename) {
    if (!ssnip_tmpl_load(_t) || !_t->filename_regex_m) {
        return false;
    } else {
        return ssnip_tmpl_match(_t, &_t->filename_regex, _filename);
    }
}
void ssnip_search_tmpl(ssnip *_s, const char *_name, ssnip_tmpl **_tmpl) {
    ssnip_tmpl *found = NULL;
    FOREACH(ssnip_tmpl, t, _s->t) {
        if (strcasecmp(t->name_s, _name)) continue;
        if (!ssnip_tmpl_load(t)) continue;
        found = t;
        break;
    }
    if (_tmpl) *_tmpl = found;
}
bool ssnip_process_fp(ssnip *_s, const char _i_filename[], FILE *_i_fp, int _o_fd, int *_o_count, bool *_o_valid) {
    FILE       *buf_fp     = NULL;
    char       *buf_m      = NULL;
    size_t      buf_msz    = 0;
    ssnip_tmpl *ot         = NULL;
    bool        valid      = true;
    int         count      = 0;
    char        l[1024]    = {0};
    int         l_number   = 0;
    bool        l_is_close = false;
    bool        l_is_open  = false;
    int         err        = 0;
    bool        retval     = false;
    const char *i_filename = (_i_filename)?_i_filename:"-";

    /* Open buffer. */
    buf_fp = open_memstream(&buf_m, &buf_msz);
    if (!buf_fp/*err*/) goto cleanup_errno;
    
    /* Match filename. */
    if (_i_filename) {
        FOREACH(ssnip_tmpl,t,_s->t) {
            if (ssnip_tmpl_match_filename(t, _i_filename)) {
                ot = t;
                break;
            }
        }
    }
    if (ot && !_s->quiet) {
        verbose("%s: sof: %s: open", i_filename, ot->name_s);
    }
    
    /* Read lines. */
    while (fgets(l, sizeof(l)-1, _i_fp)) {
        
        l_is_close = false;
        l_is_open  = false;
        l_number++;

        /* Search open. */
        if (!ot) {
            FOREACH(ssnip_tmpl,t,_s->t) {
                if (ssnip_tmpl_match_open(t, l)) {
                    ot = t;
                    l_is_open = true;
                    break;
                }
            }
        }
        if (l_is_open && !_s->quiet) {
            verbose("%s: %3i: %s: open", i_filename, l_number, ot->name_s);
        }
        
        /* Search close. */
        if (ot && !l_is_open) {
            if (ssnip_tmpl_match_quit(ot, l)) {
                l_is_close = true;
            }
            if (!ssnip_tmpl_match_inside(ot, l)) {
                l_is_close = true;
            }
        }
        if (l_is_close && !_s->quiet) {
            verbose("%s: %3i: %s: close", i_filename, l_number-1, ot->name_s);
        }
        
        /* Store line if not a close. */
        if (ot && !l_is_close) {
            fputs(l, buf_fp);
        }
        
        /* Print template. */
        if (l_is_close) {
            /* Put final zero to the stored buffer. */
            fputc('\0', buf_fp);
            fflush(buf_fp);
            /* Try replacing the section. */
            err = ssnip_tmpl_write(ot, buf_m, _i_filename, _o_fd);
            /* If the replacement fails, put the stored again. Mark as invalid. */
            if (!err) {
                valid = false;
                write(_o_fd, buf_m, buf_msz-1);
            } else {
                count++;
            }
            /* Restore buffer. */
            fseek(buf_fp, 0, SEEK_SET);
            ot = NULL;
            /* Check whether there is another open. */
            l_is_open  = false;
            FOREACH(ssnip_tmpl,t,_s->t) {
                if (ssnip_tmpl_match_open(t, l)) {
                    ot = t;
                    l_is_open = true;
                    break;
                }
            }
            if (l_is_open && !_s->quiet) {
                verbose("%s: %3i: %s: open", i_filename, l_number, ot->name_s);
            }
            if (ot) {
                fputs(l, buf_fp);
            }
        }
        /* Write. */
        if (!ot) {
            write(_o_fd, l, strlen(l));
        }
    }
    /* Close when the file ends. */
    if (ot) {
        /* Put final zero to the stored buffer. */
        fputc('\0', buf_fp);
        fflush(buf_fp);
        /* Try replacing the section. */
        err = ssnip_tmpl_write(ot, buf_m, _i_filename, _o_fd);
        if (!err) {
            valid = false;
            write(_o_fd, buf_m, buf_msz-1);
        } else {
            count++;
        }
        if (!_s->quiet) {
            verbose("%s: eof: %s: close", i_filename, ot->name_s);
        }
    }
    /* Successfull. */
    if (_o_count) *_o_count = count;
    if (_o_valid) *_o_valid = valid;
    retval = true;
 cleanup:
    if (buf_fp) fclose(buf_fp);
    if (buf_m)  free(buf_m);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", i_filename, strerror(errno));
    goto cleanup;
}
bool ssnip_cat(ssnip *_s, const char _i_filename[], int _o_fd, int *_o_count, bool *_o_valid) {
    FILE   *fp      = NULL;
    bool    retval  = false;
    int     err     = 0;
    fp = fopen(_i_filename, "rb");
    if (!fp/*err*/) goto cleanup_errno;
    err = ssnip_process_fp(_s, _i_filename, fp, _o_fd, _o_count, _o_valid);
    if (!err/*err*/) goto cleanup;
    retval = true;
    goto cleanup;
 cleanup:
    if (fp) fclose(fp);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", _i_filename, strerror(errno));
    goto cleanup;
}
bool diff_files(bool *_same_content, FILE *_o_fp, const char _f1[], const char _f2[]) {
    bool  retval  = false;
    int   err     = -1;
    int   pid     = -1;
    int   status  = 0;
    int   p[2]    = {-1,-1};
    char  b[1024] = {0};
    /* Create pipe. */
    err = pipe(p);
    if (err == -1/*err*/) goto cleanup_errno; 
    /* Create diff process. */
    pid = fork();
    if (pid == -1/*err*/) goto cleanup_errno;
    if (pid == 0) {
        dup2(p[1], 1);
        close(p[0]);
        close(p[1]);
        execl(g_env_prog, g_env_prog, "diff", _f1, _f2, NULL);
        syslog(LOG_ERR, "Can't execute diff.");
        exit(1);
    }
    close(p[1]); p[1] = -1;
    /* Read to buffer. */
    while (1) {
        ssize_t r = read(p[0], b, sizeof(b));
        if (r == -1/*err*/) goto cleanup_errno;
        if (r == 0) break;
        if (_o_fp) {
            size_t w = fwrite(b, 1, r, _o_fp);
            if (w != r/*err*/) goto cleanup_errno;
        }
    }
    close(p[0]); p[0] = -1;
    /* Wait. */
    waitpid(pid, &status, 0); pid = -1;
    if (!WIFEXITED(status)/*err*/) goto cleanup_interrupted;
    status = WEXITSTATUS(status);
    if (status>1/*err*/) goto cleanup_diff_error;
    *_same_content = (status == 0)?true:false;
    retval = true;
    goto cleanup;
 cleanup:
    if (VALID_FD(p[0])) close(p[0]);
    if (VALID_FD(p[1])) close(p[1]);
    if (VALID_FD(pid))  waitpid(pid, NULL, 0);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "diff: %s", strerror(errno));
    goto cleanup;
 cleanup_interrupted:
    syslog(LOG_ERR, "diff: Interrupted.");
    goto cleanup;
 cleanup_diff_error:
    syslog(LOG_ERR, "diff: Returned error code %i.", status);
    goto cleanup;
}
bool pager_print(size_t _bsz, const char _b[]) {
    bool  retval = false;
    pid_t pid    = -1;
    int   p[2]   = {-1,-1};
    int   status = 0;
    int   err    = 0;
    /* Create pipe. */
    err = pipe(p);
    if (err==-1/*err*/) goto cleanup_errno;
    /* Create process. */
    pid = fork();
    if (pid==-1/*err*/) goto cleanup_errno;
    if (pid==0) {
        dup2(p[0], 0);
        close(p[0]);
        close(p[1]);
        execl(g_env_prog, g_env_prog, "less", "-R", NULL);
        syslog(LOG_ERR, "Can't execute less: %s", strerror(errno));
        exit(1);
    }
    close(p[0]); p[0] = -1;
    /* Write to pager. */
    ssize_t w = write(p[1], _b, _bsz);
    if (w == -1/*err*/) goto cleanup_errno;
    if (w != _bsz/*err*/) goto cleanup_pager_failed;
    close(p[1]); p[1] = -1;
    
    /* Wait child. */
    err = waitpid(pid, &status, 0); pid = -1;
    if (err==-1/*err*/) goto cleanup_errno;
    err = WIFEXITED(status);
    if (err==0/*err*/) goto cleanup_pager_failed;
    status = WEXITSTATUS(status);
    if (status!=0/*err*/) goto cleanup_pager_failed;
    retval = true;

 cleanup:
    if (p[0]!=-1) close(p[0]);
    if (p[1]!=-1) close(p[1]);
    if (pid!=-1)  waitpid(pid, NULL, 0);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "pager: %s", strerror(errno));
    goto cleanup;
 cleanup_pager_failed:
    syslog(LOG_ERR, "pager: Interrupted or returned error.");
    goto cleanup;
}
bool ssnip_replace(ssnip *_s, const char _io_filename[]) {
    bool        retval              = false;
    FILE       *i_fp                = NULL;
    FILE       *d_fp                = NULL;
    char       *d_mem               = NULL;
    size_t      d_memsz             = 0;
    bool        d_files_the_same_p  = false;
    int         o_fd                = -1;
    int         err                 = 0;
    int         count               = 0;
    bool        valid               = false;
    struct stat io_stat             = {0};
    
    char  t_filename[strlen(_io_filename)+3];
    char  yn[32] = {0};
    
    /* Get file stats. */
    err = stat(_io_filename, &io_stat);
    if (err==-1/*err*/) goto cleanup_errno;
    
    /* Open for reading. */
    i_fp = fopen(_io_filename, "r");
    if (!i_fp/*err*/) goto cleanup_errno;
    
    /* Open for writting. */
    sprintf(t_filename, "%s.t", _io_filename);
    o_fd = creat(t_filename, io_stat.st_mode);
    if (o_fd==-1/*err*/) goto cleanup_errno;

    /* Write template. */
    err = ssnip_cat(_s, _io_filename, o_fd, &count, &valid);
    if (!err/*err*/) goto cleanup;

    /* Close template. */
    close(o_fd); o_fd = -1;

    /* Without matches, simply skip. */
    if (count == 0) {
        retval = true;
        goto cleanup;
    }

    /* Calculate diff. Skip when no changes. */
    d_fp = open_memstream(&d_mem, &d_memsz);
    if (!d_fp/*err*/) goto cleanup_errno;
    err = diff_files(&d_files_the_same_p, d_fp, _io_filename, t_filename);
    if (!err/*err*/) goto cleanup;
    err = fflush(d_fp);
    if (err == EOF/*err*/) goto cleanup_errno; 
    if (d_files_the_same_p || d_mem == NULL) {
        if (!_s->quiet) {
            verbose("%s: File didn't change.", _io_filename);
        }
        retval = true;
        goto cleanup;
    }
    
    /* In interactive mode, show the difference. */
    if (_s->interactive) {
        err = pager_print(d_memsz, d_mem);
        if (!err/*err*/) goto cleanup;
        fputs("Apply the change? y/N: ", stderr);
        if (!fgets(yn, sizeof(yn)-1, stdin) || strcasecmp(yn, "y\n")) {
            syslog(LOG_WARNING, "%s: Cancelled.", _io_filename);
            retval = true;
            goto cleanup;
        }
    }

    /* Rename the file. */
    err = rename(t_filename, _io_filename);
    if (err==-1/*err*/) goto cleanup_errno;
    retval = true;

    /* Returns. */
 cleanup:
    unlink(t_filename);
    if (i_fp)     fclose(i_fp);
    if (o_fd!=-1) close(o_fd);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", _io_filename, strerror(errno));
    goto cleanup;
}
