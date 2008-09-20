/**
 * @file colorscheme.h
 * @brief
 */

#ifndef COLORSCHEME_H
#define COLORSCHEME_H

void ColorScheme_registerCommands(void);
GtkMenuItem* create_colours_menu(void);
void ColorScheme_Init(void);
void ColorScheme_Destroy(void);

#endif
