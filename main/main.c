#include <stdio.h>
#include "it8951.h"
#include "m5paper.h"

void app_main(void)
{
    puts("hello world");

    m5paper_init();
    it8951_init(2300);

}