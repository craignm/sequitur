/******************************************************************************

 getopt.c - An implementation of getopt() [command line options parsing].

   getopt() - used to parse command line options - is a standard library
   function on Unix systems. If you are on Unix/Linux, you shouldn't need this
   module to build your program. Use this module if getopt() is not available
   otherwise.

 Taken from: http://www.funducode.com/freec/misc_c/misc_c5.htm

 *******************************************************************************/

#include <stdio.h>
#include <string.h>

#define NONOPT (-1)

char *optarg = NULL;
int optind = 1;
static int offset = 0;

int getopt ( int argc, char **argv, char *optstring )
{
    char *group, option, *s;
    int len;

    option = NONOPT;
    optarg = NULL;

    while ( optind < argc )
    {
        group = argv[optind];
        if ( *group != '-' )
        {
            option = NONOPT;
            optarg = group;
            optind++;
            break;
        }

        len = strlen (group);
        group = group + offset;
        if ( *group == '-' )
        {
            group++;
            offset += 2;
        }
        else
            offset++;

        option = *group;
        s = strchr ( optstring, option );
        if ( s != NULL )
        {
            s++;
            if ( *s == ':' )
            {
                optarg = group + 1;
                if ( *optarg == '\0' )
                    optarg = argv[++optind];

                if ( *optarg == '-' )
                {
                    fprintf ( stderr, "\n%s: illegal option -- %c", argv[0], option );
                    option = '?';
                    break;
                }
                else
                {
                    optind++;
                    offset = 0;
                    break;
                }
            }
            if ( offset >= len )
            {
                optind++;
                offset = 0;
            }
            break;
        }
        else
        {
            fprintf ( stderr, "\n%s: illegal option -- %c", argv[0], option );
            option = '?';
            break;
        }
    }
    return ( option );

}
