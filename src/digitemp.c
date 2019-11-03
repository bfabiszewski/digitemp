/* -----------------------------------------------------------------------
   DigiTemp
      
   Copyright 1996-2018 by Brian C. Lane <bcl@brianlane.com>
   All Rights Reserved

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
   
     digitemp -w                        Walk the LAN & show all
     digitemp -i			Initialize .digitemprc file
     digitemp -I                        Initialize .digitemprc w/sorted serial #s
     digitemp -s/dev/ttyS0		Set serial port (required)
     digitemp -cdigitemp.conf		Configuration File
     digitemp -r1000			Set Read timeout to 1000mS
     digitemp -l/var/log/temperature	Send output to logfile
     digitemp -v			Verbose mode
     digitemp -t0			Read Temperature
     digitemp -q                        Quiet, no copyright banner
     digitemp -a			Read all Temperatures
     digitemp -e                        Log all sensors to MySQL table
     digitemp -d5                       Delay between samples (in sec.)
     digitemp -n50                      Number of times to repeat. 0=forever
     digitemp -A                        Treat DS2438 as A/D converter
     digitemp -o1                       Output format for logfile
                                        See description below
     digitemp -o"output format string"  See description below
     digitemp -O"counter format"        See description below
     digitemp -H"Humidity format"       See description below

     Logfile formats:
     1 = (default) - 1 line per sensor, time, C, F
         1 line for each sample, elapsed time, sensor #1, #2, ... tab seperated
     2 = (seconds since start of measurement) & Reading in C
     3 = (seconds since start of measurement) & Reading in F
     4 = Unixtime(seconds since 1970-01-01 00:00:00) & Reading in C
     5 = Unixtime(seconds since 1970-01-01 00:00:00) & Reading in F

     The format string uses strftime tokens plus 6 special
     ones for digitemp - %s for sensor #, %C for centigrade,
     %F for fahrenheit, %R for hex serial number, %N for seconds since Epoch

     Humidity uses %h for the relative humidity in percent

     The counter format uses %n for the counter # and %C for the count 
     in decimal

     Remember the case of the token is important!

   =======================================================================
   See ChangeLog file for history of changes
   -----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#if !defined(AIX) && !defined(SOLARIS) && !defined(FREEBSD) && !defined(DARWIN)
#include <getopt.h>
#endif /* !AIX and !SOLARIS and !FREEBSD and !DARWIN */
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <strings.h>
#include <stdint.h>
#include "ad26.h"

#include "digitemp.h"
#include "device_name.h"
#include "ownet.h"
#include "owproto.h"


/* For tracking down strange errors */
#undef BCL_DEBUG

extern char 	*optarg;              
extern int	optind, opterr, optopt;

extern const char dtlib[];			/* Library Used            */
 
char serial_port[1024],				/* Path to the serial port */
     tmp_serial_port[1024],
     serial_dev[1024],				/* Device name without /dev/ */
     log_file[1024],                         /* Path to the log file    */
     tmp_log_file[1024],
     temp_format[80],                        /* Format for temperature readings	*/
     tmp_temp_format[80],
     counter_format[80],                     /* Format for counter readings		*/
     tmp_counter_format[80],
     humidity_format[80],			/* Format for Humidity readings		*/
     tmp_humidity_format[80],
     conf_file[1024],			/* Configuration File      */
     option_list[40],
     db_conf_file[1024],                     /* MySQL Configuration File*/
     dbuser[16],                             /* MySQL user name         */
     dbhost[16],                             /* MySQL hostname          */
     dbpass[16],                             /* MySQL password          */
     dbname[32],                             /* MySQL database name     */
     dbtable[32],                            /* MySQL database table    */
     dbcolumns[1024];                        /* MySQL database columns  */
int	read_time,				/* Pause during read	   */
	tmp_read_time,
	log_type,				/* output format type	   */
	tmp_log_type,
    num_cs = 0,                             /* Number of sensors on cplr */
	opts = 0;				/* Bitmask of flags	        */

//MySQL DBCOLUMN config file delimeter
char ckdelim[] = ",";
//For column keywords from MySQL config file
char *columnKeywords[7];

struct _coupler *coupler_top = NULL;		/* Linked list of couplers */

unsigned char Last2409[9];                      /* Last selected coupler   */


int	global_msec = 10;			/* For ReadCOM delay       */
int	global_msec_max = 15;

/* ----------------------------------------------------------------------- *
   Print out the program usage
 * ----------------------------------------------------------------------- */
void usage()
{
  printf(BANNER_1);
  printf(BANNER_2);
  printf(BANNER_3, dtlib );		         /* Report Library version */
  printf("\nUsage: digitemp [-s -i -I -U -l -r -v -t -a -d -n -o -c -e]\n");
  printf("                -i                            Initialize .digitemprc file\n");
  printf("                -I                            Initialize .digitemprc file w/sorted serial #s\n");
  printf("                -w                            Walk the full device tree\n");
  printf("                -s /dev/ttyS0                 Set serial port\n");
  printf("                -l /var/log/temperature       Send output to logfile\n");
  printf("                -c digitemp.conf              Configuration File\n");
  printf("                -r 1000                       Read delay in mS\n");
  printf("                -v                            Verbose output\n");
  printf("                -t 0                          Read Sensor #\n");
  printf("                -q                            No Copyright notice\n");
  printf("                -a                            Read all Sensors\n");
  printf("                -e                            Log all sensors to MySQL table\n");
  printf("                -d 5                          Delay between samples (in sec.)\n");
  printf("                -n 50                         Number of times to repeat\n");
  printf("                                              0=loop forever\n");
  printf("                -A                            Treat DS2438 as A/D converter\n");
  printf("                -O\"counter format string\"      See description below\n");
  printf("                -o 2                          Output format for logfile\n");
  printf("                -o\"output format string\"      See description below\n");
  printf("                -H\"Humidity format string\"    See description below\n");
  printf("\nLogfile formats:  1 = One line per sensor, time, C, F (default)\n");
  printf("                  2 = One line per sample, elapsed time, temperature in C\n");
  printf("                  3 = Same as #2, except temperature is in F\n");
  printf("                  4 = Same as #2, except elapsed time since (1970-01-01 00:00:00)\n");
  printf("                  5 = Same as #4, except temperature is in F\n");
  printf("        #2 and #3 have the data separated by tabs, suitable for import\n");
  printf("        into a spreadsheet or other graphing software.\n");
  printf("\n        The format string uses strftime tokens plus 5 special ones for\n");
  printf("        digitemp - %%s for sensor #, %%C for centigrade, %%F for fahrenheit,\n");
  printf("        %%R to output the hex serial number, and %%N for seconds since Epoch.\n");
  printf("        The case of the token is important! The default format string is:\n");
  printf("        \"%%b %%d %%H:%%M:%%S Sensor %%s C: %%.2C F: %%.2F\" which gives you an\n");
  printf("        output of: May 24 21:25:43 Sensor 0 C: 23.66 F: 74.59\n\n");
  printf("        The counter format string has 2 special specifiers:\n");
  printf("        %%n is the counter # and %%C is the count in decimal.\n");
  printf("        The humidity format uses %%h for the humidity in percent\n\n");
  printf("        The logfile may contain strftime pattern to format the filename\n");
}


/* ----------------------------------------------------------------------- *
   Free up all memory used by the coupler list
 * ----------------------------------------------------------------------- */
void free_coupler( int free_only )
{
  unsigned char   a[3];
  struct _coupler *c;
  
  c = coupler_top;
  while(c)
  {
     /* Turn off the Coupler */
     if ( !free_only )
       SetSwitch1F(0, c->SN, ALL_LINES_OFF, 0, a, TRUE);

    /* Free up the serial number lists */
    if( c->num_main > 0 )
      free( c->main );
      
    if( c->num_aux > 0 )
      free( c->aux );
      
    /* Point to the next in the list */
    coupler_top = c->next;
    
    /* Free up the current entry */
    free( c );
    
    c = coupler_top;
  } /* Coupler free loop */

  /* Make sure its null */
  coupler_top = NULL;
}


/* -----------------------------------------------------------------------
   Convert degrees C to degrees F
   ----------------------------------------------------------------------- */
float c2f( float temp )
{
  return 32 + ((temp*9)/5);
}


/* -----------------------------------------------------------------------
   Take the log_format string and parse out the
   digitemp tags (%*s %*C and %*F) including any format
   specifiers to pass to sprintf. Build a new string
   with the strftime tokens and the temperatures mixed
   together

   If humidity is <0 then it is invalid
   ----------------------------------------------------------------------- */
int build_tf( char *time_format, char *format, int sensor, 
              float temp_c, int humidity, unsigned char *sn )
{
  char	*tf_ptr,
  	*lf_ptr,
  	*tk_ptr,
  	token[80],
  	temp[80];
  	
  if( !time_format || !format )
    return 0;

  tf_ptr = time_format;
  lf_ptr = format;
  
  while( *lf_ptr )
  {
    if( *lf_ptr != '%' )
    {
      *tf_ptr++ = *lf_ptr++;
    } else {
      /* Found a token, decide if its one of ours... */
      /* save initial pointer, grab everything up to... */
      tk_ptr = token;

      /* 
         At this point it has a potential format specifier, copy it over
         to the token variable, up to the alpha-numeric specifier.
	 
	 It needs to stop copying after it gets the alpha character
      */
      while( isalnum( *lf_ptr ) || (*lf_ptr == '.') || (*lf_ptr == '*') 
             || (*lf_ptr == '%') )
      {
        *tk_ptr++ = *lf_ptr++;
        *tk_ptr = 0;
	
	/* Break out when the alpha character is copied over */
	if( isalpha( *(lf_ptr-1) ) )
	  break;
      }
      
      /* see if the format specifier is digitemp or strftime */
      switch( *(tk_ptr-1) )
      {
        case 's' :
        	/* Sensor number */
	        /* Change the specifier to a d */
	        *(tk_ptr-1) = 'd';
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, sensor );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;
        	break;
        	
        case 'h' :
        	/* Relative humidity % */
	        /* Change the specifier to a d */
	        *(tk_ptr-1) = 'd';
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, humidity );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;
        	break;
        	
        case 'F' :
        	/* Degrees Fahrenheit */
	        /* Change the specifier to a f */
	        *(tk_ptr-1) = 'f';
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, c2f(temp_c) );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;
        
        	break;
        	
        case 'C' :
        	/* Degrees Centigrade */
                /* Change the specifier to a f */
	        *(tk_ptr-1) = 'f';
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, temp_c );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;        	
        	break;
        	
        case 'R' :
        	/* ROM Serial Number */
                /* Change the specifier to a hex (x) */
	        *(tk_ptr-1) = 'X';

                /* Insert the serial number in HEX, yes its ugly,
                   but it works and saves using another temporary
                   location and variable.
                */
                sprintf( temp, "%02X%02X%02X%02X%02X%02X%02X%02X",
                         sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);
                
		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;
        	break;

        case 'N' :
        	/* Seconds since Epoch */
	        /* Change the specifier to a s and pass to time */
	        *(tk_ptr-1) = 's';

        	/* Intentional fall through */
        default:
		/* Not something for us, copy it into the time format */
        	tk_ptr = token;
        	while( *tk_ptr )
        	  *tf_ptr++ = *tk_ptr++;
        	break;
      } 
    }
  
  }

  /* Terminate the string */
  *tf_ptr = 0;


  return 1;
}


/* -----------------------------------------------------------------------
   Take the log_format string and parse out the
   digitemp tags (%*s %*C and %*F) including any format
   specifiers to pass to sprintf. Build a new string
   with the strftime tokens and the temperatures mixed
   together
   ----------------------------------------------------------------------- */
int build_cf( char *time_format, char *format, int sensor, int page,
              unsigned long count, unsigned char *sn )
{
  char	*tf_ptr,
  	*lf_ptr,
  	*tk_ptr,
  	token[80],
  	temp[80];
  	
  if( !time_format || !format )
    return 0;

  tf_ptr = time_format;
  lf_ptr = format;
  
  while( *lf_ptr )
  {
    if( *lf_ptr != '%' )
    {
      *tf_ptr++ = *lf_ptr++;
    } else {
      /* Found a token, decide if its one of ours... */
      /* save initial pointer, grab everything up to... */
      tk_ptr = token;
      
      /* Take numbers, astrix, period and letters */
      while( isalnum( *lf_ptr ) || (*lf_ptr == '.') ||
             (*lf_ptr == '*') || (*lf_ptr == '%') )
      {
        *tk_ptr++ = *lf_ptr++;
        *tk_ptr = 0;  
      }
      
      /* see if the format specifier is digitemp or strftime */
      switch( *(tk_ptr-1) )
      {
        case 's' :
        	/* Sensor number */
	        /* Change the specifier to a d */
	        *(tk_ptr-1) = 'd';
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, sensor );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;
        	break;
        	
        case 'F' :
        	break;

        case 'n' :
                /* Show the page/counter # (0 or 1) */
	        /* Change the specifier to a d */
	        *(tk_ptr-1) = 'd';
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, page );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;
                break;
        	
        case 'C' :
        	/* Counter reading, 32 bit value */
                /* Change the specifier to a ld */
	        *(tk_ptr-1) = 'l';
	        *(tk_ptr) = 'd';
	        *(tk_ptr+1) = 0;
	        
	        /* Pass it through sprintf */
	        sprintf( temp, token, count );

		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;        	
        	break;
        	
        case 'R' :
        	/* ROM Serial Number */
                /* Change the specifier to a hex (x) */
	        *(tk_ptr-1) = 'X';

                /* Insert the serial number in HEX, yes its ugly,
                   but it works and saves using another temporary
                   location and variable.
                */
                sprintf( temp, "%02X%02X%02X%02X%02X%02X%02X%02X",
                         sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);
                
		/* Insert this into the time format string */
		tk_ptr = temp;
		while( *tk_ptr )
		  *tf_ptr++ = *tk_ptr++;        	
        	break;

        case 'N' :
        	/* Seconds since Epoch */
	        /* Change the specifier to a s and pass to time */
	        *(tk_ptr-1) = 's';

        	/* Intentional fall through */
        default:
		/* Not something for us, copy it into the time format */
        	tk_ptr = token;
        	while( *tk_ptr )
        	  *tf_ptr++ = *tk_ptr++;
        	break;
      } 
    }
  }

  /* Terminate the string */
  *tf_ptr = 0;

  return 1;
}


/* -----------------------------------------------------------------------
   Print a string to the console or the logfile
   ----------------------------------------------------------------------- */
int log_string( char *line )
{
  int fd=0;
  char time_log_file[1024];

  if( log_file[0] != 0 )
  {
    time_t now = time(NULL);

    /* Update time_log_file name according to current time and logfile format */
    strftime(time_log_file, sizeof(log_file) - 1, log_file, gmtime(&now));

    if( (fd = open( time_log_file, O_CREAT | O_WRONLY | O_APPEND,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ) ) == -1 )
    {
      printf("Error opening logfile: %s\n", time_log_file );
      return -1;
    }
    if( write( fd, line, strlen( line ) ) == -1)
      perror("Error loging to logfile");
    close( fd );
  } else {
    printf( "%s", line );
    fflush( stdout );
  }
  return 0;
}  


/* -----------------------------------------------------------------------
   Log one line of text to the logfile with the current date and time

   Used with temperatures
   ----------------------------------------------------------------------- */
int log_temp( int sensor, float temp_c, unsigned char *sn, MYSQL *conn )
{
  char	temp[1024],
        time_format[160],
        dtf[160],
        tempd[1024];
        time_t mytime;

  mytime = time(NULL);
  if( mytime )
  {
    /* MySQL mods by Nick, more info at http://illx.org/digitemp */
    /* MySQL code has now been implemented on digitemp 3.5       */
    /* MySQL code can now handle custom table format             */
    if(log_type == 6) {
      char query[2048], snt[1024];

      sprintf(snt, "%02X%02X%02X%02X%02X%02X%02X%02X",
              sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);
      strcpy(dtf, "%Y-%m-%d %H:%M:%S");
      strftime( tempd, 1024, dtf, localtime( &mytime ) );

      /* build the mysql SQL query */
      if(strlen(dbcolumns) == 0) {
        if(strlen(dbtable) == 0)
          sprintf(query, "INSERT INTO temps VALUES('%d','%f','%f','%s','%s')",
                  sensor, temp_c, c2f(temp_c), snt, tempd);
        else
          sprintf(query, "INSERT INTO %s VALUES('%d','%f','%f','%s','%s')",
                  dbtable, sensor, temp_c, c2f(temp_c), snt, tempd);
      }
      else {
        char *token, *cp;
        char tstr[1024];

        cp = strdup(dbcolumns);
        token = strtok(cp, ckdelim);
        sprintf(query, "INSERT INTO %s VALUES(", dbtable);
        int wc = 0;
        while(token != NULL) {
          if(wc > 0)
            strcat(query, ",");

          if(!strcmp(token,"BLANK"))
            sprintf(tstr, "''");
          else if(!strcmp(token,"HUMI"))
            sprintf(tstr, "NULL");
          else if(!strcmp(token,"TEMPC"))
            sprintf(tstr, "'%f'", temp_c);
          else if(!strcmp(token,"TEMPF"))
            sprintf(tstr, "'%f'", c2f(temp_c));
          else if(!strcmp(token,"SERIAL"))
            sprintf(tstr, "'%s'", snt);
          else if(!strcmp(token,"TIMESTAMP"))
            sprintf(tstr, "'%s'", tempd);
          else if(!strcmp(token,"SENSOR"))
            sprintf(tstr, "'%d'", sensor);
          else
            sprintf(tstr, "'BAD TOKEN'");

          strcat(query, tstr);
          token = strtok(NULL, ckdelim);
          wc++;
        }
        strcat(query, ")");
        free(cp);
      }

      //printf("query string: '%s'\n", query);

      /* execute it */
      if(mysql_query (conn, query) != 0) {
        printf("Error!! MySQL INSERT failed!\n");
      }
      else {
        return 0;
      }
    }
    else {
      /* Build the time format string from log_format */
      build_tf( time_format, temp_format, sensor, temp_c, -1, sn );

      /* Handle the time format tokens */
      strftime( temp, 1024, time_format, localtime( &mytime ) );

      strcat( temp, "\n" );
    }
  } else {
    sprintf( temp, "Time Error\n" );
  }
  /* Log it to stdout, logfile or both */
  log_string( temp );

  return 0;
}


/* -----------------------------------------------------------------------
   Log one line of text to the logfile with the current date and time

   Used with counters
   ----------------------------------------------------------------------- */
int log_counter( int sensor, int page, unsigned long counter, unsigned char *sn )
{
  char	temp[1024],
  	time_format[160];
  time_t	mytime;


  mytime = time(NULL);
  if( mytime )
  {
    /* Build the time format string from counter_format */
    build_cf( time_format, counter_format, sensor, page, counter, sn );

    /* Handle the time format tokens */
    strftime( temp, 1024, time_format, localtime( &mytime ) );

    strcat( temp, "\n" );
  } else {
    sprintf( temp, "Time Error\n" );
  }
  /* Log it to stdout, logfile or both */
  log_string( temp );

  return 0;
}


/* -----------------------------------------------------------------------
   Log one line of text to the logfile with the current date and time

   Used with temperatures
   ----------------------------------------------------------------------- */
int log_humidity( int sensor, double temp_c, int humidity, unsigned char *sn, MYSQL *conn  )
{
  char	temp[1024],
  	time_format[160];
  time_t	mytime;
  char query[2048], snt[1024];
  char dtf[160];
  char *token, *cp;
  char tstr[1024];
  char tempd[1024];


  mytime = time(NULL);
  if( mytime )
  {
    /* Log the temperature */
    switch( log_type )
    {
      /* Multiple Centigrade temps per line */
      case 2:
      case 4:     sprintf( temp, "\t%3.2f", temp_c );
                  break;

      /* Multiple Fahrenheit temps per line */
      case 3:
      case 5:     sprintf( temp, "\t%3.2f", c2f(temp_c) );
                  break;

        /* MySQL mods by Nick, more info at http://illx.org/digitemp */
        /* MySQL code has now been implemented on digitemp 3.5       */
        /* MySQL code can now handle custom table format             */
      case 6:
        sprintf(snt, "%02X%02X%02X%02X%02X%02X%02X%02X",
                sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);
        strcpy(dtf, "%Y-%m-%d %H:%M:%S");
        strftime( tempd, 1024, dtf, localtime( &mytime ) );

        /* build the mysql SQL query */
        if(strlen(dbcolumns) == 0) {
          if(strlen(dbtable) == 0)
            sprintf(query, "INSERT INTO temps VALUES('%d','%f','%f','%s','%s')",
                    sensor, temp_c, c2f(temp_c), snt, tempd);
          else
            sprintf(query, "INSERT INTO %s VALUES('%d','%f','%f','%s','%s')",
                    dbtable, sensor, temp_c, c2f(temp_c), snt, tempd);
        }
        else {

          cp = strdup(dbcolumns);
          token = strtok(cp, ckdelim);
          sprintf(query, "INSERT INTO %s VALUES(", dbtable);
          int wc = 0;
          while(token != NULL) {
            if(wc > 0)
              strcat(query, ",");

            if(!strcmp(token,"BLANK"))
              sprintf(tstr, "''");
            else if(!strcmp(token,"TEMPC"))
              sprintf(tstr, "'%f'", temp_c);
            else if(!strcmp(token,"TEMPF"))
              sprintf(tstr, "'%f'", c2f(temp_c));
            else if(!strcmp(token,"HUMI"))
              sprintf(tstr, "'%d'", humidity);
            else if(!strcmp(token,"SERIAL"))
              sprintf(tstr, "'%s'", snt);
            else if(!strcmp(token,"TIMESTAMP"))
              sprintf(tstr, "'%s'", tempd);
            else if(!strcmp(token,"SENSOR"))
              sprintf(tstr, "'%d'", sensor);
            else
              sprintf(tstr, "'BAD TOKEN'");

            strcat(query, tstr);
            token = strtok(NULL, ckdelim);
            wc++;
          }
          strcat(query, ")");
          free(cp);
        }
        //printf("query string: '%s'\n", query);

        /* execute it */
        if(mysql_query (conn, query) != 0) {
          sprintf(temp, "Error!! MySQL INSERT failed!\n");
        }
        break;

      default:
                  /* Build the time format string from log_format */
                  build_tf( time_format, humidity_format, sensor, temp_c, humidity, sn );

                  /* Handle the time format tokens */
                  strftime( temp, 1024, time_format, localtime( &mytime ) );

                  strcat( temp, "\n" );
                  break;
    }
  } else {
    sprintf( temp, "Time Error\n" );
  }
  /* Log it to stdout, logfile or both */
  log_string( temp );

  return 0;
}


/* -----------------------------------------------------------------------
   Compare two serial numbers and return 1 of they match

   The second one has an additional byte indicating the main (0) or aux (1)
   branch.
   ----------------------------------------------------------------------- */
int cmpSN( unsigned char *sn1, unsigned char *sn2, int branch )
{
  int i;

  for(i = 0; i < 8; i++ )
  {
    if( sn1[i] != sn2[i] )
    {
      return 0;
    }
  }
  if( branch != sn2[8] )
  {
    return 0;
  }

  /* Everything Matches */
  return 1;
}  


/* -----------------------------------------------------------------------
   Show the verbose contents of the scratchpad
   ----------------------------------------------------------------------- */
void show_scratchpad( unsigned char *scratchpad, int sensor_family )
{
  char temp[80];
  int i;
  
  /* Log and/or print the scratchpad diagnostics */
  switch( sensor_family )
  {
    case DS1820_FAMILY:
      sprintf( temp, "  Temperature   : 0x%02X\n", scratchpad[1] );
      log_string( temp );
      sprintf( temp, "  Sign          : 0x%02X\n", scratchpad[2] );
      log_string( temp );
      sprintf( temp, "  TH            : 0x%02X\n", scratchpad[3] );
      log_string( temp );
      sprintf( temp, "  TL            : 0x%02X\n", scratchpad[4] );
      log_string( temp );
      sprintf( temp, "  Remain        : 0x%02X\n", scratchpad[7] );
      log_string( temp );
      sprintf( temp, "  Count Per C   : 0x%02X\n", scratchpad[8] );
      log_string( temp );
      sprintf( temp, "  CRC           : 0x%02X\n", scratchpad[9] );
      log_string( temp );
      break;
  
    case DS18B20_FAMILY:
    case DS1822_FAMILY:
    case DS28EA00_FAMILY:
      sprintf( temp, "  Temp. LSB     : 0x%02X\n", scratchpad[1] );
      log_string( temp );
      sprintf( temp, "  Temp. MSB     : 0x%02X\n", scratchpad[2] );
      log_string( temp );
      sprintf( temp, "  TH            : 0x%02X\n", scratchpad[3] );
      log_string( temp );
      sprintf( temp, "  TL            : 0x%02X\n", scratchpad[4] );
      log_string( temp );
      sprintf( temp, "  Config Reg.   : 0x%02X\n", scratchpad[5] );
      log_string( temp );
      sprintf( temp, "  CRC           : 0x%02X\n", scratchpad[9] );
      log_string( temp );
      break;
      
    case OWSLAVE_FAMILY:
      sprintf( temp, "  Hum. MSB     : 0x%02X\n", scratchpad[1] );
      log_string( temp );
      sprintf( temp, "  Hum. LSB     : 0x%02X\n", scratchpad[2] );
      log_string( temp );
      sprintf( temp, "  Hum. CRC     : 0x%02X\n", scratchpad[3] );
      log_string( temp );
      sprintf( temp, "  Temp. MSB    : 0x%02X\n", scratchpad[4] );
      log_string( temp );
      sprintf( temp, "  Temp. LSB    : 0x%02X\n", scratchpad[5] );
      log_string( temp );
      sprintf( temp, "  Temp. CRC    : 0x%02X\n", scratchpad[6] );
      log_string( temp );
      sprintf( temp, "  SHT type     : 0x%02X\n", scratchpad[7] );
      log_string( temp );
      break;

    case DS2422_FAMILY:
    case DS2423_FAMILY:
    
      break;  
  } /* sensor_family switch */
  
  /* Dump the complete contents of the scratchpad */
  for( i = 0; i < 10; i++ )
  {
    printf( "scratchpad[%d] = 0x%02X\n", i, scratchpad[i] );
  }
}

/* -----------------------------------------------------------------------
 Read the temperature and humidity from owslave

 Hum MSB      = scratchpad[1]
 Hum LSB      = scratchpad[2]
 Hum CRC      = scratchpad[3]
 Temp MSB     = scratchpad[4]
 Temp LSB     = scratchpad[5]
 Temp CRC     = scratchpad[6]
 SHT type     = scratchpad[7]

 ----------------------------------------------------------------------- */
int read_owslave( int sensor, MYSQL *conn )
{
  char    temp[1024];            /* For output string                    */
  unsigned char lastcrc8,
                scratchpad[30],  /* Scratchpad block from the sensor     */
                TempSN[8];
  int     j,
          try;                   /* Number of tries at reading device    */
  float   temp_c;                /* Calculated temperature in Centigrade */

  const float C1 = -2.0468;
  const float C2 = +0.0367;
  const float C3 = -0.0000015955;
  const float T1 = +0.01;
  const float T2 = +0.00008;
  float rh_lin;
  float rh_true;
  float rh, t;
  unsigned tcrc, hcrc;
  unsigned char sensor_type;

  for( try = 0; try < MAX_READ_TRIES; try++ )
  {
    if( owAccess(0) )
    {
      /* Convert humidity and temperature */
      if( !owWriteBytePower( 0, 0x44 ) )
      {
        return FALSE;
      }

      /* Sleep for conversion second */
      msDelay( read_time );

      /* Turn off the strong pullup */
      owLevel( 0, MODE_NORMAL );

      /* Now read the scratchpad from the device */
      if( owAccess(0) )
      {
        /* Build a block for the Scratchpad read */
        scratchpad[0] = 0xBE;
        for( j = 1; j < 8; j++ )
          scratchpad[j] = 0xFF;

        /* Send the block */
        if( owBlock( 0, FALSE, scratchpad, 8 ) )
        {
          /* Calculate the CRC 8 checksum on the received data */
          lastcrc8 = 0x00;
          sensor_type = scratchpad[7];
          if( sensor_type == 2 ){
            // sht2x - i2c
            hcrc = crc8_add( 0x0, scratchpad[1] );
            hcrc = crc8_add( hcrc, scratchpad[2] );
            tcrc = crc8_add( 0x0, scratchpad[4] );
            tcrc = crc8_add( tcrc, scratchpad[5] );
          } else {
            //sht1x sht7x
            hcrc = crc8_add( 0x0, 0x05 );
            hcrc = crc8_add( hcrc, scratchpad[1] );
            hcrc = crc8_add( hcrc, scratchpad[2] );
            hcrc = rev8bits( hcrc );
            tcrc = crc8_add( 0x0, 0x03 );
            tcrc = crc8_add( tcrc, scratchpad[4] );
            tcrc = crc8_add( tcrc, scratchpad[5] );
            tcrc = rev8bits( tcrc );
          }
          if( hcrc != scratchpad[3] )
          {
            lastcrc8 += 0x01;
            fprintf( stderr, "HUMI CRC Failed. CRC is 0x%02X instead of 0x%02X\n", scratchpad[3], hcrc );
          }
          if( tcrc != scratchpad[6] )
          {
            lastcrc8 += 0x01;
            fprintf( stderr, "TEMP CRC Failed. CRC is 0x%02X instead of 0x%02X\n", scratchpad[6], tcrc );
          }
          /* If the CRC8 is valid then calculate the temperature */
          if( lastcrc8 == 0x00 )
          {
            scratchpad[2] = scratchpad[2] & ~0x3;
            scratchpad[5] = scratchpad[5] & ~0x3;
            rh = (float)( ( scratchpad[1] << 8 ) | scratchpad[2] );
            t = (float)( ( scratchpad[4] << 8 ) | scratchpad[5] );

            if( sensor_type == 2 )
            {
              // sht2x i2c
              rh_true = 125 * rh / 65536 - 6;
              temp_c = 175.72 * t / 65536 - 46.85;
            } else {
              // sht1x sht7x
              temp_c = t * 0.01 - 40.1;
              rh_lin = C3 * rh * rh + C2 * rh + C1;
              rh_true = ( temp_c - 25 ) * (T1 + T2 * rh ) + rh_lin;
              if ( rh_true > 100 )
                rh_true = 100;
              if ( rh_true < 0.1 )
                rh_true = 0.1;
            }
            /* Log the temperature */
            switch( log_type )
            {
              /* Multiple Centigrade temps per line */
              case 2:
              case 4:     sprintf( temp, "\t%3.2f", temp_c );
                log_string( temp );
                break;

              /* Multiple Fahrenheit temps per line */
              case 3:
              case 5:     sprintf( temp, "\t%3.2f", c2f(temp_c) );
                log_string( temp );
                break;

              /* MySQL log_type */
              case 6:     owSerialNum( 0, &TempSN[0], TRUE );
                log_humidity( sensor, temp_c, rh_true, TempSN, conn );
                break;

              default:    owSerialNum( 0, &TempSN[0], TRUE );
                log_humidity( sensor, temp_c, rh_true, TempSN, 0 );
                break;
            } /* switch( log_type ) */

            /* Show the scratchpad if verbose is seelcted */
            if( opts & OPT_VERBOSE )
            {
              show_scratchpad( scratchpad, OWSLAVE_FAMILY );
            } /* if OPT_VERBOSE */

            /* Good conversion finished */
            return TRUE;
          } else {
            if ( try == MAX_READ_TRIES - 1 )
            {
              /* need to output something (0,-,NaN?) to keep columns consistent */
              switch( log_type )
              {
                 /* Multiple Centigrade temps per line */
                 case 2:
                 case 4:
                 /* Multiple Fahrenheit temps per line */
                 case 3:
                 case 5:     sprintf( temp, "\t%3.2f", (double) 0 );
                             log_string( temp );
                             break;

                 default:
                             break;
              } /* switch( log_type ) */
            } /* if tries == max_read_tries */

            if( opts & OPT_VERBOSE )
            {
              show_scratchpad( scratchpad, OWSLAVE_FAMILY );
            } /* if OPT_VERBOSE */
          } /* CRC 8 is OK */
        } /* Scratchpad Read */
      } /* owAccess failed */
    } /* owAccess failed */

    /* Failed to read, rest the network, delay and try again */
    owTouchReset(0);
    msDelay( read_time );
  } /* for try < 3 */

  /* Failed, no good reads after MAX_READ_TRIES */
  return FALSE;
}

/* -----------------------------------------------------------------------
   Read the temperature from one sensor

   Return the high-precision temperature value

   Calculated using formula from DS1820 datasheet

   Temperature   = scratchpad[1]
   Sign          = scratchpad[2]
   TH            = scratchpad[3]
   TL            = scratchpad[4]
   Count Remain  = scratchpad[7]
   Count Per C   = scratchpad[8]
   CRC           = scratchpad[9]
   
                   count_per_C - count_remain
   (temp - 0.25) * --------------------------
                       count_per_C

   If Sign is not 0x00 then it is a negative (Centigrade) number, and
   the temperature must be subtracted from 0x100 and multiplied by -1
   ----------------------------------------------------------------------- */
int read_temperature( int sensor_family, int sensor, MYSQL *conn )
{
  char    temp[1024];              /* For output string                    */
  unsigned char lastcrc8 = 0,
                scratchpad[30],    /* Scratchpad block from the sensor     */
                TempSN[8];
  int     j,
          try,                     /* Number of tries at reading device    */
          ds1820_try,              /* Allow ds1820 glitch 1 time           */
          ds18s20_try;             /* Allow DS18S20 error 1 time           */
  float   temp_c,                  /* Calculated temperature in Centigrade */
          hi_precision;

  ds1820_try = 0;
  ds18s20_try = 0;  
  temp_c = 0;
  
  for( try = 0; try < MAX_READ_TRIES; try++ )
  {
    if( owAccess(0) )
    {
      /* Convert Temperature */
      if( !owWriteBytePower( 0, 0x44 ) )
      {
        return FALSE;
      }

      /* Sleep for conversion second */
      msDelay( read_time );
      
      /* Turn off the strong pullup */
      owLevel( 0, MODE_NORMAL );

      /* Now read the scratchpad from the device */
      if( owAccess(0) )
      {
/* Use Read_Scratchpad instead? */
        /* Build a block for the Scratchpad read */
        scratchpad[0] = 0xBE;
        for( j = 1; j < 10; j++ )
          scratchpad[j] = 0xFF;

        /* Send the block */
        if( owBlock( 0, FALSE, scratchpad, 10 ) )
        {
          /* Calculate the CRC 8 checksum on the received data */
          setcrc8(0, 0);
          for( j = 1; j < 10; j++ )
            lastcrc8 = docrc8( 0, scratchpad[j] );

          /* If the CRC8 is valid then calculate the temperature */
          if( lastcrc8 == 0x00 )
          {
            /* DS1822 and DS18B20 use a different calculation */
            if( (sensor_family == DS18B20_FAMILY) ||
                (sensor_family == DS1822_FAMILY) ||
                (sensor_family == DS28EA00_FAMILY) ||
                (sensor_family == DS1923_FAMILY) )
            {
              short int temp2 = (scratchpad[2] << 8) | scratchpad[1];
              temp_c = temp2 / 16.0;
            }

            /* Handle the DS1820 and DS18S20 */
            if( sensor_family == DS1820_FAMILY )
            {
              /* Check for DS1820 glitch condition */
              /* COUNT_PER_C - COUNT_REMAIN == 1 */
              if( ds1820_try == 0 )
              {
                if( (scratchpad[7] - scratchpad[6]) == 1 )
                {
                  ds1820_try = 1;
                  continue;
                } /* DS1820 error */
              } /* ds1820_try */
            
              /* Check for DS18S20 Error condition */
              /*  LSB = 0xAA
                  MSB = 0x00
                  COUNT_REMAIN = 0x0C
                  COUNT_PER_C = 0x10
              */
              if( ds18s20_try == 0 )
              {
                if( (scratchpad[4]==0xAA) &&
                    (scratchpad[3]==0x00) &&
                    (scratchpad[7]==0x0C) &&
                    (scratchpad[8]==0x10)
                  )
                {
                  ds18s20_try = 1;
                  continue;
                } /* DS18S20 error condition */
              } /* ds18s20_try */
          
              /* Convert data to temperature */
              if( scratchpad[2] == 0 )
              {
                temp_c = (int) scratchpad[1] >> 1;
              } else {
                temp_c = -1 * (int) (0x100-scratchpad[1]) >> 1;
              } /* Negative temp calculation */
              temp_c -= 0.25;
              hi_precision = (int) scratchpad[8] - (int) scratchpad[7];
              hi_precision = hi_precision / (int) scratchpad[8];
              temp_c = temp_c + hi_precision;
            } /* DS1820_FAMILY */
            
            /* Log the temperature */
            switch( log_type )
            {
              /* Multiple Centigrade temps per line */
              case 2:
              case 4:     sprintf( temp, "\t%3.2f", temp_c );
                          log_string( temp );
                          break;

              /* Multiple Fahrenheit temps per line */
              case 3:
              case 5:     sprintf( temp, "\t%3.2f", c2f(temp_c) );
                          log_string( temp );
                          break;
                
              /* MySQL log_type */
              case 6:     owSerialNum( 0, &TempSN[0], TRUE );
                          log_temp( sensor, temp_c, TempSN, conn );
                          break;

              default:    owSerialNum( 0, &TempSN[0], TRUE );
                          log_temp( sensor, temp_c, TempSN, 0 );
                          break;
            } /* switch( log_type ) */

            /* Show the scratchpad if verbose is seelcted */
            if( opts & OPT_VERBOSE )
            {
              show_scratchpad( scratchpad, sensor_family );              
            } /* if OPT_VERBOSE */

            /* Good conversion finished */
            return TRUE;
          } else {
            fprintf( stderr, "CRC Failed. CRC is %02X instead of 0x00\n", lastcrc8 );

            if (try == MAX_READ_TRIES - 1)
            {
              /* need to output something (0,-,NaN?) to keep columns consistent */
              switch( log_type )
              {
            	/* Multiple Centigrade temps per line */
                 case 2:
                 case 4:
                 /* Multiple Fahrenheit temps per line */
                 case 3:
                 case 5:     sprintf( temp, "\t%3.2f", (double) 0 );
                             log_string( temp );
                             break;
             
                 default:
                             break;
               } /* switch( log_type ) */
            } /* if tries == max_read_tries */

            if( opts & OPT_VERBOSE )
            {
              show_scratchpad( scratchpad, sensor_family );              
            } /* if OPT_VERBOSE */
          } /* CRC 8 is OK */
        } /* Scratchpad Read */
      } /* owAccess failed */
    } /* owAccess failed */
    
    /* Failed to read, rest the network, delay and try again */
    owTouchReset(0);
    msDelay( read_time );
  } /* for try < 3 */
  
  /* Failed, no good reads after MAX_READ_TRIES */
  return FALSE;
}


/* -----------------------------------------------------------------------
   Read the current counter values
   ----------------------------------------------------------------------- */
int read_counter( int sensor_family, int sensor )
{
  char          temp[1024];        /* For output string                    */
  unsigned char TempSN[8];
  int           page;
  unsigned long counter_value;
  
  if( sensor_family == DS2422_FAMILY )
  {
    /* Read Pages 2, 3 */
    for( page=2; page<=3; page++ )
    {
      if( ReadCounter( 0, page, &counter_value ) )
      {
        /* Log the counter */
        switch( log_type )
        {
          /* Multiple Centigrade temps per line */
          case 2:
          case 3:
          case 4:
          case 5:     sprintf( temp, "\t%ld", counter_value );
                      log_string( temp );
                      break;

          default:    owSerialNum( 0, &TempSN[0], TRUE );
                      log_counter( sensor, page-2, counter_value, TempSN );
                      break;
        } /* switch( log_type ) */
      }
    }
  } else if( sensor_family == DS2423_FAMILY ) {
    /* Read Pages 14, 15 */
    for( page=14; page<=15; page++ )
    {
      if( ReadCounter( 0, page, &counter_value ) )
      {
        /* Log the counter */
        switch( log_type )
        {
          /* Multiple Centigrade temps per line */
          case 2:
          case 3:
          case 4:
          case 5:     sprintf( temp, "\t%ld", counter_value );
                      log_string( temp );
                      break;

          default:    owSerialNum( 0, &TempSN[0], TRUE );
                      log_counter( sensor, page-14, counter_value, TempSN );
                      break;
        } /* switch( log_type ) */
      }
    }
  }

  return FALSE;
}


/* -----------------------------------------------------------------------
   Read the DS2406
   General Purpose PIO
	by Tomasz R. Surmacz (tsurmacz@ict.pwr.wroc.pl)
   !!!! Not finished !!!!
   Needs an output format string system. Hard-coded for the moment.
   ----------------------------------------------------------------------- */
int read_ds2406( int sensor_family, int sensor )
{
  int		pio;
  char		temp[1024],
  		    time_format[160];
  time_t	mytime;

  
  if( sensor_family == DS2406_FAMILY )
  {
    /* Read Vdd */
    pio = PIO_Reading(0, 0);

    if (pio==-1) {
	printf(" PIO DS2406 sensor %d CRC failed\n", sensor);
	return FALSE;
    }
    mytime = time(NULL);
    if( mytime )
    {
      /* Log the temperature */
      switch( log_type )
      {
        /* Multiple Centigrade temps per line */
        case 2:
        case 4:     sprintf( temp, "\t%02x,%02x", pio>>8, pio&0xff );
                    break;

        /* Multiple Fahrenheit temps per line */
        case 3:
        case 5:     sprintf( temp, "\t%02x,%02x", pio>>8, pio&0xff);
                    break;

        default:
                    sprintf( time_format, "%%b %%d %%H:%%M:%%S Sensor %d PIO: %02x,%02x, PIO-A: %s%s", sensor, pio>>8, pio&0xff,
			((pio&0x1000)!=0)? // Port A latch: there was a change
				(((pio&0x0400)!=0)?
					"ON"	// and the current state is ON
					:"on")
				:"off",	// the current state is off, no change
			((pio&0x4000)!=0)? // we have 2 ports if bit is 1
				( ((pio&0x2000)!=0)?
					(((pio&0x0800)!=0)? // the latch says 1
						" PIO-B: ON" // and state too
						:" PIO-B: on")
					:" PIO-B: off") // the latch said no
				:
				"")
			;
                    /* Handle the time format tokens */
                    strftime( temp, 1024, time_format, localtime( &mytime ) );
                    strcat( temp, "\n" );
                    break;
      } /* switch( log_type ) */
    } else {
      sprintf( temp, "Time Error\n" );
    }

    /* Log it to stdout, logfile or both */
    log_string( temp );
  }

  return TRUE;
}


/* -----------------------------------------------------------------------
   Read the DS2438
   General Purpose A/D
   VDD
   Temperature
   ...

   !!!! Not finished !!!!
   Needs an output format string system. Hard-coded for the moment.
   ----------------------------------------------------------------------- */
int read_ds2438( int sensor_family, int sensor )
{
  double	temp_c = -999.0;
  float		vdd = -1.0,
  		ad = -1.0;
  char		temp[1024],
  		time_format[160];
  time_t	mytime;
  int           cad = 0;
  int           try;
  int           result = FALSE;

  for( try = 0; try < MAX_READ_TRIES; try++ )
  {
    /* Read the temperature */
    temp_c = Get_Temperature(0);
    if (temp_c == -999.0)
        return result;

    /* Read Vdd, the supply voltage */
    if( (vdd = Volt_Reading(0, 1, &cad)) != -1.0 )
    {
      /* Read A/D reading from the humidity sensor */
      if( (ad = Volt_Reading(0, 0, NULL)) != -1.0 )
      {
        break;
      }
    }

    owTouchReset(0);
    msDelay(read_time);
  }

  /* Log the temperature */
  mytime = time(NULL);
  switch( log_type )
  {
      /* Multiple Centigrade temps per line */
      case 2:     sprintf(temp, "\t%3.2f", temp_c);
                  break;

      /* Multiple Fahrenheit temps per line */
      case 3:     sprintf(temp, "\t%3.2f", c2f(temp_c));
                  break;

      default:
                  sprintf(time_format, "%%b %%d %%H:%%M:%%S Sensor %d VDD: %0.2f AD: %0.2f CAD: %d C: %0.2f", sensor, vdd, ad, cad, temp_c);
                  /* Handle the time format tokens */
                  strftime(temp, 1024, time_format, localtime(&mytime));
                  strcat(temp, "\n");
                  break;
  } /* switch( log_type ) */

  /* Log it to stdout, logfile or both */
  log_string(temp);

  return TRUE;
}


/* -----------------------------------------------------------------------
   (This routine is modified from code by Eric Wilde)

   Read the humidity from one sensor (e.g. the AAG TAI8540x).
 
   Log the temperature value and relative humidity.
 
   Calculated using formula cribbed from the Dallas source code (gethumd.c),
   DS2438 data sheet and HIH-3610 data sheet.
 
   Sensors like the TAI8540x use a DS2438 battery monitor to sense temperature
   and convert humidity readings from a Honeywell HIH-3610.  The DS2438
   scratchpad is:
 
   Status/config = scratchpad[2]
   Temp LSB      = scratchpad[3]
   Temp MSB      = scratchpad[4]
   Voltage LSB   = scratchpad[5]
   Voltage MSB   = scratchpad[6]
   CRC           = scratchpad[10]
 
                            Temp LSB
   temp = (Temp MSB * 32) + -------- * 0.03125
                                8
 
   The temperature is a two's complement signed number.
 
   voltage = ((Voltage MSB * 256) + Voltage LSB) / 100
 
   There are two voltages that must be read to get an accurate humidity
   reading.  The supply voltage (VDD) is read to determine what voltage the
   humidity sensor is running at (this affects the zero offset and slope of
   the humidity curve).  The sensor voltage (VAD) is read to get the humidity
   value.  Here is the formula for the humidity (temperature and voltage
   compensated):
    
              ((VAD/VDD) - 0.16) * 161.29
   humidity = ---------------------------
               1.0546 - (0.00216 * temp)
 
   The humidity sensor is linear from approx 10% to 100% R.H.  Accuracy is
   approx 2%.

   !!!! Not Finished !!!!
   ----------------------------------------------------------------------- */
int read_humidity( int sensor_family, int sensor )
{
  double	temp_c = -999.0;	/* Converted temperature in degrees C */
  float		sup_voltage,		/* Supply voltage in volts            */
		hum_voltage,		/* Humidity sensor voltage in volts   */
		humidity = 0.0;		/* Calculated humidity in %RH         */
  unsigned char	TempSN[8];
  int		try;  
  int           result = FALSE;
	
  for( try = 0; try < MAX_READ_TRIES; try++ )
  {
    /* Read the temperature */
    temp_c = Get_Temperature(0);
    if (temp_c == -999.0)
        return result;

    /* Read Vdd, the supply voltage */
    if( (sup_voltage = Volt_Reading(0, 1, NULL)) != -1.0 )
    {
      /* Read A/D reading from the humidity sensor */
      if( (hum_voltage = Volt_Reading(0, 0, NULL)) != -1.0 )
      {
        /* Convert the measured voltage to humidity */
        humidity = (((hum_voltage/sup_voltage) - 0.16) * 161.29)
                      / (1.0546 - (0.00216 * temp_c));
        if( humidity > 100.0 )
          humidity = 100.0;
        else if( humidity < 0.0 )
          humidity = 0.0;

        result = TRUE;
        break;
      }
    }

    owTouchReset(0);
    msDelay(read_time);
  }

  /* Log the temperature and humidity */
  owSerialNum( 0, &TempSN[0], TRUE );
  log_humidity( sensor, temp_c, humidity, TempSN, 0 );

  return result;
}


/* -----------------------------------------------------------------------
   Read the DS1923 Hygrochton Temperature/Humidity Logger
   ----------------------------------------------------------------------- */
int read_temperature_DS1923( int sensor_family, int sensor )
{
  unsigned char TempSN[8],
  		block2[2];
  int try;                     /* Number of tries at reading device    */
  int b;
  int pre_t;
  float temp_c;
  int ival;
  float adval;
  float humidity;

  for( try = 0; try < MAX_READ_TRIES; try++ )
  {
    if( owAccess(0) )
    {
      /* Force Conversion */
      if( !owWriteByte( 0, 0x55 ) || !owWriteByte( 0, 0x55 ))
      {
        return FALSE;
      }
      /* TODO CRC checking and read the addresses 020Ch to 020Fh (results)i
       * and the Device Sample Counter at address 0223h to 0225h. 
       * If the count has incremented, the command was executed successfully.
       */

      /* Sleep for conversion (spec says it takes max 666ms */
      /* Q. Is it possible to poll? */
      msDelay( 666 );
      
      /* Now read the memory 0x20C:0x020F */
      if( owAccess(0) )
      {
        if( !owWriteByte( 0, 0x69 ) )
        {
          return FALSE;
        }

        /* "Latest Temp" in the memory */
        block2[0] = 0x0c;
        block2[1] = 0x02;
 
        /* Send the block */
        if( owBlock( 0, FALSE, block2, 2 ) )
        {
          if (block2[0] != 0x0c && block2[1] != 0x02) 
            return FALSE;

          /* Send dummy password */
          for(b = 0; b < 8; ++b) {
            owWriteByte(0, 0x04);
          }

          /* Read the temperature */
          block2[0] = owReadByte(0);
          block2[1] = owReadByte(0);
          pre_t  = (block2[1]/2)-41;
          temp_c = 1.0f * pre_t + block2[0]/512.0f;

          /* Read the humidity */
          block2[0] = owReadByte(0);
          block2[1] = owReadByte(0);
          ival = (block2[1]*256 + block2[0])/16;
          adval = 1.0f * ival * 5.02f/4096;
          humidity = (adval-0.958f) / 0.0307f;

          /* Log the temperature and humidity */
	  /* TUTAJ masz wartosci we floatach dla Thermochrona
 	     sensor to nr sensora z pliku konfiguracyjnego,
 	     a tempsn to pewnie id urzadzenia 1wire
          */
          owSerialNum( 0, &TempSN[0], TRUE );
          log_humidity( sensor, temp_c, humidity, TempSN, 0 );

          /* Good conversion finished */
          return TRUE;
        } /* Scratchpad Read */
      } /* owAccess failed */
    } /* owAccess failed */

    /* Failed to read, rest the network, delay and try again */
    owTouchReset(0);
    msDelay( read_time );
  } /* for try < 3 */
  
  /* Failed, no good reads after MAX_READ_TRIES */
  return FALSE;
}


/* -----------------------------------------------------------------------
   Select the indicated device, turning on any required couplers
   ----------------------------------------------------------------------- */
int read_device( struct _roms *sensor_list, int sensor, MYSQL *conn )
{
  unsigned char   TempSN[8],
                  a[3];
  int             s,
                  status = 0,
                  sensor_family;
  struct _coupler *c_ptr;          /* Coupler linked list                  */
    
  /* Tell the sensor to do a temperature conversion */

  /* Sort out how to address the sensor.
     If sensor < num_sensors then it can be directly addressed
     if sensor >= num_sensors then the coupler must first be
     addressed and the correct branch turned on.
  */
  if( sensor < sensor_list->max )
  {
    /* Address the sensor directly */
    owSerialNum( 0, &sensor_list->roms[sensor*8], FALSE );
  } else {
    /* Step through the coupler list until the right sensor is found.
       Sensors are in order.
    */
    s = sensor - sensor_list->max;
    c_ptr = coupler_top;
    while( c_ptr )
    {
      if( s < c_ptr->num_main )
      {
        /* Found the right area */
        
        /* Is this coupler & branch already on? */
        if( !cmpSN( c_ptr->SN, Last2409, 0 ) )
        {
          /* Turn on the main branch */
          if(!SetSwitch1F(0, c_ptr->SN, DIRECT_MAIN_ON, 0, a, TRUE))
          {
            printf("Setting Switch to Main ON state failed\n");
            return FALSE;
          }
          /* Remember the last selected coupler & Branch */
          memcpy( &Last2409, &c_ptr->SN, 8 );
          Last2409[8] = 0;
        }
        
        /* Select the sensor */
        owSerialNum( 0, &c_ptr->main[s*8], FALSE );
        break;
      } else {
        s -= c_ptr->num_main;
        if( s < c_ptr->num_aux )
        {
          /* Found the right area */

          /* Is this coupler & branch already on? */
          if( !cmpSN( c_ptr->SN, Last2409, 1 ) )
          {        
            /* Turn on the aux branch */
            if(!SetSwitch1F(0, c_ptr->SN, AUXILARY_ON, 2, a, TRUE))
            {
              printf("Setting Switch to Aux ON state failed\n");
              return FALSE;
            }
            /* Remember the last selected coupler & Branch */
            memcpy( &Last2409, &c_ptr->SN, 8 );
            Last2409[8] = 1;
          } /* Last2409 check */

          /* Select the sensor */
          owSerialNum( 0, &c_ptr->aux[s*8], FALSE );
          break;          
        }
      }
      s -= c_ptr->num_aux;
      c_ptr = c_ptr->next;
    }    
  }

  /* Get the Serial # selected */
  owSerialNum( 0, &TempSN[0], TRUE );
  sensor_family = TempSN[0];
  
  switch( sensor_family )
  {
    case DS28EA00_FAMILY:
    case DS2413_FAMILY:
      if( (opts & OPT_DS2438) || (sensor_family==DS2413_FAMILY) ) { // read PIO
		status = read_pio_ds28ea00( sensor_family, sensor );
	    break;
	  }
  	  // else - drop through to DS1822
    case DS1820_FAMILY:
    case DS1822_FAMILY:
    case DS18B20_FAMILY:
      status = read_temperature( sensor_family, sensor, conn ); // also for DS28EA00
      break;

    case OWSLAVE_FAMILY:
      status = read_owslave( sensor, conn );
      break;

    case DS1923_FAMILY:
      status = read_temperature_DS1923( sensor_family, sensor );
      break;      

    case DS2422_FAMILY:
    case DS2423_FAMILY:
      status = read_counter( sensor_family, sensor );
      break;

    case DS2438_FAMILY:
    // What type is it?
        {
            int page;
            for( page=3; page<8; page++)
            {
                get_ibl_type( 0, page, 0);
            }
        }
        if( opts & OPT_DS2438 )
        {
            status = read_ds2438( sensor_family, sensor );
        } else {
            status = read_humidity( sensor_family, sensor );
        }
        break;
    }

  return status;
}




/* -----------------------------------------------------------------------
   Read the temperaturess for all the connected sensors

   Step through all the sensors in the list of serial numbers
   ----------------------------------------------------------------------- */
int read_all( struct _roms *sensor_list )
{
  int x;
  
  for( x = 0; x <  (num_cs+sensor_list->max); x++ )
  {
    read_device( sensor_list, x, 0 );
  }
  
  return 0;
}

/* This routine reads all the sensors and logs to MySQL */
int read_all_and_dblog( struct _roms *sensor_list, MYSQL *conn )
{
  int x;
 
  for( x = 0; x <  (num_cs+sensor_list->max); x++ )
    {
      /*printf();*/
      read_device( sensor_list, x, conn );
    }
  
  return 0;
}

/* -----------------------------------------------------------------------
   Read a .digitemprc file from the current directory

   The rc file contains:
   
   TTY <serial>
   LOG <logfilepath>
   READ_TIME <time in mS>
   LOG_TYPE <from -o>
   LOG_FORMAT <format string for temperature logging and printing>
   CNT_FORMAT <format string for counter logging and printing>
   SENSORS <number of ROM lines>
   Multiple ROM x <serial number in bytes> lines

   v 2.3 additions:
   Multiple COUPLER x <serial number in decimal> lines
   CROM x <COUPLER #> <M or A> <Serial number in decimal>
   
   ----------------------------------------------------------------------- */
int read_rcfile( char *fname, struct _roms *sensor_list )
{
  FILE	*fp;
  char	temp[1024];
  char	*ptr;
  int	sensors, x;
  struct _coupler *c_ptr, *coupler_end;

  num_cs = 0;
  coupler_end = coupler_top;
    
  if( ( fp = fopen( fname, "r" ) ) == NULL )
  {
    /* No rcfile to read, could be part of an -i so don't die */
    return 1;
  }
  
  while( fgets( temp, sizeof(temp), fp ) != 0 )
  {
    if( (temp[0] == '\n') || (temp[0] == '#') )
      continue;
      
    ptr = strtok( temp, " \t\n" );
    
    if( strncasecmp( "TTY", ptr, 3 ) == 0 )
    {
      ptr = strtok( NULL, " \t\n" );
      strncpy( serial_port, ptr, sizeof(serial_port)-1 );
      serial_port[sizeof(serial_port)-1] = 0x00;
    } else if( strncasecmp( "LOG_TYPE", ptr, 8 ) == 0 ) {
      ptr = strtok( NULL, " \t\n");
      log_type = atoi( ptr );
    } else if( strncasecmp( "LOG_FORMAT", ptr, 10 ) == 0 ) {
      ptr = strtok( NULL, "\"\n");
      strncpy( temp_format, ptr, sizeof(temp_format)-1 );
      temp_format[sizeof(temp_format)-1] = 0x00;
    } else if( strncasecmp( "CNT_FORMAT", ptr, 10 ) == 0 ) {
      ptr = strtok( NULL, "\"\n");
      strncpy( counter_format, ptr, sizeof(counter_format)-1 );
      counter_format[sizeof(counter_format)-1] = 0x00;
    } else if( strncasecmp( "HUM_FORMAT", ptr, 10 ) == 0 ) {
      ptr = strtok( NULL, "\"\n");
      strncpy( humidity_format, ptr, sizeof(humidity_format)-1 );
      humidity_format[sizeof(humidity_format)-1] = 0x00;
    } else if( strncasecmp( "LOG", ptr, 3 ) == 0 ) {
      ptr = strtok( NULL, " \t\n" );
      strncpy( log_file, ptr, sizeof(log_file)-1 );
      log_file[sizeof(log_file)-1] = 0x00;
    } else if( strncasecmp( "FAIL_TIME", ptr, 9 ) == 0 ) {

    } else if( strncasecmp( "READ_TIME", ptr, 9 ) == 0 ) {
      ptr = strtok( NULL, " \t\n");
      read_time = atoi( ptr );
    } else if( strncasecmp( "SENSORS", ptr, 7 ) == 0 && sensor_list->roms == NULL ) {
      ptr = strtok( NULL, " \t\n" );
      sensors = atoi( ptr );
      
      if( sensors > 0 )
      {
        /* Reserve some memory for the list */
        if( ( sensor_list->roms = malloc( sensors * 8 ) ) == NULL )
        {
          fprintf( stderr, "Error reserving memory for %d sensors\n", sensors );
          fclose( fp );
          return -1;
        }
        sensor_list->max = sensors; 
      }
    } else if( strncasecmp( "ROM", ptr, 3 ) == 0 && sensor_list->roms != NULL ) {
      /* Main LAN sensors */
      ptr = strtok( NULL, " \t\n" );
      sensors = atoi( ptr );
      if ( sensors >= sensor_list->max ) {
          fprintf( stderr, "Error, too many ROM entries. Check SENSORS value.\n");
          fclose( fp );
          return -1;
      }

      /* Read the 8 byte ROM address */
      for( x = 0; x < 8; x++ )
      {
        ptr = strtok( NULL, " \t\n" );
        sensor_list->roms[(sensors * 8) + x] = strtol( ptr, (char **)NULL, 0 );
      }
    } else if( strncasecmp( "COUPLER", ptr, 7 ) == 0 ) {
      /* DS2409 Coupler list, they are ALWAYS in order, so ignore the
         coupler # and create the list in the order found
       */
      
      /* Allocate space for this coupler */
      /* Create a new entry in the coupler linked list */
      if( (c_ptr = malloc( sizeof( struct _coupler ) ) ) == NULL )
      {
	      fprintf( stderr, "Failed to allocate %d bytes for coupler linked list\n", (int) sizeof( struct _coupler ) );
          free_coupler(0);
#ifndef OWUSB
	      owRelease(0);
#else
          owRelease(0, temp );
          fprintf( stderr, "USB ERROR: %s\n", temp );
#endif /* OWUSB */
	      exit(EXIT_ERR);
      }

      c_ptr->next = NULL;
      c_ptr->num_main = 0;
      c_ptr->num_aux = 0;
      c_ptr->main = NULL;
      c_ptr->aux = NULL;

      if( coupler_top == NULL )
      {
	/* First coupler, add it to the top of the list */
	coupler_top = c_ptr;
	coupler_end = c_ptr;
      } else {
        /* Add the new coupler to the list, point to new end */
        coupler_end->next = c_ptr;
	coupler_end = c_ptr;
      }

      /* Ignore the coupler # */
      strtok( NULL, " \t\n" );
      
      /* Read the 8 byte ROM address */
      for( x = 0; x < 8; x++ )
      {
        ptr = strtok( NULL, " \t\n" );
        c_ptr->SN[x] = strtol( ptr, (char **)NULL, 0);
      }
    } else if( strncasecmp( "CROM", ptr, 4 ) == 0 ) {
      /* Count the number of coupler connected sensors */
      num_cs++;

      /* DS2409 Coupler sensors */    
      /* Ignore sensor #, they are all created in order */
      strtok( NULL, " \t\n" );

      /* Get the coupler number, and set the pointer to the right
         coupler
       */
      ptr = strtok( NULL, " \t\n" );
      x = atoi(ptr);
      c_ptr = coupler_top;
      while( c_ptr && (x > 0) )
      {
        c_ptr = c_ptr->next;
	x--;
      }
	
      /* Make sure we are pointing to something */
      if( c_ptr )
      {
        /* Main/Aux branch */
        ptr = strtok( NULL, " \t\n" );
	
	if( *ptr == 'M' )
	{
	  /* Add to the main list */
	  c_ptr->num_main++;
	  
          /* Allocate enough space for the new serial number */
          if( (c_ptr->main = realloc( c_ptr->main, c_ptr->num_main * 8 ) ) == NULL )
          {
            fprintf( stderr, "Failed to allocate %d bytes for main branch\n", c_ptr->num_main * 8 );
            free_coupler(0);
#ifndef OWUSB
            owRelease(0);
#else
            owRelease(0, temp );
            fprintf( stderr, "USB ERROR: %s\n", temp );
#endif /* OWUSB */
            exit(EXIT_ERR);
          }
	  
	  /* Add the serial number to the list */
	  for( x = 0; x < 8; x++ )
	  {
            ptr = strtok( NULL, " \t\n" );
            c_ptr->main[((c_ptr->num_main-1)*8)+x] = strtol( ptr, (char **)NULL,0);
          }	  
	} else {
	  /* Add to the aux list */
	  c_ptr->num_aux++;
	  
          /* Allocate enough space for the new serial number */
          if( (c_ptr->aux = realloc( c_ptr->aux, c_ptr->num_aux * 8 ) ) == NULL )
          {
            fprintf( stderr, "Failed to allocate %d bytes for aux branch\n", c_ptr->num_aux * 8 );
            free_coupler(0);
#ifndef OWUSB
            owRelease(0);
#else
            owRelease(0, temp );
            fprintf( stderr, "USB ERROR: %s\n", temp );
#endif /* OWUSB */
            exit(EXIT_ERR);
          } /* Allocate more aux space */
	  
	  /* Add the serial number to the list */
	  for( x = 0; x < 8; x++ )
	  {
            ptr = strtok( NULL, " \t\n" );
            c_ptr->aux[((c_ptr->num_aux-1)*8)+x] = strtol( ptr, (char **)NULL, 0 );
          } /* aux serial number loop */
	} /* Main/Aux branch check */
      } /* c_ptr Pointing somewhere check */
    } else {
      fprintf( stderr, "Error reading rcfile: %s\n", fname );
      fclose( fp );
      return -1;
    }
  }
  
  fclose( fp ); 

  return 0;
}

/* -----------------------------------------------------------------------                                        
   Read a .digitemprc_mysql file from the current directory
   The rc file contains:
 
   DBNAME database_name
   DBUSER database_user_name
   DBPASS database_password
   DBHOST localhost (or specified host)
   DBTABLE temps (or specified table )
   DBCOLUMNS localhost (or default column format which is (SENSOR, TEMPC, TEMPF, SERIAL, TIMESTAMP))

   DBCOLUMNS must be formatted with the following keywords in any order delimited by commas:
   SENSOR
   TEMPC
   TEMPF
   HUMI
   SERIAL
   TIMESTAMP

   Also, DBCOLUMNS may have an optional BLANK which would serve as a blank coulmn INSERT, an example could be:
   SENSOR,BLANK,TEMPC,TIMESTAMP,BLANK,SERIAL
*/
int read_rcdbfile( char *fname )
{
  FILE  *fp;
  char  temp[80];
  char  *ptr;

  num_cs = 0;
 
  if( ( fp = fopen( fname, "r" ) ) == NULL ) {
    /* No rcfile to read, could be part of an -i so don't die */
    return 1;
  }
  
  while( fgets( temp, 80, fp ) != 0 ) {
    if( (temp[0] == '\n') || (temp[0] == '#') )
      continue;
    
    ptr = strtok( temp, " \t\n" );
    if( strncasecmp( "DBUSER", ptr, 6 ) == 0 ) {
      ptr = strtok( NULL, " \t\n" );
      strcpy( dbuser, ptr );
    } 
    else if( strncasecmp( "DBPASS", ptr, 6 ) == 0 ) {
      ptr = strtok( NULL, " \t\n");
      strcpy( dbpass, ptr );
    } 
    else if( strncasecmp( "DBHOST", ptr, 6 ) == 0 ) {
      ptr = strtok( NULL, " \t\n");
      strcpy( dbhost, ptr );
    } 
    else if( strncasecmp( "DBNAME", ptr, 6 ) == 0 ) {
      ptr = strtok( NULL, "\"\n");
      strcpy( dbname, ptr );
    } 
    else if( strncasecmp( "DBTABLE", ptr, 6 ) == 0 ) {
      ptr = strtok( NULL, "\"\n");
      strcpy( dbtable, ptr );
    } 
    else if( strncasecmp( "DBCOLUMNS", ptr, 6 ) == 0 ) {
      ptr = strtok( NULL, "\"\n");
      strcpy( dbcolumns, ptr );
    } 
    else {
      fprintf( stderr, "Error reading %s file\n", fname );
      printf("Error reading %s file\n", fname );
      fclose( fp );
      return -1;
    }
  }
  fclose( fp );

  //validate the columns
  if(strlen(dbcolumns) > 0) {
    char *token, *cp;

    cp = strdup(dbcolumns);
    token = strtok(cp, ckdelim);
    int c=0;
    while(token != NULL) {
      c++;
      //printf("dbcolumn token validation '%s'\n", token);
      int bc = 1;
      int i;
      for(i=0;i<7;i++)
	if(!strcmp(columnKeywords[i], token))
	  bc--;

      if(bc) {
	printf("/etc/digitemprc_mysql, DBCOLUMNS: Bad column keyword '%s' found at column %i\n", token, c);
	exit(0);
      }
      token = strtok(NULL, ckdelim);
    }
    free(cp);
  }

  return 0;
}

/* -----------------------------------------------------------------------
   Write a .digitemprc file, it contains:
   
   TTY <serial>
   LOG <logfilepath>
   READ_TIME <time in mS>
   LOG_TYPE <from -o>
   LOG_FORMAT <format string for temperature logging and printing>
   CNT_FORMAT <format string for counter logging and printing>
   SENSORS <number of ROM lines>
   Multiple ROM x <serial number in bytes> lines

   v 2.3 additions:
   Multiple COUPLER x <serial number in decimal> lines
   CROM x <COUPLER #> <M or A> <Serial number in decimal>

   v 2.4 additions:
   All serial numbers are now in Hex.  Still can read older decimal
     format. 
   Added 'ALIAS # <string>'  
   ----------------------------------------------------------------------- */
int write_rcfile( char *fname, struct _roms *sensor_list )
{
  FILE	*fp;
  int	x, y, i;
  struct _coupler *c_ptr;

  if( ( fp = fopen( fname, "wb" ) ) == NULL )
  {
    return -1;
  }
  
  fprintf( fp, "TTY %s\n", serial_port );
  if( log_file[0] != 0 )
    fprintf( fp, "LOG %s\n", log_file );

  fprintf( fp, "READ_TIME %d\n", read_time );		/* mSeconds	*/

  fprintf( fp, "LOG_TYPE %d\n", log_type );
  fprintf( fp, "LOG_FORMAT \"%s\"\n", temp_format );
  fprintf( fp, "CNT_FORMAT \"%s\"\n", counter_format );
  fprintf( fp, "HUM_FORMAT \"%s\"\n", humidity_format );
    
  fprintf( fp, "SENSORS %d\n", sensor_list->max );

  for( x = 0; x < sensor_list->max; x++ )
  {
    fprintf( fp, "ROM %d ", x );
    
    for( y = 0; y < 8; y++ )
    {
	  fprintf( fp, "0x%02X ", sensor_list->roms[(x * 8) + y] );
    }
    fprintf( fp, "\n" );
  }

  /* If any DS2409 Couplers were found, write out their information too */
  /* Write out the couplers first */
  c_ptr = coupler_top;
  x =  0;
  while( c_ptr )
  {
    fprintf( fp, "COUPLER %d ", x );
    for( y = 0; y < 8; y++ )
    {
      fprintf( fp, "0x%02X ", c_ptr->SN[y] );
    }
    fprintf( fp, "\n" );
    x++;
    c_ptr = c_ptr->next;
  } /* Coupler list */

  /* Sendor # ID for coupler starts at num_sensors */
  num_cs = 0;  

  /* Start at the top of the coupler list */  
  c_ptr = coupler_top;
  x =  0;
  while( c_ptr )
  {
    /* Print the devices on this coupler's main branch */
    if( c_ptr->num_main > 0 )
    {
      for( i = 0; i < c_ptr->num_main; i++ )
      {
        fprintf( fp, "CROM %d %d M ", sensor_list->max+num_cs++, x );

        for( y = 0; y < 8; y++ )
        {
          fprintf( fp, "0x%02X ", c_ptr->main[(i * 8) + y] );
        }
        fprintf( fp, "\n" );
      }
    }
    
    /* Print the devices on this coupler's aux branch */
    if( c_ptr->num_aux > 0 )
    {
      for( i = 0; i < c_ptr->num_aux; i++ )
      {
        fprintf( fp, "CROM %d %d A ", sensor_list->max+num_cs++, x );

        for( y = 0; y < 8; y++ )
        {
          fprintf( fp, "0x%02X ", c_ptr->aux[(i * 8) + y] );
        }
        fprintf( fp, "\n" );
      }
    }

    x++;
    c_ptr = c_ptr->next;
  } /* Coupler list */
  

  fclose( fp );
  if( !(opts & OPT_QUIET) )
  {
    fprintf( stderr, "Wrote %s\n",fname);
  }
  return 0;
}


/* -----------------------------------------------------------------------
   Print out a serial number
   ----------------------------------------------------------------------- */      
void printSN( unsigned char *TempSN, int crlf )
{
  int y;

  /* Print the serial number */    
  for( y = 0; y < 8; y++ )
  {
    printf("%02X", TempSN[y] );
  }
  if( crlf )
    printf("\n");
}


/* -----------------------------------------------------------------------
   Walk the entire connected 1-wire LAN and display the serial number
   and type of device.
   ----------------------------------------------------------------------- */      
int Walk1Wire()
{
  unsigned char TempSN[8],
                InfoByte[3];
  short result;
  struct _roms  coupler_list;           /* Attached Roms                */
  int   x;

  bzero( &coupler_list, sizeof( struct _roms ) );
    
  /* Find any DS2409 Couplers and turn them all off.
     This WILL NOT WORK if there is a coupler attached to the
     bus of another coupler. DigiTemp on;y supports couplers
     on the main 1-Wire LAN.

     We also don't record any couplers in this loop because if
     one was one and we detected a branch that is closed off
     after it is turned off we will end up with multiple copies
     of the same couplers.
  */
  if( !(opts & OPT_QUIET) )
  {
    printf("Turning off all DS2409 Couplers\n");
  }
  result = owFirst( 0, TRUE, FALSE );
  while(result)
  {
    owSerialNum( 0, TempSN, TRUE );
    if( !(opts & OPT_QUIET) )
    {
      printf(".");
    }
    fflush(stdout);
    if( TempSN[0] == SWITCH_FAMILY )
    {
      /* Turn off the Coupler */
      if(!SetSwitch1F(0, TempSN, ALL_LINES_OFF, 0, InfoByte, TRUE))
      {
        fprintf( stderr, "Setting Coupler to OFF state failed\n");

        if( coupler_list.roms != NULL )
          free( coupler_list.roms );

        return -1;
      }
    }
    result = owNext( 0, TRUE, FALSE );
  } /* HUB search */
  if( !(opts &OPT_QUIET) )
  {
    printf("\n");
  }
  
  /* Now we know all the couplers on the main LAN are off, we
     can now start mapping the 1-Wire LAN
   */
  if( !(opts & OPT_QUIET) )
  {
    printf("Devices on the Main LAN\n");
  }
  result = owFirst( 0, TRUE, FALSE );
  while(result)
  {
    owSerialNum( 0, TempSN, TRUE );
    /* Print the serial number */    
    printSN( TempSN, 0 );
    printf(" : %s\n", device_name( TempSN[0]) );

    if( TempSN[0] == SWITCH_FAMILY )
    {
      /* Save the Coupler's serial number so we can explore it later */
      /* Count the sensors detected */
      coupler_list.max++;

      /* Allocate enough space for the new serial number */
      if( (coupler_list.roms = realloc( coupler_list.roms, coupler_list.max * 8 ) ) == NULL )
      {
        fprintf( stderr,"Failed to allocate %d bytes for coupler_list\n", coupler_list.max * 8 );

        if( coupler_list.roms != NULL )
          free( coupler_list.roms );

        return -1;
      }
      owSerialNum( 0, &coupler_list.roms[(coupler_list.max-1)*8], TRUE );
        
      /* Turn off the Coupler */
      if(!SetSwitch1F(0, TempSN, ALL_LINES_OFF, 0, InfoByte, TRUE))
      {
        fprintf(stderr, "Setting Switch to OFF state failed\n");

        if( coupler_list.roms != NULL )
          free( coupler_list.roms );

        return -1;
      }
    }
    result = owNext( 0, TRUE, FALSE );
  } /* HUB search */
  if( !(opts & OPT_QUIET) )
  {
    printf("\n");
  }

  /* If there were any 2409 Couplers present walk their trees too */
  if( coupler_list.max > 0 )
  {
    for(x = 0; x < coupler_list.max; x++ )
    {
      if( !(opts & OPT_QUIET) )
      {
        printf("\nDevices on Main Branch of Coupler : ");
        printSN( &coupler_list.roms[x*8], 1 );
      }
      result = owBranchFirst( 0, &coupler_list.roms[x * 8], FALSE, TRUE );
      while(result)
      {
        owSerialNum( 0, TempSN, TRUE );

        /* Print the serial number */    
        printSN( TempSN, 0 );
        printf(" : %s\n", device_name( TempSN[0]) );

        result = owBranchNext(0, &coupler_list.roms[x * 8], FALSE, TRUE );
      } /* Main branch loop */
      
      if( !(opts & OPT_QUIET) )
      {
        printf("\n");
        printf("Devices on Aux Branch of Coupler : ");
        printSN( &coupler_list.roms[x*8], 1 );
      }
      result = owBranchFirst( 0, &coupler_list.roms[x * 8], FALSE, FALSE );
      while(result)
      {
        owSerialNum( 0, TempSN, TRUE );

        /* Print the serial number */    
        printSN( TempSN, 0 );
        printf(" : %s\n", device_name( TempSN[0]) );

        result = owBranchNext(0, &coupler_list.roms[x * 8], FALSE, FALSE );
      } /* Aux Branch loop */
    }  /* Coupler loop */
  } /* num_couplers check */
    
  if( coupler_list.roms != NULL )
    free( coupler_list.roms );

  return 0;
}



/* -----------------------------------------------------------------------
   Compare 2 serial numbers (8 bytes)
   
   Return:
     -1 if the 2nd is < 1st
     0 if equal
     1 if the 2nd is > 1st
   ----------------------------------------------------------------------- */
int sercmp( unsigned char *sn1, unsigned char *sn2 )
{
    int i;
    
    for (i=0; i<8; i++)
    {
        if (sn2[i] < sn1[i])
            return -1;
        if (sn2[i] > sn1[i])
            return 1;
    }
    
    return 0;
}


/* -----------------------------------------------------------------------
   Find all the supported temperature sensors on the bus, searching down
   DS2409 hubs on the main bus (but not on other hubs).
   ----------------------------------------------------------------------- */
int Init1WireLan( struct _roms *sensor_list )
{
  unsigned char TempSN[8],
                InfoByte[3];
  int result,
      x;
  unsigned int found_sensors = 0;
  struct _coupler       *c_ptr,         /* Coupler pointer              */
                        *coupler_end;   /* end of the list              */

  /* Free up anything that was read from .digitemprc */
  if( sensor_list->roms != NULL )
  {
    free( sensor_list->roms );
    sensor_list->roms = NULL;
  }
  sensor_list->max = 0;
  num_cs = 0;

  /* Free up the coupler list */
  free_coupler(0);

  /* Initialize the coupler pointer */
  coupler_end = coupler_top;

  if( !(opts & OPT_QUIET) )
  {
    printf("Turning off all DS2409 Couplers\n");
  }
  result = owFirst( 0, TRUE, FALSE );
  while(result)
  {
    owSerialNum( 0, TempSN, TRUE );
    if( !(opts & OPT_QUIET) )
    {
      printf(".");
    }
    fflush(stdout);
    if( TempSN[0] == SWITCH_FAMILY )
    {
      /* Turn off the Coupler */
      if(!SetSwitch1F(0, TempSN, ALL_LINES_OFF, 0, InfoByte, TRUE))
      {
        fprintf( stderr, "Setting Coupler to OFF state failed\n");
        free_coupler(0);
        return -1;
      }
    }
    result = owNext( 0, TRUE, FALSE );
  } /* HUB OFF search */ 
  if( !(opts & OPT_QUIET) )
  {
    printf("\n");
  }
  
  if( !(opts & OPT_QUIET) )
  {
    printf("Searching the 1-Wire LAN\n");
  }
  /* Find any DS2409 Couplers and turn them all off */
  result = owFirst( 0, TRUE, FALSE );
  while(result)
  {
    owSerialNum( 0, TempSN, TRUE );

    if( TempSN[0] == SWITCH_FAMILY )
    {
      /* Print the serial number */
      printSN( TempSN, 0 );
      printf(" : %s\n", device_name( TempSN[0]) );

      /* Save the Coupler's serial number */
      /* Create a new entry in the coupler linked list */
      if( (c_ptr = malloc( sizeof( struct _coupler ) ) ) == NULL )
      {
        fprintf( stderr, "Failed to allocate %d bytes for coupler linked list\n", (int) sizeof( struct _coupler ) );
        free_coupler(0);
        return -1;
      }

      /* Write the serial number to the new list entry */
      owSerialNum( 0, c_ptr->SN, TRUE );

      c_ptr->next = NULL;
      c_ptr->num_main = 0;
      c_ptr->num_aux = 0;
      c_ptr->main = NULL;
      c_ptr->aux = NULL;
        
      if( coupler_top == NULL )
      {
        /* First coupler, add it to the top of the list */
        coupler_top = c_ptr;
        coupler_end = c_ptr;
      } else {
        /* Add the new coupler to the end of the list, point to new end */
        coupler_end->next = c_ptr;
        coupler_end = c_ptr;        
      }
        
    } else if( (TempSN[0] == DS1820_FAMILY) ||
               (TempSN[0] == DS1822_FAMILY) ||
               (TempSN[0] == DS28EA00_FAMILY) ||
               (TempSN[0] == DS18B20_FAMILY) ||
               (TempSN[0] == DS1923_FAMILY) ||
               (TempSN[0] == DS2406_FAMILY) ||
               (TempSN[0] == DS2413_FAMILY) ||
               (TempSN[0] == DS2422_FAMILY) ||
               (TempSN[0] == DS2423_FAMILY) ||
               (TempSN[0] == OWSLAVE_FAMILY) ||
               (TempSN[0] == DS2438_FAMILY)
             )
    {
      /* Print the serial number */
      printSN( TempSN, 0 );
      printf(" : %s\n", device_name( TempSN[0]) );

      found_sensors = 1;
      /* Count the sensors detected */
      sensor_list->max++;

      /* Allocate enough space for the new serial number */
      if( (sensor_list->roms = realloc( sensor_list->roms, sensor_list->max * 8 ) ) == NULL )
      {
        fprintf( stderr, "Failed to allocate %d bytes for sensor_list\n", sensor_list->max * 8 );
        if( sensor_list->roms )
        {
          if( sensor_list->roms )
          {
            free( sensor_list->roms );
            sensor_list->roms = NULL;
          }
        }
        return -1;
      }
      owSerialNum( 0, &sensor_list->roms[(sensor_list->max-1)*8], TRUE );
    }
    result = owNext( 0, TRUE, FALSE );
  }    

  /* Now go through each coupler branch and search there */
  c_ptr = coupler_top;
  while( c_ptr )
  {
    /* Search the Main branch */
    result = owBranchFirst( 0, c_ptr->SN, FALSE, TRUE );
    while(result)
    {
      owSerialNum( 0, TempSN, TRUE );

      /* Check to see if it is a temperature sensor or a PIO device */
      if( (TempSN[0] == DS1820_FAMILY) ||
          (TempSN[0] == DS1822_FAMILY) ||
          (TempSN[0] == DS28EA00_FAMILY) ||
          (TempSN[0] == DS18B20_FAMILY)||
          (TempSN[0] == DS1923_FAMILY) ||
          (TempSN[0] == DS2406_FAMILY) ||
          (TempSN[0] == DS2413_FAMILY) ||
          (TempSN[0] == DS2422_FAMILY) ||
          (TempSN[0] == DS2423_FAMILY) ||
          (TempSN[0] == OWSLAVE_FAMILY) ||
          (TempSN[0] == DS2438_FAMILY)
	)
      {
        /* Print the serial number */
        printSN( TempSN, 0 );
        printf(" : %s\n", device_name( TempSN[0]) );

        found_sensors = 1;
        /* Count the number of sensors on the main branch */
        c_ptr->num_main++;
                
        /* Allocate enough space for the new serial number */
        if( (c_ptr->main = realloc( c_ptr->main, c_ptr->num_main * 8 ) ) == NULL )
        {
          fprintf( stderr, "Failed to allocate %d bytes for main branch\n", c_ptr->num_main * 8 );
          free_coupler(0);
          if( sensor_list->roms )
          {
            free( sensor_list->roms );
            sensor_list->roms = NULL;
          }
          return -1;
        }
        owSerialNum( 0, &c_ptr->main[(c_ptr->num_main-1)*8], TRUE );
      } /* Add serial number to list */
        
      /* Find the next device on this branch */
      result = owBranchNext(0, c_ptr->SN, FALSE, TRUE );
    } /* Main branch loop */
      
    /* Search the Aux branch */
    result = owBranchFirst( 0, c_ptr->SN, FALSE, FALSE );
    while(result)
    {
      owSerialNum( 0, TempSN, TRUE );

      if( (TempSN[0] == DS1820_FAMILY) ||
          (TempSN[0] == DS1822_FAMILY) ||
          (TempSN[0] == DS28EA00_FAMILY) ||
          (TempSN[0] == DS18B20_FAMILY)||
          (TempSN[0] == DS1923_FAMILY) ||
          (TempSN[0] == DS2406_FAMILY) ||
          (TempSN[0] == DS2413_FAMILY) ||
          (TempSN[0] == DS2422_FAMILY) ||
          (TempSN[0] == DS2423_FAMILY) ||
          (TempSN[0] == DS2438_FAMILY)
	)
      {
        /* Print the serial number */
        printSN( TempSN, 0 );
        printf(" : %s\n", device_name( TempSN[0]) );

        found_sensors = 1;
        /* Count the number of sensors on the aux branch */
        c_ptr->num_aux++;
        
        /* Allocate enough space for the new serial number */
        if( (c_ptr->aux = realloc( c_ptr->aux, c_ptr->num_aux * 8 ) ) == NULL )
        {
          fprintf( stderr, "Failed to allocate %d bytes for aux branch\n", c_ptr->num_main * 8 );
          free_coupler(0);
          if( sensor_list->roms )
          {
            free( sensor_list->roms );
            sensor_list->roms = NULL;
          }
          return -1;
        }
        owSerialNum( 0, &c_ptr->aux[(c_ptr->num_aux-1)*8], TRUE );
      } /* Add serial number to list */
        
      /* Find the next device on this branch */
      result = owBranchNext(0, c_ptr->SN, FALSE, FALSE );
    } /* Aux branch loop */
      
    c_ptr = c_ptr->next;
  }  /* Coupler loop */


  /*
     Did the search find any sensors? Even if there was an error it may
     have found some valid sensors
  */ 
  if( found_sensors )
  {
    /* Was anything found on the main branch? */
    if( sensor_list->max > 0 )
    {
      for( x = 0; x < sensor_list->max; x++ )
      {
        printf("ROM #%d : ", x );
        printSN( &sensor_list->roms[x*8], 1 );
      }
    } /* num_sensors check */
      
    /* Was anything found on any DS2409 couplers? */
    c_ptr = coupler_top;
    while( c_ptr )      
    {
      /* Check the main branch */
      if( c_ptr->num_main > 0 )
      {
        for( x = 0; x < c_ptr->num_main; x++ )
        {    
          printf("ROM #%d : ", sensor_list->max+num_cs++ );
          printSN( &c_ptr->main[x*8], 1 );
        }
      }
      
      /* Check the aux branch */
      if( c_ptr->num_aux > 0 )
      {
        for( x = 0; x < c_ptr->num_aux; x++ )
        {    
          printf("ROM #%d : ", sensor_list->max+num_cs++ );
          printSN( &c_ptr->aux[x*8], 1 );
        }
      }
        
      /* Next Coupler */
      c_ptr = c_ptr->next;
    } /* Coupler list loop */

    /* Write the new list of sensors to the current directory */
    write_rcfile( conf_file, sensor_list );
  }
  return 0;
}


/* ----------------------------------------------------------------------- *
   Check to see if the file actually exists
 * ----------------------------------------------------------------------- */
int file_exists (char * fileName)
{
   struct stat buf;

   if (stat( fileName, &buf ) == 0)
   {
       /* file found */
       return 1;
   }
   return 0;
}


/* ----------------------------------------------------------------------- *
   DigiTemp main routine
   
   Parse command line options, run functions
 * ----------------------------------------------------------------------- */
int main( int argc, char *argv[] )
{
  int		sensor;			/* Single sensor index to read  */
  char		temp[1024];		/* Temporary strings            */
  int		c;
  int		sample_delay = 0;	/* Delay between samples (SEC)	*/
  unsigned int	x,
  		num_samples = 1;	/* Number of samples 		*/
  time_t	last_time,		/* Last time we started samples */
		start_time;		/* Starting time		*/
  long int	elapsed_time;		/* Elapsed from start		*/
  struct _roms  sensor_list;            /* Attached Roms                */

  /* valid MySQL column keywords */
  char *ck1 = "SENSOR", *ck2 = "TEMPC", *ck3 = "TEMPF", *ck4 = "SERIAL", *ck5 = "TIMESTAMP", *ck6 = "BLANK", *ck7 = "HUMI";
  columnKeywords[0] = ck1; columnKeywords[1] = ck2; columnKeywords[2] = ck3; 
  columnKeywords[3] = ck4; columnKeywords[4] = ck5; columnKeywords[5] = ck6; 
  columnKeywords[6] = ck7;


  /* Make sure the structure is erased */
  bzero( &sensor_list, sizeof( struct _roms ) );
 

  if( argc <= 1 )
  {
    fprintf(stderr,"Error! Not enough arguments.\n\n");
    usage();
    return -1;
  }

#ifndef OWUSB
  serial_port[0] = 0;			/* No default port		*/
#else
  strcpy( serial_port, "USB" );
#endif
  tmp_serial_port[0] = 0;
  log_file[0] = 0;			/* No default log file		*/
  tmp_log_file[0] = 0;
  tmp_counter_format[0] = 0;
  tmp_temp_format[0] = 0;
  tmp_humidity_format[0] = 0;
  read_time = 1000;			/* 1000mS read delay		*/
  tmp_read_time = -1;
  sensor = 0;				/* First sensor	in list		*/
  log_type = 1;			        /* Normal DigiTemp logfile	*/
  tmp_log_type = -1;
  sample_delay = 0;			/* No delay			*/
  num_samples = 1;			/* Only do it once by default	*/

  /* Default log format string:                 */
  /* May 24 21:25:43 Sensor 0 C: 23.66 F: 74.59 */
  strcpy( temp_format, "%b %d %H:%M:%S Sensor %s C: %.2C F: %.2F" );
  strcpy( counter_format, "%b %d %H:%M:%S Sensor %s #%n %C" );
  strcpy( humidity_format, "%b %d %H:%M:%S Sensor %s C: %.2C F: %.2F H: %h%%" );
  strcpy( conf_file, "/etc/digitemprc" );
  strcpy( option_list, "?hqiaAvwr:f:s:l:t:d:n:o:c:O:H:e" );
  strcpy( db_conf_file, "/etc/digitemprc_mysql" );

  /* Command line options override any .digitemprc options temporarily	*/
  /* Unless the -i parameter is specified, then changes are saved to    */
  /* .digitemprc file                                                   */

  opterr = 1;

  while( (c = getopt(argc, argv, option_list)) != -1 )
  {
    /* Process the command line arguments */
    switch( c )
    {
      case 'c': if( optarg )			/* Configuration file	*/
		{
		  strncpy( conf_file, optarg, sizeof( conf_file ) - 1 );
                  conf_file[sizeof(conf_file)-1] = 0x00;
		}
		break;

      case 'T': opts |= OPT_TEST;
	       break;

      case 'w': opts |= OPT_WALK;               /* Walk the LAN         */
                break;
    
      case 'i':	opts |= OPT_INIT;		/* Initialize the s#'s	*/
      		break;
      		
      case 'r':	tmp_read_time = atoi(optarg);	/* Read delay in mS	*/
      		break;
      		
      case 'v': opts |= OPT_VERBOSE;		/* Verbose		*/
      		break;

      case 's': if(optarg)			/* Serial port		*/
      		{
      		  strncpy( tmp_serial_port, optarg, sizeof(tmp_serial_port) - 1 );
                  tmp_serial_port[sizeof(tmp_serial_port) - 1] = 0x00;
      		}
      		break;
      		
      case 'l': if(optarg)			/* Log Filename		*/
      		{
      		  strncpy( tmp_log_file, optarg, sizeof( tmp_log_file ) - 1);
                  tmp_log_file[sizeof( tmp_log_file ) - 1] = 0x00;
      		}
      		break;
      		
      case 't':	if(optarg)			/* Single Sensor #	*/
      		{
      		  sensor = atoi(optarg);
      		  opts |= OPT_SINGLE;
      		}
      		break;

      case 'a': opts |= OPT_ALL;		/* Read All sensors	*/
		break;

      case 'd': if(optarg)			/* Sample Delay		*/
		{
		  sample_delay = atoi(optarg);
		}
		break;

      case 'n': if(optarg)			/* Number of samples 	*/
		{
		  num_samples = atoi(optarg);
		}
		break;

      case 'A': opts |= OPT_DS2438;		/* Treat DS2438 as A/D converter */
      		break;

      case 'o': if(optarg)			/* Temperature Logfile format	*/
		{
		  if( isdigit( optarg[0] ) )
		  {
		    /* Its a number, get it */
		    tmp_log_type = atoi(optarg);
		  } else {
		    /* Not a nuber, get the string */
                    if( strlen( optarg ) > sizeof(tmp_temp_format)-1 ) {
                      printf("Temperature format string too long! > %d\n", (int) sizeof(tmp_temp_format)-1);
                    } else {
                      strncpy( tmp_temp_format, optarg, sizeof(tmp_temp_format)-1 );
                      tmp_temp_format[sizeof(tmp_temp_format)-1] = 0x00;
                    }
		    tmp_log_type=0;
		  }
		}
		break;
		
      case 'O': if(optarg)			/* Counter Logfile format	*/
		{
                  if( strlen( optarg ) > sizeof(tmp_counter_format)-1 ) {
                    printf("Counter format string too long! > %d\n", (int) sizeof(tmp_counter_format)-1);
                  } else {
                    strncpy( tmp_counter_format, optarg, sizeof(tmp_counter_format)-1 );
                    tmp_counter_format[sizeof(tmp_counter_format)-1] = 0x00;
                  }
		}
		break;
		
      case 'H': if(optarg)			/* Humidity Logfile format	*/
		{
                  if( strlen( optarg ) > sizeof(tmp_humidity_format)-1 ) {
                    printf("Humidity format string too long! > %d\n", (int) sizeof(tmp_humidity_format)-1);
                  } else {
                    strncpy( tmp_humidity_format, optarg, sizeof(tmp_humidity_format)-1 );
                    tmp_humidity_format[sizeof(tmp_humidity_format)-1] = 0x00;
                  }
		}
		break;
      case 'e': opts |= OPT_DBLOG;              /* Log to MySQL */
	        break;     		

      case 'q': opts |= OPT_QUIET;
      		break;

      case ':':
      case 'h':
      case '?': usage();
      		exit(EXIT_HELP);
      		break;
    
      default:	break;
    } /* switch getopt */
  }  /* while getopt */

  /* Require one 1 action command, no more, no less. */
  if ((opts & (OPT_WALK|OPT_INIT|OPT_SINGLE|OPT_ALL|OPT_DBLOG)) == 0 )
  {
    fprintf( stderr, "Error!  You need 1 of the following action commands, -w -a -i -t -e\n");
    exit(EXIT_HELP);
  }

  if ( read_rcfile( conf_file, &sensor_list ) < 0 ) {
    exit(EXIT_NORC);
  }

  if ( read_rcdbfile( db_conf_file ) < 0 ) {
    exit(EXIT_NORC);
  }

  /* Now we go through and override with values from the command line */
  if (tmp_read_time > 0) {
	read_time = tmp_read_time;
  }
  
  if (tmp_serial_port[0] != 0) {
	strncpy( serial_port, tmp_serial_port, sizeof(serial_port)-1 );
        serial_port[sizeof(serial_port)-1] = 0x00;
  }
  
  if (tmp_log_file[0] != 0) {
	strncpy( log_file, tmp_log_file, sizeof(log_file)-1 );
        log_file[sizeof(log_file)-1] = 0x00;
  }
  
  if (tmp_log_type != -1) {
    log_type = tmp_log_type;
    if ( tmp_log_type == 0 )
    {
      strncpy( temp_format, tmp_temp_format, sizeof(temp_format)-1 );
      temp_format[sizeof(temp_format)-1] = 0x00;
    }
  }

  if( tmp_counter_format[0] != 0 ) {
    strncpy( counter_format, tmp_counter_format, sizeof(counter_format)-1 );
    counter_format[sizeof(counter_format)-1] = 0x00;
  }
  
  if( tmp_humidity_format[0] != 0 ) {
    strncpy( humidity_format, tmp_humidity_format, sizeof(humidity_format)-1 );
    humidity_format[sizeof(humidity_format)-1] = 0x00;
  }
  
  /* Show the copyright banner? */
  if( !(opts & OPT_QUIET) )
  {
    printf(BANNER_1);
    printf(BANNER_2);
  }

#ifndef OWUSB
  /* Check to see if the device file actually exists */
  if( !file_exists( serial_port ) )
  {
    fprintf( stderr, "Error, serial port '%s' does not exist!\n", serial_port );

    if( sensor_list.roms != NULL )
      free( sensor_list.roms );

    if( coupler_top != NULL )
      free_coupler(1);

    exit(EXIT_NOPORT);
  }

  /* Check to make sure we have permission to access the port */
  if( access( serial_port, R_OK|W_OK ) < 0 ) {
    fprintf( stderr, "Error, you don't have +rw permission to access serial port: %s\n", serial_port );

    if( sensor_list.roms != NULL )
      free( sensor_list.roms );

    if( coupler_top != NULL )
      free_coupler(1);

    exit(EXIT_NOPERM);
  }
#endif		/* !OWUSB 	*/

  /* Connect to the MLan network */
#ifndef OWUSB
  if( !owAcquire( 0, serial_port) )
  {
#else
  if( !owAcquire( 0, serial_port, temp ) )
  {
    fprintf( stderr, "USB ERROR: %s\n", temp );
#endif
    
    /* Error connecting, print the error and exit */
    OWERROR_DUMP(stdout);

    if( sensor_list.roms != NULL )
      free( sensor_list.roms );

    if( coupler_top != NULL )
      free_coupler(0);

    exit(EXIT_ERR);
  }


  /* Should we walk the whole LAN and display all devices? */
  if( opts & OPT_WALK )
  {
    Walk1Wire();

    if( sensor_list.roms != NULL )
      free( sensor_list.roms );

    if( coupler_top != NULL )
      free_coupler(0);

#ifndef OWUSB
      owRelease(0);
#else
      owRelease(0, temp );
#endif /* OWUSB */

    exit(EXIT_OK);
  }


  /* ------------------------------------------------------------------*/
  /* Should we initialize the sensors?                                  */
  /* This should store the serial numbers to the .digitemprc file      */
  if( opts & OPT_INIT )
  {
    if( Init1WireLan( &sensor_list ) != 0 )
    {
      if( sensor_list.roms != NULL )
        free( sensor_list.roms );

      if( coupler_top != NULL )
        free_coupler(0);

      /* Close the serial port */
#ifndef OWUSB
      owRelease(0);
#else
      owRelease(0, temp );
      fprintf( stderr, "USB ERROR: %s\n", temp );
#endif /* OWUSB */

      exit(EXIT_ERR);
    }
  }

  
  /* Record the starting time */
  switch (log_type) {
    case 4:
    case 5:   start_time = 0;           /* Relative to 1970-01-01 00:00:00 (Unixtime) */
              break;
    default:  start_time = time(NULL);  /* Relative to now */
              break;
  }

  /* Sample the prescribed number of times, 0=infinity */
  for( x = 0;num_samples==0 || x < num_samples; x++ )
  {
    last_time = time(NULL);
    elapsed_time = last_time - start_time;

    switch( log_type )
    {
      /* For this type of logging we print out the elapsed time at the
         start of the line
       */
      case 2:
      case 3:
      case 4:
      case 5:	sprintf(temp, "%ld", elapsed_time );
                log_string( temp );
		break;
      default:
		break;
    }


    /* Should we read just one sensor? */
    if( opts & OPT_SINGLE )
    {
      read_device( &sensor_list, sensor, 0 );  
    }
  
    /* Should we read all connected sensors? */
    if( opts & OPT_ALL )
    {
      read_all( &sensor_list );
    }

    /* MySQL database stuff */
    if( opts & OPT_DBLOG )
    {
      /* open the mysql database */
      MYSQL *conn;
      conn = mysql_init( NULL );
      if( conn == NULL )
      {
        fprintf(stderr, "mysql_init() failed... probably out of memory\n");
        exit(1);
      }
      
      if( mysql_real_connect( conn,dbhost,dbuser,dbpass,
                              dbname,0,NULL,0) == NULL )
      {
        fprintf( stderr, "mysql_real_connect() failed!\nError: (%u) '%s'\n",
                mysql_errno( conn ), mysql_error( conn ) );
        mysql_close(conn);
        exit(1);
      }
      
      log_type = 6;
      read_all_and_dblog( &sensor_list, conn );
      mysql_close( conn );
    }
      
    switch( log_type )
    {
      /* For this type of logging we print out the elapsed time at the
         start of the line
       */
      case 2:
      case 3:
      case 4:
      case 5:	log_string( "\n" );
		break;
      default:
		break;
    }

    /* Wait until we have passed last_time + sample_delay. We do it
       this way because reading the sensors takes a certain amount
       of time, and sample_delay may be less then the time needed
       to read all the sensors. We should complain about this.
    */
    if( (time(NULL) > last_time + sample_delay) && (sample_delay > 0) )
    {
      fprintf(stderr, "Warning: delay (-d) is less than the time needed to ");
      fprintf(stderr, "read all of the attached sensors. It took %ld seconds", (long int) time(NULL) - last_time );
      fprintf(stderr, " to read the sensors\n" );
    }

    /* Should we delay before the next sample? */
    if( sample_delay > 0 )
    {
      /* Sleep for the remaining time, if there is any */
      if( (time(NULL) - last_time) < sample_delay )
        sleep( sample_delay - (unsigned int) (time(NULL) - last_time) );
    }
  }

  if( sensor_list.roms != NULL )
    free( sensor_list.roms );

  free_coupler(0);

#ifndef OWUSB
  owRelease(0);
#else
  owRelease(0, temp );
#endif /* OWUSB */

  exit(EXIT_OK);
}


unsigned short int Get_2800_Pio(int portnum) {
	unsigned short int pio = -1;

	if(owAccess(portnum)) {
		// read pio command
		owWriteByte(portnum, 0xf5);
		pio=owReadByte(portnum);
	}
    if(owAccess(portnum)) {
		return (pio);
	} else {
		return -1;
	}
}

/* -----------------------------------------------------------------------
   Read the DS28ea00 temperature or PIO by Tomasz R. Surmacz
   (tsurmacz@ict.pwr.wroc.pl)
   ----------------------------------------------------------------------- */
int read_pio_ds28ea00( int sensor_family, int sensor )
{
  unsigned char pio;
  char		temp[1024],
  		    time_format[160];
  time_t	mytime;

  
  if ( (sensor_family == DS28EA00_FAMILY) || (sensor_family == DS2413_FAMILY) )
  {
    pio = Get_2800_Pio(0);

	if ( ((pio ^ (pio>>4)) &0xf) != 0xf) {
	  // upper nibble should be complement of lower nibble
          // sprintf( temp, "Sensor %d Read Error (%02x)\n", sensor, pio );
      fprintf(stderr, "Sensor %d Read Error (%02x)\n", sensor,  pio );
	  return FALSE;
	}


    mytime = time(NULL);
    if( mytime )
    {
      /* Log the temperature */
      switch( log_type )
      {
        /* Multiple Centigrade temps per line */
		case 3:
        case 2:     sprintf( temp, "\t%02x", pio );
                    break;

        default:    
                    sprintf( time_format, "%%b %%d %%H:%%M:%%S Sensor %d PIO: %02x, PIO-A: %s PIO-B: %s", sensor, pio, (pio&0x01)?"ON ":"OFF", (pio&0x04)?"ON ":"OFF" );
                    /* Handle the time format tokens */
                    strftime( temp, 1024, time_format, localtime( &mytime ) );
                    strcat( temp, "\n" );
                    break;
      } /* switch( log_type ) */
    } else {
      sprintf( temp, "Time Error\n" );
    }

    /* Log it to stdout, logfile or both */
    log_string( temp );
  }

  return FALSE;
}
/* sht data crc */
static unsigned crc8_add( unsigned acc, unsigned byte )
{
  int i;
  acc ^= byte;
  for( i = 0; i < 8; i++ ) {
    if( acc & 0x80 ) {
      acc = ( acc << 1 ) ^ 0x31;
    } else {
      acc <<= 1;
    }
  }
  return acc & 0xff;
}
static unsigned char rev8bits( unsigned char v )
{
  unsigned char r = v;
  int s = 7;
  for( v >>= 1; v; v >>= 1 ) {
    r <<= 1;
    r |= v & 1;
    s--;
  }
  r <<= s;
  return r;
}
