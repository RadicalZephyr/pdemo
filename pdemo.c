// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// File        : pdemo.c
// Description : Programmed Dialogue with Interactive Programs
// License     :
//
//  Copyright (C) 2007-2013 Rachid Koucha <rachid dot koucha at free dot fr>
//
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//
// Evolutions  :
//
//     12-Jun-2007  R. Koucha    - Creation
//     03-Jul-2007  R. Koucha    - Added DOS mode to send CR/LF at end
//                                 of line instead of LF
//     16-Jul-2007  R. Koucha    - Better management of end of file on
//                                 input
//                               - Reset the signals before exiting
//     18-Jul-2007  R. koucha    - Added "sleep" command
//     03-Aug-2007  R. Koucha    - Enhanced the way we wait for the
//                                 child's status because we were
//                                 systematically calling "kill_chld"
//                                 which triggered sleeps
//     19-Sep-2007 R. Koucha     - The data coming from the program may
//                                 contain NUL chars (e.g. telnet login
//                                 session). So, when displaying the
//                                 data and searching synchro strings, we
//                                 ignore the NUL chars.
//                               - Suppress useless DOS mode
//                               - Better management of the synchro
//                                 strings
//     02-Oct-2007 R. Koucha     - Management of regular expressions for
//                                 for 'recv' command
//                               - Management of formated strings for
//                                 'send' command
//     16-Oct-2007 R. Koucha     - Suppressed useless -v = verbose
//                               - Added -V = version
//     19-Oct-2007 R. Koucha     - Fixed pb of failed exec program
//                                 (cf. comment above the definition
//                                 of pdemo_chld_failed_on_exec)
//     25-Nov-2007 R. Koucha     - Fixed management of offset in
//                                 pdemo_read_line()
//     14-Dec-2007 R. Koucha     - Support of POSIX regular expressions
//                               - Exit 1 on timeout
//                               - Better management of the string
//                                 parameters to allow '#' and \" inside*
//                                 them
//                               - Command to launch directly on the
//                                 command line
//     18-Jan-2008 R. Koucha     - Fixed infinite loop in pdemo_write()
//                                 when output file descriptor becomes
//                                 invalid (e.g. remote program crashed)
//                                 and so, write() returns -1
//                               - errno saving
//     21-Apr-2008 R. Koucha     - Mngt of exception signals
//     24-Apr-2008 R. Koucha     - Suppressed the debug print in SIGCHLD
//                                 handler
//     06-Feb-2009 R. Koucha     - pdemo_handle_synchro(): do not skip blank
//                                 chars !
//                               - Added ability to send ESC = 'CTRL [' control
//                                 character
//     10-Feb-2009 R. Koucha     - Fixed parameter mngt of sleep keyword
//     19-Aug-2009 R. Koucha     - Added 'print' command
//                               - Added 'dbg' command with debug level
//                               - Fixed pdemo_write() because it didn't work
//                                 when multiple writes were necessary
//                               - pdemo_read_program(): Replaced polling by a
//                                 timed wait
//                               - Better management of the end of the program
//                               - Dynamic allocation of the internal buffers
//                                 to accept very long lines
//     29-Nov-2009 R. Koucha     - Do not redirect the error output of the
//                                 child unless "-e" option is specified
//     25-Dec-2009 R. Koucha     - Management of background mode
//                               - Added -t option
//     29-Jul-2010 R. Koucha     - Made the slave side of the PTY become the
//                                 controlling terminal of the controlled
//                                 program
//                               - Added management of the control characters
//                                 through the notation "^character"
//                               - Added new keyword "sig" to send signals to
//                                 the controlled program
//     18-Aug-2010 R. Koucha     - Added option "-p" to propagate exit code of
//                                 controlled program (no longer the default
//                                 behaviour !)
//     10-Sep-2012 EMarchand     - Fixed some new gcc4.4 warnings
//     05-Jun-2013 R. Koucha     - Added 'sh' command
//     08-Oct-2013 R. Koucha     - Display (flush) outstanding data when
//                                 program finishes prematurely
//                               - Default buffer size 512 --> 4096
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#define _GNU_SOURCE
#include <getopt.h>
#include <sys/wait.h>
#include <libgen.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>


// ----------------------------------------------------------------------------
// Name   : pdemo_debug
// Usage  : Debug level
// ----------------------------------------------------------------------------
static int pdemo_debug;

// ----------------------------------------------------------------------------
// Name   : pdemo_in
// Usage  : Input of PDEMO (terminal or script)
// ----------------------------------------------------------------------------
static int pdemo_in;

// ----------------------------------------------------------------------------
// Name   : pdemo_out
// Usage  : Output of PDEMO (terminal)
// ----------------------------------------------------------------------------
static int pdemo_out;

// ----------------------------------------------------------------------------
// Name   : pdemo_err
// Usage  : Error output of PDEMO (terminal)
// ----------------------------------------------------------------------------
static int pdemo_err;

// ----------------------------------------------------------------------------
// Name   : pdemo_pty
// Usage  : Master side of the pseudo-terminal
// ----------------------------------------------------------------------------
static int pdemo_pty = -1;

// ----------------------------------------------------------------------------
// Name   : pdemo_background_in
// Usage  : Set to 1 if background mode and input is the terminal
// ----------------------------------------------------------------------------
static int pdemo_background_in;

// ----------------------------------------------------------------------------
// Name   : pdemo_background_out
// Usage  : Set to 1 if background mode and output is the terminal
// ----------------------------------------------------------------------------
static int pdemo_background_out;

// ----------------------------------------------------------------------------
// Name   : environ
// Usage  : Pointer on the environment variables
// ----------------------------------------------------------------------------
extern char **environ;

// ----------------------------------------------------------------------------
// Name   : PDEMO_COMMENT
// Usage  : Comment marker on the command line
// ----------------------------------------------------------------------------
#define PDEMO_COMMENT  '#'

// ----------------------------------------------------------------------------
// Name   : PDEMO_ERR
// Usage  : Error messages
// ----------------------------------------------------------------------------
#define PDEMO_ERR(format, ...) do { if (!pdemo_background_out)            \
            fprintf(stderr,                                             \
                    "PDEMO(%d) ERROR (%s#%d): "format,                   \
                    getpid(), __FUNCTION__, __LINE__, ## __VA_ARGS__);  \
    } while (0)

// ----------------------------------------------------------------------------
// Name   : PDEMO_DBG/DUMP
// Usage  : Debug messages
// ----------------------------------------------------------------------------
#define PDEMO_DBG(level, format, ...) do { if (!pdemo_background_out && (pdemo_debug >= level)) \
            fprintf(stderr,                                             \
                    "\nPDEMO_DBG(%d-%d) - %s#%d: "format,                \
                    getpid(), level,                                    \
                    __FUNCTION__, __LINE__, ## __VA_ARGS__);            \
    } while(0)

#define PDEMO_DUMP(level, b, l) do { if (!pdemo_background_out && (pdemo_debug >= level)) \
        {  pdemo_dump((b), (l)); }                                       \
    } while(0)

// ----------------------------------------------------------------------------
// Name   : pdemo_version
// Usage  : Version of the software
// ----------------------------------------------------------------------------
static const char *pdemo_version = "1.8.11";

// ----------------------------------------------------------------------------
// Name   : pdemo_bufsz
// Usage  : I/O buffer size
// ----------------------------------------------------------------------------
static unsigned int pdemo_bufsz;
#define PDEMO_DEF_BUFSZ   4096


// ----------------------------------------------------------------------------
// Name   : pdemo_buf
// Usage  : I/O buffer
// ----------------------------------------------------------------------------
static char *pdemo_buf;
static char *pdemo_buf1;

// ----------------------------------------------------------------------------
// Name   : pdemo_outstanding_buf
// Usage  : Buffer of data in which the synchro string is looked for before
//          being printed out
// ----------------------------------------------------------------------------
static char         *pdemo_outstanding_buf;
static unsigned int  pdemo_outstanding_buf_sz;
static unsigned int  pdemo_loutstanding;

// ----------------------------------------------------------------------------
// Name   : pdemo_argv/argc/argv_nb
// Usage  : Parameter table
// ----------------------------------------------------------------------------
static char **pdemo_argv    = NULL;
static int    pdemo_argc    = 0;
static int    pdemo_argv_nb = 0;


// ----------------------------------------------------------------------------
// Name   : pdemo_pid
// Usage  : Process id of the piloted child
// ----------------------------------------------------------------------------
static pid_t pdemo_pid = -1;


// ----------------------------------------------------------------------------
// Name   : pdemo_dead_prog
// Usage  : Set to one when the sub-process (program) dies
// ----------------------------------------------------------------------------
static int pdemo_dead_prog = 1;

// ----------------------------------------------------------------------------
// Name   : pdemo_exit_prog
// Usage  : Exit code of the dead program to propagate to PDEMO
// ----------------------------------------------------------------------------
static int pdemo_exit_prog = 0;


// ----------------------------------------------------------------------------
// Name   : pdemo_chld_failed_on_exec
// Usage  : 0, if child OK
//          1, if child failed on exec while father was forking
// Note   :
//          If the child fails because of a bad pathname for the name
//          of the program to exec, fork() will be interrupted in the
//          father and so, pdemo_pid will not be assigned before going
//          into the SIGCHLD handler.
//          Moreover, fork() does not return any error but the pid
//          of the terminated process !!!! Hence, the global variable
//          pdemo_chld_failed_on_exec that we set in the signal handler
//          if we face this situation.
//
//          I discovered this behaviour because my program was hanging. I
//          launched the debugger to attach the process and the call stack
//          showed that I was in the assert(pdemo_pid == pid) of the signal
//          handler: pdemo_pid was still with -1 value whereas pid (the return
//          code of waitpid() was the process id of the dead child)
// ----------------------------------------------------------------------------
static int pdemo_chld_failed_on_exec = 0;

// ----------------------------------------------------------------------------
// Name   : pdemo_longops
// Usage  : Option on the command line
// ----------------------------------------------------------------------------
static struct option pdemo_longopts[] =
    {
        { "script",     required_argument, NULL, 's' },
        { "bufsz",      required_argument, NULL, 'b' },
        { "debug",      required_argument, NULL, 'd' },
        { "version",    no_argument,       NULL, 'V' },
        { "error",      no_argument,       NULL, 'e' },
        { "term",       no_argument,       NULL, 't' },
        { "outstand",   no_argument,       NULL, 'o' },
        { "propexit",   no_argument,       NULL, 'p' },
        { "help",       no_argument,       NULL, 'h' },

        // Last entry
        {NULL, 0, NULL, 0 }
    };


//---------------------------------------------------------------------------
// Name : pdemo_help
// Usage: Display help
//----------------------------------------------------------------------------
static void pdemo_help(char *prog)
{
    char *p = basename(prog);

    fprintf(stderr,
            "\n%s %s\n"
            "\n"
            "Copyright (C) 2007-2013  Rachid Koucha\n"
            "\n"
            "This program comes with ABSOLUTELY NO WARRANTY.\n"
            "This is free software, and you are welcome to redistribute it\n"
            "under the terms of the GNU General Public License as published by\n"
            "the Free Software Foundation; either version 3 of the License , or\n"
            "(at your option) any later version.\n"
            "\n"
            "Usage: %s [<options> ...] -- <command> <params and/or options>...\n"
            "\n"
            "The command along with its parameters and options is launched and piloted by\n"
            "%s according to the commands from the script. The input and outputs\n"
            "of the program are redirected to %s.\n"
            "\n"
            "Options:\n"
            "\n"
            "If options are passed to %s and/or the command, then the command to launch\n"
            "must be separated from the options with a double hyphen (--).\n"
            "\n"
            "\t-s | --script            : Script of commands (default stdin)\n"
            "\n"
            "\t-b | --bufsz             : Size in bytes of the internal I/O buffer (default: %u)\n"
            "\t-d level | --debug=level : Set debug mode to 'level'\n"
            "\t-V | --version           : Display the version\n"
            "\t-e | --error             : Redirect error output of the controlled program\n"
            "\t-o | --outstand          : Dump outstanding data at the end of the session\n"
            "\t-p | --propexit          : Propagate the exit code of the controlled program\n"
            "\t-h | --help              : This help\n"
            ,
            p, pdemo_version,
            p,
            p,
            p,
            p,
            PDEMO_DEF_BUFSZ
            );
} // pdemo_help



// ----------------------------------------------------------------------------
// Name   : pdemo_free_argv
// Usage  : Deallocate the parameter table
// Return : None
// ----------------------------------------------------------------------------
static void pdemo_free_argv(void)
{
    assert(pdemo_argv_nb >= 0);
    if (pdemo_argv_nb)
    {
        free(pdemo_argv);
    }
    pdemo_argv = NULL;
    pdemo_argc = 0;
    pdemo_argv_nb = 0;
} // pdemo_free_argv


// ----------------------------------------------------------------------------
// Name   : uint_to_char
// Usage  : Translate an 'unsigned int' into a string of characters
// Return : None
// ----------------------------------------------------------------------------
static void uint_to_char(
                         unsigned int   val,
                         unsigned char *buf,
                         unsigned int  len)
{
    int i;

    for (i = (int)(len - 1); i >= 0; i --)
    {
        if ((val % 10) != val)
        {
            buf[i]  = (unsigned char)((val % 10) + '0');
            val    /= 10;
        }
        else
        {
            buf[i] = (unsigned char)(val + '0');
            val    = 0;
        } /* end if */
    } /* end for */

} /* uint_to_char */

static unsigned int blk_size = 512U;
static unsigned int absLineNumber = 0;

// ----------------------------------------------------------------------------
// Name   : pdemo_dump
// Usage  : Dump a memory zone on stderr
// Return : None
// ----------------------------------------------------------------------------
void pdemo_dump(
               char         *buf,
               unsigned int  size_buf
               )
{
    static char  line[77 + 1];
    unsigned int nb_line;
    unsigned int i;
    unsigned int last_line;
    unsigned int j;
    unsigned int k;
    unsigned int size_line;
    static unsigned int lineNumber = 0;

    /* Line format : lineno(10)_bytes(16 * 2 + 15)_*ascii(16)* */

    size_line = (blk_size >= 16 ? 16 : blk_size);

    nb_line = size_buf / size_line;

    last_line = size_buf % size_line;

    /* Initialization of the buffer */
    for (i = 0; i < 77; i ++)
    {
        line[i] = ' ';
    }

    /* The stars */
    line[59] = '*';
    line[76] = '*';

    /* Terminating NUL */
    line[77] = '\0';

    /* Index in the buffer */
    k = 0;

    for (i = 0; i < nb_line; i++)
    {
        /* Write the line number */
        if (absLineNumber)
        {
            uint_to_char(lineNumber, (unsigned char *)line, 10);
            lineNumber += 16;
        }
        else
        {
            uint_to_char(i, (unsigned char *)line, 10);
        }

        /* dump the "size_line" octets */
        for (j = 0; j < size_line; j ++, k++)
        {
            if (((unsigned char)(buf[k]) % 16) < 10)
            {
                (line + 11)[j * 3 + 1] = (char)(((unsigned char)(buf[k]) % 16) + '0');
            }
            else
            {
                (line + 11)[j * 3 + 1] = (char)((((unsigned char)(buf[k]) % 16) - 10) + 'A');
            }

            if (((unsigned char)(buf[k]) / 16) < 10)
            {
                (line + 11)[j * 3]     = (char)(((unsigned char)(buf[k]) / 16) + '0');
            }
            else
            {
                (line + 11)[j * 3]     = (char)((((unsigned char)(buf[k]) / 16) - 10) + 'A');
            }

            /* ASCII translation */
            if (((unsigned char)(buf[k]) >= 32) && ((unsigned char)(buf[k]) <= 127))
            {
                (line + 60)[j] = (char)(buf[k]);
            }
            else
            {
                (line + 60)[j] = '.';
            }
        } /* end for */

        fprintf(stderr, "%s\n", line);
    } /* end for */

    if (last_line)
    {
        /* Line number */
        if (absLineNumber)
        {
            uint_to_char(lineNumber, (unsigned char *)line, 10);
        }
        else
        {
            uint_to_char(i, (unsigned char *)line, 10);
        }

        /* dump of 16 bytes */
        for (j = 0; j < last_line; j ++, k++)
        {
            if (((unsigned char)(buf[k]) % 16) < 10)
            {
                (line + 11)[j * 3 + 1] = (char)(((unsigned char)(buf[k]) % 16) + '0');
            }
            else
            {
                (line + 11)[j * 3 + 1] = (char)((((unsigned char)(buf[k]) % 16) - 10) + 'A');
            }

            if (((unsigned char)(buf[k]) / 16) < 10)
            {
                (line + 11)[j * 3]     = (char)(((unsigned char)(buf[k]) / 16) + '0');
            }
            else
            {
                (line + 11)[j * 3]     = (char)((((unsigned char)(buf[k]) / 16) - 10) + 'A');
            }

            /* ASCII translation */
            if ((unsigned char)(buf[k]) >= 32 && (unsigned char)(buf[k]) <= 127)
            {
                (line + 60)[j] = (char)(buf[k]);
            }
            else
            {
                (line + 60)[j] = '.';
            }
        } /* end for */

        for (j = last_line; j < size_line; j ++)
        {
            (line + 11)[j * 3 + 1] = ' ';
            (line + 11)[j * 3]     = ' ';
            (line + 60)[j]         = ' ';
        } /* end for */

        fprintf(stderr, "%s\n", line);
    } /* end if last_line */
} // pdemo_dump


#if 0

//----------------------------------------------------------------------------
// Name        : pdemo_reset_sigchld
// Description : Unset the handlers for the signals
//----------------------------------------------------------------------------
static void pdemo_reset_sigchld(void)
{
    sighandler_t p;
    int          err_sav;

    p = signal(SIGCHLD, SIG_DFL);
    if (SIG_ERR == p)
    {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on signal()\n", errno);
        errno = err_sav;
        return;
    }

} // pdemo_reset_sigchld

#endif // 0


//----------------------------------------------------------------------------
// Name        : pdemo_kill_chld
// Description : Terminate the child process
//----------------------------------------------------------------------------
static void pdemo_kill_chld(void)
{
    int status;

    // Unset SIGCHLD
    // RECTIFICATION: We don't unset the SIGCHLD signal otherwise all
    //                subsequent calls to waitpid() fail with ECHILD and
    //                the sub-process is detached to belong to init and
    //                will live sometimes before the cyclic cleanup of init
    //
    //pdemo_reset_sigchld();

    // If the child process was not finished just before the reset of SIGCHLD
    if (!pdemo_dead_prog)
    {
        kill(pdemo_pid, SIGTERM);
        sleep(1);

        // The signal handler for SIGCHLD may be triggered if the child dies

        // If the sub-process didn't died ==> SIGKILL
        if (!pdemo_dead_prog)
        {
            kill(pdemo_pid, SIGKILL);
        }

        // The SIGCHLD signal handler must be triggered

        // Wait for the termination of the child (this may be done
        // by the SIGCHLD handler but we need to wait to make sure
        // that the sub-process will not become a zombie)
        (void)waitpid(-1, &status, 0);
    }
} // pdemo_kill_chld


//----------------------------------------------------------------------------
// Name        : pdemo_sig_tt
// Description : Signal handler for SIGTTIN/OU
//----------------------------------------------------------------------------
static void pdemo_sig_tt(int sig)
{
    switch(sig)
    {
    case SIGTTOU : // Attempt to write to terminal while in background mode
        {
            pdemo_background_out = 1;

            (void)signal(SIGTTOU, SIG_DFL);
        }
        break;

    case SIGTTIN : // Attempt to read from terminal while in background mode
        {
            pdemo_background_in = 1;

            // This will make subsequent reads from terminal fail with EIO
            (void)signal(SIGTTIN, SIG_IGN);
        }
        break;

    default :
        {
            assert(0);
        }
    } // End switch
} // pdemo_sig_tt


//----------------------------------------------------------------------------
// Name        : pdemo_read
// Description : Read input data
// Return      : Number of read bytes if OK
//               -1, if error
//               -2, if background mode
//----------------------------------------------------------------------------
static int pdemo_read(
                     int           fd,
                     char         *buf,
                     unsigned int  l
                     )
{
    int rc;
    int err_sav;

    do
    {
        rc = read(fd, buf, l);
        if (-1 == rc)
        {
            if (EINTR == errno)
            {
                continue;
            }

            // If launched in background and the input is the
            // terminal, we will receive a SIGTTIN. The signal handler
            // will set pdemo_background_in to 1
            if ((pdemo_in == fd) && (pdemo_background_in))
            {
                return -2;
            }

            err_sav = errno;
            //PDEMO_ERR("Error '%m' (%d) on read(fd:%d, l:%u)\n", errno, fd, l);
            errno = err_sav;
            return -1;
        }
    } while (rc < 0);

    return rc;
} // pdemo_read


//----------------------------------------------------------------------------
// Name        : pdemo_read_to
// Description : Read input data with a timeout
// Return      : Number of read bytes if OK
//               -1, if error (errno set to ETIMEDOUT if timeout)
//----------------------------------------------------------------------------
int pdemo_read_to(
                 int           fd,
                 char         *buf,
                 unsigned int  l,
                 unsigned int  timeout    // Timeout in seconds
                 )
{
    int             rc;
    fd_set          fdset;
    struct timeval  to;

 one_more_time:

    // Make the list of supervised file descriptors
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);

    to.tv_sec  = (time_t)timeout;
    to.tv_usec = 0;

    rc = select(fd + 1, &fdset, NULL, NULL, &to);

    switch (rc)
    {
    case -1 : // Error
        {
            if (EINTR == errno)
            {
                goto one_more_time;
            }

            return -1;
        }
        break;

    case 0: // Timeout
        {
            errno = ETIMEDOUT;
            return -1;
        }
        break;

    default : // Incoming data
        {
            assert(FD_ISSET(fd, &fdset));

            return pdemo_read(fd, buf, l);
        }
        break;
    } // End switch

    return rc;
} // pdemo_read_to



//----------------------------------------------------------------------------
// Name        : pdemo_dump_outstanding_data
// Description : Get latest data from program
//----------------------------------------------------------------------------
static void pdemo_dump_outstanding_data(void)
{
    int rc;

    PDEMO_DBG(0, "Outstanding data from program are:\n");

    if (pdemo_loutstanding)
    {
        PDEMO_DUMP(0, pdemo_outstanding_buf, pdemo_loutstanding);
    }

    if (pdemo_pty >= 0)
    {
        do
        {
            rc = pdemo_read_to(pdemo_pty,
                              pdemo_buf,
                              pdemo_bufsz,
                              0);

            if (rc > 0)
            {
                PDEMO_DUMP(0, pdemo_buf, (unsigned int)rc);
            }
        } while (rc > 0);
    }

} // pdemo_dump_outstanding_data


//----------------------------------------------------------------------------
// Name        : pdemo_sig_chld
// Description : Signal handler for death of child
//----------------------------------------------------------------------------
static void pdemo_sig_chld(int sig)
{
    pid_t pid;
    int   status;

    assert(SIGCHLD == sig);

    // Get the status of the child
    pid = waitpid(-1, &status, WNOHANG);

    PDEMO_DBG(6, "Received SIGCHLD signal from process %d\n", pid);

    // If error
    if (-1 == pid)
    {
        //PDEMO_ERR("Error %d '%m' from waitpid()\n", errno);
        assert(ECHILD == errno);
        return;
    }

    // If it is an asynchronous program
    if (pdemo_pid != pid)
    {
        if (WIFEXITED(status))
        {
            PDEMO_DBG(2, "Sub process with pid %d exited with code %d\n", pid, WEXITSTATUS(status));
        }
        else
        {
            if (WIFSIGNALED(status))
            {
                PDEMO_DBG(1, "Sub process with pid %d finished with signal %d%s\n", pid, WTERMSIG(status), (WCOREDUMP(status) ? " (core dumped)" : ""));
            }
            else
            {
                PDEMO_DBG(1, "Sub process with pid %d finished in error\n", pid);
            }
        }

        return;
    }

    // We must have at least one child
    if (-1 != pdemo_pid)
    {
        // When PDEMO encounters en EOF on its standard input, it closes
        // the PTY. This triggers an EOF on the program's side which may
        // finish as well. So, the waitpid() in the main() function catches
        // the status of the dead program and we receive a SIGCHLD to trigger
        // this handler. But in this case, the status is no longer available
        // of course...
        if (pid != -1)
        {
            if (WIFEXITED(status))
            {
                pdemo_exit_prog = WEXITSTATUS(status);

                // Debug message if normal exit or debug activated
                PDEMO_DBG(1, "Sub process with pid %d exited with code %d\n", pid, pdemo_exit_prog);

                // If error and debug not activated, force an error message
                if (pdemo_exit_prog != 0)
                {
                    if (0 == pdemo_debug)
                    {
                        PDEMO_DBG(0, "Sub process with pid %d exited with code %d\n", pid, pdemo_exit_prog);
                    }

                    // If error, dump the outstanding data in case
                    // error messages from the program are available
                    // pdemo_dump_outstanding_data();
                }
            }
            else
            {
                if (WIFSIGNALED(status))
                {
                    PDEMO_ERR("Sub process with pid %d finished with signal %d%s\n", pid, WTERMSIG(status), (WCOREDUMP(status) ? " (core dumped)" : ""));
                }
                else
                {
                    PDEMO_ERR("Sub process with pid %d finished in error\n", pid);
                }

                // Default error exit code
                pdemo_exit_prog = 1;

                // Dump the outstanding data in case error messages from
                // the program are available
                //pdemo_dump_outstanding_data();
            } // End if exited
        }
        else
        {
            assert(ECHILD == errno);
        }
    }
    else
    {
        // We failed inside the fork() ==> See comment above
        // pdemo_chld_failed_on_exec definition
        PDEMO_DBG(1, "Child %d finished in error\n", pid);
        pdemo_chld_failed_on_exec = 1;

        // Default error exit code
        pdemo_exit_prog = 1;

        return;
    }

    // Warn the father
    pdemo_dead_prog = 1;

    // Make sure that only one child is reported
    pid = waitpid(-1, &status, WNOHANG);

    // We must not have any terminated child
    assert((0 == pid) || ((-1 == pid) && (ECHILD == errno)));
} // pdemo_sig_chld



//----------------------------------------------------------------------------
// Name        : pdemo_sig_alarm
// Description : Signal handler for ALARM
//----------------------------------------------------------------------------
static void pdemo_sig_alarm(int sig)
{
    assert(SIGALRM == sig);

    PDEMO_DBG(4, "Timeout while waiting for child\n");

    pdemo_kill_chld();

    exit(1);
} // pdemo_sig_alarm




//----------------------------------------------------------------------------
// Name        : pdemo_capture_sigchld
// Description : Set the handlers for the SIGCHLD signal
//----------------------------------------------------------------------------
static void pdemo_capture_sigchld(void)
{
    struct sigaction    action;
    sigset_t            sig_set;
    int                 rc;
    int                 err_sav;

    // We block SIGCHLD while managing SIGCHLD
    rc = sigemptyset(&sig_set);
    rc = sigaddset(&sig_set, SIGCHLD);

    // Set the handler for SIGCHLD
    memset(&action, 0, sizeof(action));
    action.sa_handler   = pdemo_sig_chld;
    action.sa_mask      = sig_set;
    action.sa_flags     = SA_NOCLDSTOP | SA_RESTART | SA_RESETHAND;
    rc = sigaction(SIGCHLD, &action, NULL);
    if (0 != rc)
    {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on sigaction()\n", errno);
        errno = err_sav;
        return;
    }
} // pdemo_capture_sigchld


//----------------------------------------------------------------------------
// Name        : pdemo_sig_exception
// Description : Exception handler
//----------------------------------------------------------------------------
static void pdemo_sig_exception(
                               int        sig,
                               siginfo_t *info,
                               void      *context
                               )
{
    //ucontext_t  *pCtx = (ucontext_t *)context;

    (void)context;

    /*
      The structure siginfo_t contains the following information :
      . The si_signo member contains the system-generated signal number
      . The si_errno member may contain implementation-defined additional
      error information; if non-zero, it contains an error number
      identifying the condition that caused the signal to be generated.
      . The si_code member contains a code identifying the cause of the signal.
      If the value of si_code is less than or equal to 0, then the signal was
      generated by a process and si_pid and si_uid,  respectively, indicate
      the process ID and the real user ID of the sender.
    */
    PDEMO_ERR("PDEMO crashed with signal %d at address %p\n", sig, info->si_addr);

    abort();
} // pdemo_sig_exception


//----------------------------------------------------------------------------
// Name        : pdemo_capture_exception_sig
// Description : Set the handlers for the exception signals
//----------------------------------------------------------------------------
static void pdemo_capture_exception_sig(void)
{
    struct sigaction    action;
    sigset_t            sig_set;
    int                 rc;
    int                 err_sav;
    int                 sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, 0};
    int                 i;

    rc = sigemptyset(&sig_set);
    i = 0;
    while (sigs[i])
    {
        rc = sigaddset(&sig_set, sigs[i]);
        i ++;
    }

    // Set the handler
    i = 0;
    while (sigs[i])
    {
        memset(&action, 0, sizeof(action));
        action.sa_sigaction = pdemo_sig_exception;
        action.sa_mask      = sig_set;
        action.sa_flags     = SA_NOCLDSTOP | SA_RESTART | SA_RESETHAND | SA_SIGINFO;
        rc = sigaction(sigs[i], &action, NULL);
        if (0 != rc)
        {
            err_sav = errno;
            PDEMO_ERR("Error '%m' (%d) on sigaction()\n", errno);
            errno = err_sav;
            return;
        }

        i ++;
    } // End while

} // pdemo_capture_exception_sig





//----------------------------------------------------------------------------
// Name        : pdemo_read_line
// Description : Read input data from 'fd' until end of line or end of file
//               or when 'l' chars have been read
// Return      : Number of read bytes if OK
//               -1, if error
//               -2, if background mode
//----------------------------------------------------------------------------
static int pdemo_read_line(
                          int           fd,
                          char         *buf,
                          unsigned int  l
                          )
{
    int rc;
    int offset = 0;
    int err_sav;

    assert(l > 0);

    do
    {
        rc = pdemo_read(fd, &(buf[offset]), 1);
        if (-1 == rc)
        {
            err_sav = errno;
            PDEMO_ERR("Error '%m' (%d) on read(%d)\n", errno, fd);
            errno = err_sav;
            return -1;
        }

        // Background mode ?
        if (-2 == rc)
        {
            return -2;
        }

        // End of file ?
        if (0 == rc)
        {
            return offset;
        }

        // End of line ?
        if ('\n' == buf[offset])
        {
            return offset + 1;
        }

        // One more char...
        offset ++;

        // End of buffer ?
        if (offset >= (int)l)
        {
            return offset;
        }

    } while (1);

} // pdemo_read_line




//----------------------------------------------------------------------------
// Name        : pdemo_write
// Description : Write out data
// Return      : len, if OK
//               -1, if error (errno is set)
//               -2, if background mode
//----------------------------------------------------------------------------
static int pdemo_write(
                      int           fd,
                      const char    *buf,
                      unsigned int  len
                      )
{
    int          rc;
    unsigned int l;
    int          saved_errno;

    l = 0;
    do
    {
    one_more_time:

        rc = write(fd, buf + l, len - l);

        if (rc < 0)
        {
            if (EINTR == errno)
            {
                goto one_more_time;
            }

            // If launched in background and the output is the
            // terminal, we will receive a SIGTTOU. The signal handler
            // will set pdemo_background_out to 1
            if ((pdemo_out == fd || pdemo_err == fd) && (pdemo_background_out))
            {
                return -2;
            }

            saved_errno = errno;
            PDEMO_ERR("Error '%m' (%d) on write()\n", errno);
            errno = saved_errno;
            return -1;
        }

        assert((l + (unsigned int)rc) <= len);
        l += (unsigned int)rc;
    } while (l != len);

    return (int)len;

} // pdemo_write


//----------------------------------------------------------------------------
// Name        : pdemo_terminal
// Description : Simulate a simple terminal in front of the program
// Return      : 0, if end of input file
//               -1, error
//----------------------------------------------------------------------------
static int pdemo_terminal(void)
{
    int             rc;
    fd_set          fdset;
    int             max_fd;
    int             fd_input = pdemo_in;           // To get data from user
    int             fd_program = pdemo_pty;        // To interact with the program
    int             loop = 1;
    pid_t           pid = pdemo_pid;
    int             err_sav;
    unsigned int    lbuf;

    while (loop)
    {
        // If the program is dead
        if (pdemo_dead_prog)
        {
            PDEMO_DBG(1, "Command (pid = %u) is finished\n", pid);

            // This is not an error ?
            rc = 0;
            goto end;
        } // End if program dead

        // Set the file descriptors to listen to
        FD_ZERO(&fdset);
        FD_SET(fd_input, &fdset);
        FD_SET(fd_program, &fdset);

        max_fd = (fd_input > fd_program ? fd_input + 1 : fd_program + 1);

        rc = select(max_fd, &fdset, NULL, NULL, NULL);

        switch(rc)
        {
        case -1 :
            {
                if (EINTR == errno)
                {
                    continue;
                }

                err_sav = errno;
                PDEMO_ERR("Error '%m' (%d) on select\n", errno);
                errno = err_sav;
                rc = -1;
                goto end;
            }
            break;

        default :
            {
                // If data from program
                if (FD_ISSET(fd_program, &fdset))
                {
                    rc = pdemo_read(fd_program,
                                   pdemo_buf,
                                   pdemo_bufsz - 1);
                    if (rc < 0)
                    {
                        rc = -1;
                        goto end;
                    }

                    if (0 == rc)
                    {
                        PDEMO_ERR("End of program\n");
                        goto end;
                    }

                    pdemo_buf[rc] = '\0';

                    // Length of the buffer
                    lbuf = (unsigned int)rc;

                    PDEMO_DBG(2, "Received %u bytes from program:\n", lbuf);
                    PDEMO_DUMP(2, pdemo_buf, lbuf);

                    // Forward the data to the output
                    rc = pdemo_write(pdemo_out, pdemo_buf, lbuf);
                    if ((unsigned int)rc != lbuf)
                    {
                        if (-2 != rc)
                        {
                            PDEMO_ERR("Error on write\n");
                            rc = -1;
                            goto end;
                        }
                    }
                } // End if data from program

                // If data from user
                if (FD_ISSET(fd_input, &fdset))
                {
                    rc = pdemo_read_line(fd_input, pdemo_buf, pdemo_bufsz - 1);
                    if (-1 == rc)
                    {
                        rc = -1;
                        goto end;
                    }

                    // EOF
                    if (0 == rc)
                    {
                        goto end;
                    }

                    // If background mode
                    if (-2 == rc)
                    {
                        // Simulate no input data
                        rc = 0;
                    }

                    rc = pdemo_write(pdemo_pty, pdemo_buf, (unsigned int)rc);
                    if (rc < 0)
                    {
                        rc = -1;
                        goto end;
                    }
                } // End if data from user
            }
            break;
        } // End switch
    } // End while

 end:

    return rc;
} // pdemo_terminal


typedef struct
{
    const char *name;
    int         sig;
} pdemo_sig2name;


// ----------------------------------------------------------------------------
// Options compile constants
// ----------------------------------------------------------------------------

#define HAS_SCRIPT     0x1
#define HAS_BUFFSIZE   0x2
#define HAS_DEBUGLVL   0x4
#define VERSION        0x8
#define REDIRECT_ERR   0x10
#define DUMP_O_DATA    0x40
#define PROPAGATE_EXIT 0x80


// ----------------------------------------------------------------------------
// Name   : main
// Usage  : Entry point
// ----------------------------------------------------------------------------
int main(
         int   ac,
         char *av[]
         )
{
    unsigned int   options;
    int            opt;
    int            fds;
    char          *pty_slave_name;
    unsigned int   nb_cmds;
    char         **params;
    unsigned int   i;
    int            rc;
    char          *pScript = NULL;
    int            err_sav;

    // Outputs of PDEMO
    pdemo_out = 1;
    pdemo_err = 2;

    // Capture the exception signals
    pdemo_capture_exception_sig();

    // Manage SIGTTOU/TTIN (for background mode)
    (void)signal(SIGTTOU, pdemo_sig_tt);
    (void)signal(SIGTTIN, pdemo_sig_tt);

    options = 0;

    while ((opt = getopt_long(ac, av, "s:b:d:Veoph", pdemo_longopts, NULL)) != EOF)
    {
        switch(opt)
        {
        case 's' : // Script to interpret
            {
                pScript = optarg;
                options |= HAS_SCRIPT;
            }
            break;

        case 'b' : // Set the size of the internal I/O buffer
            {
                pdemo_bufsz = (unsigned int)atoi(optarg);
                options |= HAS_BUFFSIZE;
            }
            break;

        case 'd' : // Debug level
            {
                pdemo_debug = atoi(optarg);
                options |= HAS_DEBUGLVL;
            }
            break;

        case 'V' : // Version
            {
                options |= VERSION;
            }
            break;

        case 'e' : // Redirect standard error as well
            {
                options |= REDIRECT_ERR;
            }
            break;

        case 'o' : // Dump outstanding data at the end of the session
            {
                options |= DUMP_O_DATA;
            }
            break;

        case 'p' : // Propagate exit code of controlled program
            {
                options |= PROPAGATE_EXIT;
            }
            break;

        case 'h' : // Help
            {
                pdemo_help(av[0]);
                exit(0);
            }
            break;

        case '?' :
        default :
            {
                pdemo_help(av[0]);
                exit(1);
            }
        } // End switch
    } // End while

    // If version requested
    if (options & VERSION)
    {
        printf("PDEMO version: %s\n", pdemo_version);
        exit(0);
    }

    // If no command passed
    if (ac == optind)
    {
    PDEMO_ERR("A command to launch is expected at the end of the command line \n\n");
    pdemo_help(av[0]);
    exit(1);
  }

  // If the buffer size has not been set or is invalid
  if (pdemo_bufsz <= 100)
  {
    pdemo_bufsz = PDEMO_DEF_BUFSZ;
  }

  // Get a master pty
  //
  // posix_openpt() opens a pseudo-terminal master and returns its file
  // descriptor.
  // It is equivalent to open("/dev/ptmx",O_RDWR|O_NOCTTY) on Linux systems :
  //
  //       . O_RDWR Open the device for both reading and writing
  //       . O_NOCTTY Do  not  make  this device the controlling terminal
  //         for the process
  pdemo_pty = posix_openpt(O_RDWR |O_NOCTTY);
  if (pdemo_pty < 0)
  {
    err_sav = errno;
    PDEMO_ERR("Impossible to get a master pseudo-terminal - errno = '%m' (%d)\n", errno);
    errno = err_sav;
    return 1;
  }

  // Grant access to the slave pseudo-terminal
  // (Chown the slave to the calling user)
  if (0 != grantpt(pdemo_pty))
  {
    err_sav = errno;
    PDEMO_ERR("Impossible to grant access to slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    close(pdemo_pty);
    errno = err_sav;
    return 1;
  }

  // Unlock pseudo-terminal master/slave pair
  // (Release an internal lock so the slave can be opened)
  if (0 != unlockpt(pdemo_pty))
  {
    err_sav = errno;
    PDEMO_ERR("Impossible to unlock pseudo-terminal master/slave pair - errno = '%m' (%d)\n", errno);
    close(pdemo_pty);
    errno = err_sav;
    return 1;
  }

  // Get the name of the slave pty
  pty_slave_name = ptsname(pdemo_pty);
  if (NULL == pty_slave_name)
  {
    err_sav = errno;
    PDEMO_ERR("Impossible to get the name of the slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    close(pdemo_pty);
    errno = err_sav;
    return 1;
  }

  // Open the slave part of the terminal
  fds = open(pty_slave_name, O_RDWR);
  if (fds < 0)
  {
    err_sav = errno;
    PDEMO_ERR("Impossible to open the slave pseudo-terminal - errno = '%m' (%d)\n", errno);
    close(pdemo_pty);
    errno = err_sav;
    return 1;
  }

  // Allocate the parameters for the command
  nb_cmds = (unsigned int)(ac - optind);
  params = (char **)malloc(sizeof(char *) * (nb_cmds + 1));
  if (!params)
  {
    err_sav = errno;
    PDEMO_ERR("Unable to allocate the parameters for the command (%u)\n", nb_cmds);
    errno = err_sav;
    return  1;
  }

  // Set the parameters for the command
  for (i = 0; i < nb_cmds; i ++)
  {
    params[i] = av[optind + (int)i];
  } // End for
  params[i] = NULL;

  // Capture SIGCHLD
  pdemo_capture_sigchld();

  // Fork a child
  pdemo_pid = fork();
  switch(pdemo_pid)
  {
    case -1 :
    {
      err_sav = errno;
      PDEMO_ERR("Error '%m' (%d) on fork()\n", errno);
      errno = err_sav;
      return 1;
    }
    break;

    case 0 : // Child
    {
    pid_t mypid = getpid();
    int   fd;

      assert(fds > 2);
      assert(pdemo_pty > 2);

      // Redirect input/outputs to the slave side of PTY
      close(0);
      close(1);
      if (options & REDIRECT_ERR)
      {
        close(2);
      }
      fd = dup(fds);
      assert(fd >= 0);
      fd = dup(fds);
      assert(fd >= 0);
      if (options & REDIRECT_ERR)
      {
        fd = dup(fds);
        assert(fd >= 0);
      }

      // Make some cleanups
      close(fds);
      close(pdemo_pty);

      fds = 0;

#if 0
      // Remove controlling terminal if any
      rc = ioctl(fds, TIOCNOTTY);
      if (rc < 0)
      {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on ioctl(TIOCNOTTY)\n", errno);
        errno = err_sav;
        exit(1);
      }
#endif // 0

      // Make the child become a process session leader
      rc = setsid();
      if (rc < 0)
      {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on setsid()\n", errno);
        errno = err_sav;
        exit(1);
      }

      // As the child is a session leader, set the controlling
      // terminal to be the slave side of the PTY
      rc = ioctl(fds, TIOCSCTTY, 1);
      if (rc < 0)
      {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on ioctl(TIOCSCTTY)\n", errno);
        errno = err_sav;
        exit(1);
      }

#if 0
      // Make the foreground process group on the terminal be the
      // process id of the child
      rc = ioctl(fds, TIOCSPGRP, &mypid);
      if (rc < 0)
      {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on ioctl(TIOCSPGRP)\n", errno);
        errno = err_sav;
        exit(1);
      }
#endif // 0

      // Make the foreground process group on the terminal be the
      // process id of the child
      rc = tcsetpgrp(fds, mypid);
      if (rc < 0)
      {
        err_sav = errno;
        PDEMO_ERR("Error '%m' (%d) on setpgid()\n", errno);
        errno = err_sav;
        exit(1);
      }

      // Exec the program
      rc = execvp(params[0], params);

      // The error message can't be generated as the outputs are redirected to the PTY
      //PDEMO_ERR("Error '%m' (%d) while running '%s'\n", errno, params[0]);

      _exit(1);
    }
    break;

    default: // Father
    {
    int   status;

      PDEMO_DBG(1, "Forked process %d for program '%s'\n", pdemo_pid, params[0]);

      // See comment above pdemo_chld_failed_on_exec definition
      if (pdemo_chld_failed_on_exec)
      {
        PDEMO_ERR("Error while running '%s'\n", params[0]);
        return 1;
      }

      // The program is running
      pdemo_dead_prog = 0;

      // Close the slave side of the PTY
      close(fds);

      // Allocate the I/O buffers
      pdemo_buf = (char *)malloc(pdemo_bufsz);
      assert(pdemo_buf);
      pdemo_buf1 = (char *)malloc(pdemo_bufsz);
      assert(pdemo_buf1);

      // Allocate the outstanding data buffer
      pdemo_outstanding_buf_sz = pdemo_bufsz * 2;
      pdemo_outstanding_buf = (char *)malloc(pdemo_outstanding_buf_sz);
      assert(pdemo_outstanding_buf);

      // Open the input
      if (pScript)
      {
        pdemo_in = open(pScript, O_RDONLY);
        if (pdemo_in < 0)
        {
          err_sav = errno;
          PDEMO_ERR("Unable to open '%s' (Error '%m' (%d))\n", pScript, errno);
          errno = err_sav;
          return 1;
        }
      }
      else
      {
        pdemo_in = 0;
      }

      // Interact with the program
      rc = pdemo_terminal();

      /*
        When the master device is closed, the process on the slave side gets
        the errno ENXIO when attempting a write() system call on the slave
        device but it will be able to read any data remaining on the slave
        stream. Finally, when all the data have been read, the read() system
        call will return 0 (zero) indicating that the slave can no longer be
        used.
      */
      if (options & DUMP_O_DATA)
      {
        pdemo_dump_outstanding_data();
      }
      close(pdemo_pty);
      pdemo_pty = -1;
      free(pdemo_buf);

      // Reset the signal SIGCHLD otherwise we may receive the end of child
      // with the following waitpid() and then we will receive the SIGCHLD
      // which would trigger the signal handler in which waitpid() would
      // return in error
      //
      // RECTIFICATION: We don't unset the SIGCHLD signal otherwise all
      //                subsequent calls to waitpid() fail with ECHILD and
      //                the sub-process is detached to belong to init and
      //                will live sometimes before the cyclic cleanup of init
      //
      //pdemo_reset_sigchld();

      // If the child is still running
      if (!pdemo_dead_prog)
      {
        PDEMO_DBG(4, "Wait for end of program at most 3 seconds\n");

        // Install a timeout
        signal(SIGALRM, pdemo_sig_alarm);
        alarm(3);

        // Get the status of the child if not dead yet
        (void)waitpid(-1, &status, 0);
      } // End if program is running

      // Free the parameters
      pdemo_free_argv();

      // If timeout or syntax error or any other error
      if (rc != 0)
      {
        return 1;
      }

      // If sub-process terminated in error, propagate its exit code if
      // requested
      if (options & PROPAGATE_EXIT)
      {
        return pdemo_exit_prog;
      }
    }
    break;
  } // End switch

  return 0;
} // main
