/* 
 gurov was here, use this code, or don't, whatever, I don't care. If you see a giant bug with a billion legs, please let me know so it can be squashed

 Reworked code and removed some bugs, probably added some bugs ;-) Hajo Noerenberg, https://github.com/hn/hx711

*/

#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "gb_common.h"

#define CLOCK_PIN	24
#define DATA_PIN	23
#define SAMPLESMAX	128
#define SPREAD		5	// percent

#define SCK_ON  (GPIO_SET0 = (1 << CLOCK_PIN))
#define SCK_OFF (GPIO_CLR0 = (1 << CLOCK_PIN))
#define DT_R    (GPIO_IN0  & (1 << DATA_PIN))

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
//	INP_GPIO(CLOCK_PIN);
	OUT_GPIO(CLOCK_PIN);
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

	for (i = 0; i < 24; i++) {
		SCK_ON;
		w++;
		w++;
		w++;
		w++;
		if (DT_R > 0) {
			count++;
		}
		SCK_OFF;
		w++;
		w++;
		w++;
		w++;
		count = count << 1;
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
	int i;
	int c;
	int verbose = 0;
	int good = 0;
	int nsamples = 64;
	long avg_raw = 0;
	long avg_clean = 0;
	long filter_low, filter_high;
	long samples[SAMPLESMAX];
	long caloffset = 0;
	long calval = 0;
	long calweight = 0;

	while ((c = getopt(argc, argv, "vs:o:c:w:h")) != -1)
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
				return 1;
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
				return 1;
			}
			break;
		case 'c':
			calval = atol(optarg);
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-v[v]] [-o calibration_offset] [-w calibration_weight] [-c calibration_value]\n",
				argv[0]);
			return 1;
		}

	setHighPri();
	setup_io();
	setup_gpio();
	reset_converter();

	// get the raw samples and average them
	for (i = 0; i < nsamples; i++) {
		reset_converter();
		samples[i] = read_count(verbose > 1);
		avg_raw += samples[i];
	}

	avg_raw = avg_raw / nsamples;

	// filter all values not in +-SPREAD range
	filter_low = avg_raw - labs(avg_raw * SPREAD / 100.0);
	filter_high = avg_raw + labs(avg_raw * SPREAD / 100.0);

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
		exit(255);
	}

	avg_clean = avg_clean / good;

	if (verbose > 0) {
		printf
		    ("raw average: %ld\nfilter average within %.2f percent range (%ld ... %ld): %ld from %d samples\ncalibration offset: %ld, calibration weight: %ld, calibration value: %ld.\n",
		     avg_raw, (float)SPREAD, filter_low, filter_high, avg_clean,
		     good, caloffset, calweight, calval);
	}

	if (calval > 0) {
		printf("%.2f\n",
		       (avg_clean - caloffset) * ((float)calweight / calval));
	} else {
		printf("%ld\n", avg_clean - caloffset);
	}
	unpull_pins();
	restore_io();
	return 0;
}
