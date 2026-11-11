#include "Work.h"

void destroyWorks(Work * works, const int count) {
	if (works != NULL) {
		for (int k = 0; k < count; ++k) {
			for (int b = 0; b < works[k].blockCount; ++b) {
				free(works[k].blocks[b].steps);
			}
			free(works[k].blocks);
		free(works[k].excludedDays);
		}
		free(works);
	}
}
