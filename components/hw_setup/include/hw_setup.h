/* Console example — declarations of command registration functions.

   A simple framework for setting up gpios and the like for omar

 */

#pragma once

// Configure the hardware
void omar_setup(void);

// Some console command:
int toggle_blue(int argc, char** argv);
