/* 
 gurov was here, use this code, or don't, whatever, I don't care. If you see a giant bug with a billion legs, please let me know so it can be squashed

 Reworked code and removed some bugs, probably added some bugs ;-) Hajo Noerenberg, https://github.com/hn/hx711

*/

#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include "gb_common.h"

#define CLOCK_PIN	17
#define DATA_PIN	4
#define LEDM_PIN	19	// light up LED connected to this pin during measurements, set to 0 if not used
#define LEDW_PIN	26	// light up LED connected to this pin when wheight exceeds limits, set to 0 if not used

#define SAMPLESMAX	128
#define SPREAD		5	// percent

#define SCK_ON  (GPIO_SET0 = (1 << CLOCK_PIN))
#define SCK_OFF (GPIO_CLR0 = (1 << CLOCK_PIN))
#define DT_R    (GPIO_IN0  & (1 << DATA_PIN))

#if LEDM_PIN > 0
#define LEDM_ON  (GPIO_SET0 = (1 << LEDM_PIN))
#define LEDM_OFF (GPIO_CLR0 = (1 << LEDM_PIN))
#endif
#if LEDW_PIN > 0
#define LEDW_ON  (GPIO_SET0 = (1 << LEDW_PIN))
#define LEDW_OFF (GPIO_CLR0 = (1 << LEDW_PIN))
#endif

void setHighPri(void)
{
	struct sched_param sched;

	memset(&sched, 0, sizeof(sched));

	sched.sched_priority = 10;
	if (sched_setscheduler(0, SCHED_FIFO, &sched))
		printf("Warning: Unable to set high priority\n");
}

void setup_gpio()
{
	INP_GPIO(DATA_PIN);
	INP_GPIO(CLOCK_PIN);
	OUT_GPIO(CLOCK_PIN);
#if LEDM_PIN > 0
	INP_GPIO(LEDM_PIN);
	OUT_GPIO(LEDM_PIN);
	LEDM_OFF;
#endif
#if LEDW_PIN > 0
	INP_GPIO(LEDW_PIN);
	OUT_GPIO(LEDW_PIN);
	LEDW_OFF;
#endif
	SCK_OFF;

/*
	GPIO_PULL = 0;
	short_wait();
	GPIO_PULLCLK0 = 1 << DATA_PIN;
	short_wait();
	GPIO_PULL = 0;
	GPIO_PULLCLK0 = 0;
*/
/*
	GPIO_PULL = 2;
	short_wait();
	GPIO_PULLCLK0 = 1 << DATA_PIN;
	short_wait();
	GPIO_PULL = 0;
	GPIO_PULLCLK0 = 0;
*/
}

void reset_converter(void)
{
	SCK_ON;
	usleep(60);
	SCK_OFF;
	usleep(60);
}

/*
void set_gain(int r)
{
	// r = 0 - 128 gain, channel A
	// r = 1 -  32 gain, channel B
	// r = 2 -  63 gain, channel A

	int i;

	while (DT_R) ;

	for (i = 0; i < 24 + r; i++) {
		SCK_ON;
		SCK_OFF;
	}
}
*/

void unpull_pins()
{
	GPIO_PULL = 0;
//      short_wait();
	GPIO_PULLCLK0 = 1 << DATA_PIN;
//      short_wait();
	GPIO_PULL = 0;
	GPIO_PULLCLK0 = 0;
}

long read_count(int verbose)
{
	long count = 0;
	int i;
	int w = 0;

	while (DT_R) ;
	w++;
	w++;
	w++;
	w++;

	// if measurements are shaky, adjust the number of 'w++' waits below (try and error)

	for (i = 0; i < 24; i++) {
		SCK_ON;
		count = count << 1;
		w++;
		w++;
		w++;
		w++;

		SCK_OFF;
		w++;
		w++;
		if (DT_R > 0) {
			count++;
		}
	}

	SCK_ON;
	w++;
	w++;
	w++;
	w++;
	SCK_OFF;
	w++;
	w++;
	w++;
	w++;

	if (count & 0x800000) {
		count |= (long)~0xffffff;
	}

	if (verbose) {
		printf("raw reading: ");
		for (i = 31; i >= 0; i--) {
			if (i == 23) {
				printf(" ");
			}
			printf("%d", (count & (1 << i)) != 0);
		}

		printf(" = %10ld\n", count);
	}

	return count;
}

int main(int argc, char **argv)
{
	int i, j;
	int c;
	int verbose = 0;
	int good;
	int nsamples = 64;
	int pause = 0;
	int rounds = 1;
	long avg_raw;
	long avg_clean;
	long filter_low, filter_high;
	long samples[SAMPLESMAX];
	long caloffset = 0;
	long calval = 0;
	long calweight = 0;
	float result;
	float limit_lower = -1, limit_upper = -1;
	char *logfile = NULL;
	FILE *logfh = NULL;

	while ((c = getopt(argc, argv, "vs:o:c:w:l:u:p:r:f:h")) != -1)
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 's':
			nsamples = atoi(optarg);
			if (nsamples < 1 || nsamples > SAMPLESMAX) {
				fprintf(stderr,
					"Option -%c requires a positive integer with maximum value %d as argument.\n",
					c, SAMPLESMAX);
				return EX_USAGE;
			}
			break;
		case 'o':
			caloffset = atol(optarg);
			break;
		case 'w':
			calweight = atol(optarg);
			if (calweight <= 0) {
				fprintf(stderr,
					"Option -%c requires a positive integer as argument.\n",
					c);
				return EX_USAGE;
			}
			break;
		case 'c':
			calval = atol(optarg);
			break;
		case 'l':
			limit_lower = atof(optarg);
			if (limit_lower < 0) {
				fprintf(stderr,
					"Option -%c requires a positive number as argument.\n",
					c);
				return EX_USAGE;
			}
			break;
		case 'u':
			limit_upper = atof(optarg);
			if (limit_upper <= 0) {
				fprintf(stderr,
					"Option -%c requires a positive number as argument.\n",
					c);
				return EX_USAGE;
			}
			break;
		case 'p':
			pause = atoi(optarg);
			if (pause < 1) {
				fprintf(stderr,
					"Option -%c requires a positive integer as argument.\n",
					c);
				return EX_USAGE;
			}
			break;
		case 'r':
			rounds = atoi(optarg);
			if (rounds < 0) {
				fprintf(stderr,
					"Option -%c requires a non-negative integer as argument.\n",
					c);
				return EX_USAGE;
			}
			break;
		case 'f':
			logfile = optarg;
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-v[v]] [-o calibration_offset] [-w calibration_weight] [-c calibration_value] [-l warn_lower_limit] [-u warn_upper_limit] [-p pause_after_measurement] [-r rounds_to_loop] [-f output_file]\n",
				argv[0]);
			return EX_USAGE;
		}

	if (logfile) {
		logfh = fopen(logfile, "a");
		if (logfh == NULL) {
			fprintf(stderr, "Unable to open output file %s.\n",
				logfile);
			return EX_CANTCREAT;
		}
		setbuf(logfh, NULL);
	}

	setHighPri();
	setup_io();
	setup_gpio();

	for (j = rounds; (rounds == 0) || (j > 0); j--) {

#if LEDM_PIN > 0
		LEDM_ON;
#endif

		avg_raw = 0;

		// get the raw samples and average them
		for (i = 0; i < nsamples; i++) {
			reset_converter();
			samples[i] = read_count(verbose > 1);
			avg_raw += samples[i];
		}

#if LEDM_PIN > 0
		LEDM_OFF;
#endif

		avg_raw = avg_raw / nsamples;

		// filter all values not in +-SPREAD range
		filter_low = avg_raw - labs(avg_raw * SPREAD / 100.0);
		filter_high = avg_raw + labs(avg_raw * SPREAD / 100.0);

		avg_clean = 0;
		good = 0;

		for (i = 0; i < nsamples; i++) {
			if (samples[i] > filter_low && samples[i] < filter_high) {
				avg_clean += samples[i];
				good++;
			}
		}

		if (good == 0) {
			printf
			    ("No data to consider within %.2f percent range of raw average %ld (%ld ... %ld).\n",
			     (float)SPREAD, avg_raw, filter_low, filter_high);
			continue;
		}

		avg_clean = avg_clean / good;

		if (verbose > 0) {
			printf
			    ("raw average: %ld\nfilter average within %.2f percent range (%ld ... %ld): %ld from %d samples\ncalibration offset: %ld, calibration weight: %ld, calibration value: %ld.\n",
			     avg_raw, (float)SPREAD, filter_low, filter_high,
			     avg_clean, good, caloffset, calweight, calval);
		}

		if (calval > 0) {
			result =
			    (float)(avg_clean - caloffset) * calweight / calval;
			printf("%.2f\n", result);
			if (logfh) {
				fprintf(logfh, "%.2f\n", result);
			}
#if LEDW_PIN > 0
			if ((limit_lower >= 0 && result < limit_lower)
			    || (limit_upper >= 0 && result > limit_upper)) {
				LEDW_ON;
			} else {
				LEDW_OFF;
			}
#endif
		} else {
			printf("%ld\n", avg_clean - caloffset);
		}

		if (pause) {
			sleep(pause);
		}
	}

#if LEDW_PIN > 0
	LEDW_OFF;
#endif

	if (logfh) {
		fclose(logfh);
	}
	unpull_pins();
	restore_io();
	return EX_OK;
}
