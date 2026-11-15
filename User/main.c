#include "main.h"
#include "platform.h"
#include "app.h"

int main(void)
{
    platform_init();
    app_init();
    app_run();


}
