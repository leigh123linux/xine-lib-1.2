#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "goom_tools.h"

#include <stdlib.h>

GoomRandom *goom_random_init(int i) {
	GoomRandom *grandom = (GoomRandom*)malloc(sizeof(GoomRandom));
	(void)i;
	grandom->pos = 1;
	goom_random_update_array(grandom, GOOM_NB_RAND);
	return grandom;
}

void goom_random_free(GoomRandom *grandom) {
	free(grandom);
}

void goom_random_update_array(GoomRandom *grandom, int numberOfValuesToChange) {
	while (numberOfValuesToChange > 0) {
#if RAND_MAX < 0x10000
		grandom->array[grandom->pos++] = ((rand()<<16)+rand()) / 127;
#else
		grandom->array[grandom->pos++] = rand() / 127;
#endif
		numberOfValuesToChange--;
	}
}
