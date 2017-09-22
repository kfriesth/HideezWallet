#ifndef _UI_H_
#define _UI_H_

void ui_init(void);
void ui_connected(void);
void ui_disconnected(void);
bool ui_useraction(int code);
bool ui_pincode_enter(char *buffer);
void ui_cancel(void);

void ui_button_event(u8 state);

#endif

